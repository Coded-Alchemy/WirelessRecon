
/**
 * @file DashboardScreen.cpp
 * @brief Implements the WirelessRecon operational dashboard.
 *
 * @details
 * The root is non-scrollable and owns three vertical regions:
 * - Fixed header.
 * - Flex-growing scrollable content.
 * - Fixed bottom navigation.
 *
 * Radio scan requests continue to be submitted through WirelessCoordinator.
 */

#include "ui/screens/DashboardScreen.h"

#include <cstdint>
#include <cstdio>

#include "ui/theme/UITheme.h"

namespace wirelessrecon::ui::screens {

using namespace wirelessrecon::ui::theme;

namespace {

constexpr int32_t kHeaderHeight = 48;
constexpr int32_t kMetricCardHeight = 82;
constexpr int32_t kScanButtonHeight = 48;
constexpr int32_t kNavigationHeight = 44;

constexpr int32_t kButtonVerticalPadding = 6;
constexpr int32_t kButtonHorizontalPadding = 4;

void applyTransparentContainer(lv_obj_t* object) {
    if (object == nullptr) return;

    lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
}

void applyVerticalLayout(lv_obj_t* object, int32_t spacing) {
    lv_obj_set_flex_flow(object, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        object,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START
    );
    lv_obj_set_style_pad_row(object, spacing, LV_PART_MAIN);
}

void makeFixedContainer(lv_obj_t* object) {
    if (object == nullptr) return;

    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(object, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_grow(object, 0);
}

void makeScrollableContent(lv_obj_t* object) {
    if (object == nullptr) return;

    lv_obj_set_height(object, 0);
    lv_obj_set_flex_grow(object, 1);

    lv_obj_add_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(object, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(object, LV_SCROLLBAR_MODE_AUTO);

    // Reserve a right-side gutter so the scrollbar is not drawn over
    // labels or cards.
    lv_obj_set_style_pad_right(
        object, Spacing::scrollGutter, LV_PART_MAIN
    );

    lv_obj_set_style_width(object, 3, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(
        object, lv_color_hex(Colors::textMuted), LV_PART_SCROLLBAR
    );
    lv_obj_set_style_bg_opa(
        object, LV_OPA_50, LV_PART_SCROLLBAR
    );
    lv_obj_set_style_radius(
        object, 2, LV_PART_SCROLLBAR
    );
}

lv_obj_t* createSectionHeading(
    lv_obj_t* parent,
    const char* text
) {
    if (parent == nullptr) return nullptr;

    lv_obj_t* label = lv_label_create(parent);
    if (label == nullptr) return nullptr;

    lv_label_set_text(label, text);
    lv_obj_set_width(label, lv_pct(100));

    lv_obj_set_style_text_color(
        label, lv_color_hex(Colors::textSecondary), LV_PART_MAIN
    );
    lv_obj_set_style_text_font(
        label, &lv_font_montserrat_14, LV_PART_MAIN
    );

    return label;
}

lv_obj_t* createButtonLabel(
    lv_obj_t* button,
    const char* text,
    bool enabled = true
) {
    if (button == nullptr) return nullptr;

    lv_obj_t* label = lv_label_create(button);
    if (label == nullptr) return nullptr;

    lv_label_set_text(label, text);
    lv_obj_center(label);
    applyButtonLabel(label, enabled);

    return label;
}

} // namespace

void DashboardScreen::bind(
    core::WirelessCoordinator& coordinator,
    core::DeviceInventory& inventory
) {
    coordinator_ = &coordinator;
    inventory_ = &inventory;
}

void DashboardScreen::setNavigationCallback(
    NavigationCallback callback,
    void* context
) {
    navigationCallback_ = callback;
    navigationContext_ = context;
}

bool DashboardScreen::create(lv_obj_t* parent) {
    if (parent == nullptr ||
        coordinator_ == nullptr ||
        inventory_ == nullptr) {
        return false;
    }

    root_ = lv_obj_create(parent);
    if (root_ == nullptr) return false;

    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    applyBackground(root_);

    lv_obj_set_style_pad_all(
        root_, Spacing::screenMargin, LV_PART_MAIN
    );

    applyVerticalLayout(root_, Spacing::standard);
    makeFixedContainer(root_);

    wifiCountLabel_ = nullptr;
    bleCountLabel_ = nullptr;
    classicCountLabel_ = nullptr;
    scanStatusLabel_ = nullptr;
    scanWasActive_ = false;

    // --------------------------------------------------------
    // Fixed header
    // --------------------------------------------------------

    headerContainer_ = lv_obj_create(root_);
    if (headerContainer_ == nullptr) return false;

    lv_obj_set_width(headerContainer_, lv_pct(100));
    lv_obj_set_height(headerContainer_, kHeaderHeight);

    applyTransparentContainer(headerContainer_);
    applyVerticalLayout(headerContainer_, 0);
    makeFixedContainer(headerContainer_);

    lv_obj_t* applicationName = lv_label_create(headerContainer_);
    if (applicationName == nullptr) return false;

    lv_label_set_text(applicationName, "WIRELESSRECON");
    lv_obj_set_style_text_color(
        applicationName, lv_color_hex(Colors::primary), LV_PART_MAIN
    );
    lv_obj_set_style_text_font(
        applicationName, &lv_font_montserrat_14, LV_PART_MAIN
    );
    lv_obj_set_style_pad_left(applicationName, 0, LV_PART_MAIN);

    lv_obj_t* headerSubtitle = lv_label_create(headerContainer_);
    if (headerSubtitle == nullptr) return false;

    lv_label_set_text(headerSubtitle, "WIRELESS MONITORING");
    lv_obj_set_style_text_color(
        headerSubtitle, lv_color_hex(Colors::textMuted), LV_PART_MAIN
    );
    lv_obj_set_style_text_font(
        headerSubtitle, &lv_font_montserrat_14, LV_PART_MAIN
    );
    lv_obj_set_style_pad_left(headerSubtitle, 0, LV_PART_MAIN);

    // --------------------------------------------------------
    // Scrollable content
    // --------------------------------------------------------

    contentContainer_ = lv_obj_create(root_);
    if (contentContainer_ == nullptr) return false;

    lv_obj_set_width(contentContainer_, lv_pct(100));
    applyTransparentContainer(contentContainer_);
    applyVerticalLayout(contentContainer_, Spacing::section);
    makeScrollableContent(contentContainer_);

    if (createSectionHeading(
            contentContainer_, "RADIO INVENTORY"
        ) == nullptr) {
        return false;
    }

    lv_obj_t* metricsPanel = lv_obj_create(contentContainer_);
    if (metricsPanel == nullptr) return false;

    lv_obj_set_width(metricsPanel, lv_pct(100));
    lv_obj_set_height(metricsPanel, kMetricCardHeight + 8);
    applyTransparentContainer(metricsPanel);

    lv_obj_set_flex_flow(metricsPanel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        metricsPanel,
        LV_FLEX_ALIGN_SPACE_BETWEEN,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );
    lv_obj_set_style_pad_column(
        metricsPanel, Spacing::compact, LV_PART_MAIN
    );

    wifiCountLabel_ = createMetricCard(metricsPanel, "WI-FI", "0");
    bleCountLabel_ = createMetricCard(metricsPanel, "BLE", "0");
    classicCountLabel_ = createMetricCard(metricsPanel, "CLASSIC", "0");

    if (wifiCountLabel_ == nullptr ||
        bleCountLabel_ == nullptr ||
        classicCountLabel_ == nullptr) {
        return false;
    }

    // --------------------------------------------------------
    // Scan status remains above the scan buttons.
    // --------------------------------------------------------

    lv_obj_t* statusPanel = lv_obj_create(contentContainer_);
    if (statusPanel == nullptr) return false;

    lv_obj_set_width(statusPanel, lv_pct(100));
    lv_obj_set_height(statusPanel, LV_SIZE_CONTENT);
    applyPanel(statusPanel);

    lv_obj_set_style_pad_all(
        statusPanel, Spacing::standard, LV_PART_MAIN
    );
    applyVerticalLayout(statusPanel, Spacing::compact);

    if (createSectionHeading(statusPanel, "SCAN STATUS") == nullptr) {
        return false;
    }

    scanStatusLabel_ = lv_label_create(statusPanel);
    if (scanStatusLabel_ == nullptr) return false;

    lv_label_set_text(scanStatusLabel_, "Ready to scan");
    lv_obj_set_width(scanStatusLabel_, lv_pct(100));
    lv_label_set_long_mode(scanStatusLabel_, LV_LABEL_LONG_WRAP);

    lv_obj_set_style_text_color(
        scanStatusLabel_, lv_color_hex(Colors::textPrimary), LV_PART_MAIN
    );
    lv_obj_set_style_text_font(
        scanStatusLabel_, &lv_font_montserrat_14, LV_PART_MAIN
    );

    if (createSectionHeading(
            contentContainer_, "START SCAN"
        ) == nullptr) {
        return false;
    }

    lv_obj_t* controlsPanel = lv_obj_create(contentContainer_);
    if (controlsPanel == nullptr) return false;

    lv_obj_set_width(controlsPanel, lv_pct(100));
    applyTransparentContainer(controlsPanel);
    applyVerticalLayout(controlsPanel, Spacing::standard);

    lv_obj_set_style_pad_top(
        controlsPanel, kButtonVerticalPadding, LV_PART_MAIN
    );
    lv_obj_set_style_pad_bottom(
        controlsPanel, kButtonVerticalPadding, LV_PART_MAIN
    );
    lv_obj_set_style_pad_left(
        controlsPanel, kButtonHorizontalPadding, LV_PART_MAIN
    );
    lv_obj_set_style_pad_right(
        controlsPanel, kButtonHorizontalPadding, LV_PART_MAIN
    );

    lv_obj_set_height(
        controlsPanel,
        (2 * kScanButtonHeight) +
        Spacing::standard +
        (2 * kButtonVerticalPadding)
    );

    if (createScanButton(
            controlsPanel, "Scan Wi-Fi", core::RadioType::Wifi
        ) == nullptr) {
        return false;
    }

    if (createScanButton(
            controlsPanel, "Scan BLE", core::RadioType::Ble
        ) == nullptr) {
        return false;
    }

    // --------------------------------------------------------
    // Fixed bottom navigation
    // --------------------------------------------------------

    navigationContainer_ = lv_obj_create(root_);
    if (navigationContainer_ == nullptr) return false;

    lv_obj_set_width(navigationContainer_, lv_pct(100));
    lv_obj_set_height(navigationContainer_, kNavigationHeight);
    applyTransparentContainer(navigationContainer_);
    makeFixedContainer(navigationContainer_);

    if (!createNavigationBar()) return false;

    refresh();
    return true;
}

bool DashboardScreen::createNavigationBar() {
    if (navigationContainer_ == nullptr) return false;

    lv_obj_set_flex_flow(navigationContainer_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        navigationContainer_,
        LV_FLEX_ALIGN_SPACE_BETWEEN,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );
    lv_obj_set_style_pad_column(
        navigationContainer_, 3, LV_PART_MAIN
    );

    struct Item {
        const char* label;
        NavigationDestination destination;
        bool selected;
        bool enabled;
    };

    static constexpr Item items[] = {
        {"HOME", NavigationDestination::Dashboard, true, true},
        {"WI-FI", NavigationDestination::Wifi, false, true},
        {"BLE", NavigationDestination::Bluetooth, false, false},
        {"SET", NavigationDestination::Settings, false, false}
    };

    for (const Item& item : items) {
        lv_obj_t* button = lv_btn_create(navigationContainer_);
        if (button == nullptr) return false;

        lv_obj_set_width(button, lv_pct(24));
        lv_obj_set_height(button, kNavigationHeight - 4);

        applyBottomNavigationButton(
            button, item.selected, item.enabled
        );

        lv_obj_set_user_data(
            button,
            reinterpret_cast<void*>(
                static_cast<uintptr_t>(item.destination)
            )
        );

        if (item.enabled) {
            lv_obj_add_event_cb(
                button,
                &DashboardScreen::navigationEventCallback,
                LV_EVENT_CLICKED,
                this
            );
        }

        lv_obj_t* label = lv_label_create(button);
        if (label == nullptr) return false;

        lv_label_set_text(label, item.label);
        lv_obj_center(label);
        applyButtonLabel(label, item.enabled);
    }

    return true;
}

lv_obj_t* DashboardScreen::createMetricCard(
    lv_obj_t* parent,
    const char* title,
    const char* initialValue
) {
    if (parent == nullptr) return nullptr;

    lv_obj_t* card = lv_obj_create(parent);
    if (card == nullptr) return nullptr;

    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_height(card, kMetricCardHeight);
    applyPanel(card);

    lv_obj_set_style_pad_all(
        card, Spacing::compact, LV_PART_MAIN
    );
    applyVerticalLayout(card, Spacing::compact);

    lv_obj_set_flex_align(
        card,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    lv_obj_t* titleLabel = lv_label_create(card);
    if (titleLabel == nullptr) return nullptr;

    lv_label_set_text(titleLabel, title);
    lv_obj_set_style_text_color(
        titleLabel, lv_color_hex(Colors::textSecondary), LV_PART_MAIN
    );
    lv_obj_set_style_text_font(
        titleLabel, &lv_font_montserrat_14, LV_PART_MAIN
    );

    lv_obj_t* valueLabel = lv_label_create(card);
    if (valueLabel == nullptr) return nullptr;

    lv_label_set_text(valueLabel, initialValue);
    lv_obj_set_style_text_color(
        valueLabel, lv_color_hex(Colors::primary), LV_PART_MAIN
    );
    lv_obj_set_style_text_font(
        valueLabel, &lv_font_montserrat_14, LV_PART_MAIN
    );

    return valueLabel;
}

lv_obj_t* DashboardScreen::createScanButton(
    lv_obj_t* parent,
    const char* text,
    core::RadioType radio
) {
    if (parent == nullptr) return nullptr;

    lv_obj_t* button = lv_btn_create(parent);
    if (button == nullptr) return nullptr;

    lv_obj_set_width(button, lv_pct(100));
    lv_obj_set_height(button, kScanButtonHeight);

    applyPrimaryActionButton(button);

    lv_obj_set_user_data(
        button,
        reinterpret_cast<void*>(
            static_cast<uintptr_t>(radio)
        )
    );

    lv_obj_add_event_cb(
        button,
        &DashboardScreen::scanButtonEventCallback,
        LV_EVENT_CLICKED,
        this
    );

    lv_obj_t* label = lv_label_create(button);
    if (label == nullptr) return nullptr;

    lv_label_set_text(label, text);
    lv_obj_center(label);
    applyButtonLabel(label);

    return button;
}

void DashboardScreen::scanButtonEventCallback(lv_event_t* event) {
    if (event == nullptr) return;

    auto* self = static_cast<DashboardScreen*>(
        lv_event_get_user_data(event)
    );
    auto* button = static_cast<lv_obj_t*>(
        lv_event_get_target(event)
    );

    if (self == nullptr || button == nullptr) return;

    const uintptr_t value = reinterpret_cast<uintptr_t>(
        lv_obj_get_user_data(button)
    );

    self->requestScan(static_cast<core::RadioType>(value));
}

void DashboardScreen::navigationEventCallback(lv_event_t* event) {
    if (event == nullptr) return;

    auto* self = static_cast<DashboardScreen*>(
        lv_event_get_user_data(event)
    );
    auto* button = static_cast<lv_obj_t*>(
        lv_event_get_target(event)
    );

    if (self == nullptr ||
        button == nullptr ||
        self->navigationCallback_ == nullptr) {
        return;
    }

    const auto destination =
        static_cast<NavigationDestination>(
            reinterpret_cast<uintptr_t>(
                lv_obj_get_user_data(button)
            )
        );

    self->navigationCallback_(
        self->navigationContext_,
        destination
    );
}

void DashboardScreen::requestScan(core::RadioType radio) {
    if (coordinator_ == nullptr) {
        setStatus("Scanner service unavailable");
        return;
    }

    const core::ScanRequestResult result =
        coordinator_->requestScan(radio);

    setStatus(scanResultMessage(result));

    if (result == core::ScanRequestResult::Started) {
        scanWasActive_ = false;
    }

    refresh();
}

const char* DashboardScreen::scanResultMessage(
    core::ScanRequestResult result
) {
    switch (result) {
        case core::ScanRequestResult::Started:
            return "Scan requested";
        case core::ScanRequestResult::UnsupportedRadio:
            return "Radio not supported";
        case core::ScanRequestResult::ScannerUnavailable:
            return "Scanner unavailable";
        case core::ScanRequestResult::ScannerBusy:
            return "Scanner is busy";
        case core::ScanRequestResult::RadioConflict:
            return "Bluetooth controller busy";
        case core::ScanRequestResult::StartFailed:
            return "Unable to start scan";
        default:
            return "Unknown scan result";
    }
}

void DashboardScreen::setStatus(const char* message) {
    if (scanStatusLabel_ != nullptr && message != nullptr) {
        lv_label_set_text(scanStatusLabel_, message);
    }
}

void DashboardScreen::refresh() {
    if (inventory_ == nullptr) return;

    const size_t wifi = inventory_->count(core::RadioType::Wifi);
    const size_t ble = inventory_->count(core::RadioType::Ble);
    const size_t classic =
        inventory_->count(core::RadioType::BluetoothClassic);

    char value[16];

    auto updateCount = [&value](lv_obj_t* label, size_t count) {
        if (label == nullptr) return;

        std::snprintf(
            value, sizeof(value), "%u",
            static_cast<unsigned>(count)
        );
        lv_label_set_text(label, value);
    };

    updateCount(wifiCountLabel_, wifi);
    updateCount(bleCountLabel_, ble);
    updateCount(classicCountLabel_, classic);

    if (coordinator_ == nullptr || scanStatusLabel_ == nullptr) return;

    const bool wifiScanning =
        coordinator_->scanInProgress(core::RadioType::Wifi);
    const bool bleScanning =
        coordinator_->scanInProgress(core::RadioType::Ble);

    const bool scanActive = wifiScanning || bleScanning;

    if (wifiScanning && bleScanning) {
        setStatus("Wi-Fi and BLE scanning");
    } else if (wifiScanning) {
        setStatus("Wi-Fi scan running");
    } else if (bleScanning) {
        setStatus("BLE scan running");
    } else if (scanWasActive_) {
        setStatus("Scan finished");
    }

    scanWasActive_ = scanActive;
}

} // namespace wirelessrecon::ui::screens
