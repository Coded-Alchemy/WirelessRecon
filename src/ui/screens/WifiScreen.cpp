
/**
 * @file WifiScreen.cpp
 * @brief Implements the paginated Wi-Fi inventory screen.
 *
 * @details
 * DeviceInventory remains authoritative. This screen copies at most four
 * records at a time and does not control scan lifecycle.
 *
 * Only contentContainer_ is scrollable. Header, pagination footer, and
 * global navigation remain fixed.
 */

#include "ui/screens/WifiScreen.h"

#include <cstdio>

#include "ui/theme/UITheme.h"

namespace wirelessrecon::ui::screens {

using namespace wirelessrecon::ui::theme;

namespace {

constexpr int32_t kHeaderHeight = 42;
constexpr int32_t kPaginationHeight = 42;
constexpr int32_t kNavigationHeight = 44;

constexpr int32_t kBackButtonWidth = 72;
constexpr int32_t kPaginationButtonWidth = 76;
constexpr int32_t kPaginationButtonHeight = 36;

constexpr int32_t kNetworkCardHeight = 88;

void applyTransparent(lv_obj_t* object) {
    if (object == nullptr) return;

    lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
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

    // The right padding creates a gutter for the scrollbar.
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

lv_obj_t* makeLabel(
    lv_obj_t* parent,
    const char* text,
    uint32_t color,
    const lv_font_t* font = &lv_font_montserrat_14
) {
    if (parent == nullptr) return nullptr;

    lv_obj_t* label = lv_label_create(parent);
    if (label == nullptr) return nullptr;

    lv_label_set_text(label, text);
    lv_obj_set_width(label, lv_pct(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);

    lv_obj_set_style_text_color(
        label, lv_color_hex(color), LV_PART_MAIN
    );
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);

    return label;
}

bool configurePaginationButton(
    lv_obj_t* button,
    const char* text
) {
    if (button == nullptr) return false;

    lv_obj_set_size(
        button,
        kPaginationButtonWidth,
        kPaginationButtonHeight
    );

    applyPrimaryActionButton(button);

    lv_obj_t* label = lv_label_create(button);
    if (label == nullptr) return false;

    lv_label_set_text(label, text);
    lv_obj_center(label);
    applyButtonLabel(label);

    return true;
}

} // namespace

void WifiScreen::bind(
    core::WirelessCoordinator& coordinator,
    core::DeviceInventory& inventory
) {
    coordinator_ = &coordinator;
    inventory_ = &inventory;
}

void WifiScreen::setBackCallback(
    BackCallback callback,
    void* context
) {
    backCallback_ = callback;
    backContext_ = context;
}

void WifiScreen::setNavigationCallback(
    NavigationCallback callback,
    void* context
) {
    navigationCallback_ = callback;
    navigationContext_ = context;
}

bool WifiScreen::create(lv_obj_t* parent) {
    if (parent == nullptr ||
        inventory_ == nullptr ||
        coordinator_ == nullptr) {
        return false;
    }

    root_ = lv_obj_create(parent);
    if (root_ == nullptr) return false;

    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    applyBackground(root_);

    lv_obj_set_style_pad_all(
        root_, Spacing::screenMargin, LV_PART_MAIN
    );

    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        root_,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START
    );
    lv_obj_set_style_pad_row(
        root_, Spacing::compact, LV_PART_MAIN
    );

    makeFixedContainer(root_);

    // --------------------------------------------------------
    // Fixed header
    // --------------------------------------------------------

    headerContainer_ = lv_obj_create(root_);
    if (headerContainer_ == nullptr) return false;

    lv_obj_set_width(headerContainer_, lv_pct(100));
    lv_obj_set_height(headerContainer_, kHeaderHeight);
    applyTransparent(headerContainer_);
    makeFixedContainer(headerContainer_);

    lv_obj_t* back = lv_btn_create(headerContainer_);
    if (back == nullptr) return false;

    lv_obj_set_size(
        back, kBackButtonWidth, kPaginationButtonHeight
    );
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);

    applyNavigationButton(back);

    lv_obj_add_event_cb(
        back,
        &WifiScreen::backEventCallback,
        LV_EVENT_CLICKED,
        this
    );

    lv_obj_t* backText = lv_label_create(back);
    if (backText == nullptr) return false;

    lv_label_set_text(backText, "Back");
    lv_obj_center(backText);
    applyButtonLabel(backText);

    lv_obj_t* title = lv_label_create(headerContainer_);
    if (title == nullptr) return false;

    lv_label_set_text(title, "WI-FI NETWORKS");
    lv_obj_align(title, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_set_style_text_color(
        title, lv_color_hex(Colors::primary), LV_PART_MAIN
    );
    lv_obj_set_style_text_font(
        title, &lv_font_montserrat_14, LV_PART_MAIN
    );

    // --------------------------------------------------------
    // Scrollable middle content
    // --------------------------------------------------------

    contentContainer_ = lv_obj_create(root_);
    if (contentContainer_ == nullptr) return false;

    lv_obj_set_width(contentContainer_, lv_pct(100));
    applyTransparent(contentContainer_);

    lv_obj_set_flex_flow(
        contentContainer_, LV_FLEX_FLOW_COLUMN
    );
    lv_obj_set_flex_align(
        contentContainer_,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START
    );

    lv_obj_set_style_pad_row(
        contentContainer_, Spacing::standard, LV_PART_MAIN
    );

    makeScrollableContent(contentContainer_);

    statusLabel_ = makeLabel(
        contentContainer_,
        "Loading Wi-Fi inventory...",
        Colors::textSecondary
    );
    if (statusLabel_ == nullptr) return false;

    // --------------------------------------------------------
    // Non-scrollable list container
    // The parent content container handles vertical scrolling.
    // --------------------------------------------------------

    listContainer_ = lv_obj_create(contentContainer_);
    if (listContainer_ == nullptr) return false;

    lv_obj_set_width(listContainer_, lv_pct(100));
    lv_obj_set_height(listContainer_, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(listContainer_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        listContainer_,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START
    );

    lv_obj_set_style_pad_all(listContainer_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(
        listContainer_, Spacing::networkCardGap, LV_PART_MAIN
    );

    applyTransparent(listContainer_);
    makeFixedContainer(listContainer_);

    // --------------------------------------------------------
    // Fixed pagination footer
    // --------------------------------------------------------

    controlsContainer_ = lv_obj_create(root_);
    if (controlsContainer_ == nullptr) return false;

    lv_obj_set_width(controlsContainer_, lv_pct(100));
    lv_obj_set_height(controlsContainer_, kPaginationHeight);
    applyTransparent(controlsContainer_);
    makeFixedContainer(controlsContainer_);

    previousButton_ = lv_btn_create(controlsContainer_);
    if (!configurePaginationButton(previousButton_, "Previous")) {
        return false;
    }

    lv_obj_align(previousButton_, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(
        previousButton_,
        &WifiScreen::previousEventCallback,
        LV_EVENT_CLICKED,
        this
    );

    nextButton_ = lv_btn_create(controlsContainer_);
    if (!configurePaginationButton(nextButton_, "Next")) {
        return false;
    }

    lv_obj_align(nextButton_, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(
        nextButton_,
        &WifiScreen::nextEventCallback,
        LV_EVENT_CLICKED,
        this
    );

    pageLabel_ = lv_label_create(controlsContainer_);
    if (pageLabel_ == nullptr) return false;

    lv_obj_set_width(pageLabel_, 60);
    lv_label_set_long_mode(pageLabel_, LV_LABEL_LONG_CLIP);

    lv_obj_set_style_text_align(
        pageLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN
    );
    lv_obj_set_style_text_color(
        pageLabel_, lv_color_hex(Colors::textSecondary), LV_PART_MAIN
    );
    lv_obj_set_style_text_font(
        pageLabel_, &lv_font_montserrat_14, LV_PART_MAIN
    );

    lv_obj_align(pageLabel_, LV_ALIGN_CENTER, 0, 0);

    // --------------------------------------------------------
    // Fixed global navigation
    // --------------------------------------------------------

    navigationContainer_ = lv_obj_create(root_);
    if (navigationContainer_ == nullptr) return false;

    lv_obj_set_width(navigationContainer_, lv_pct(100));
    lv_obj_set_height(navigationContainer_, kNavigationHeight);
    applyTransparent(navigationContainer_);
    makeFixedContainer(navigationContainer_);

    if (!createNavigationBar()) return false;

    reloadInventory();
    renderPage();
    refresh();

    return true;
}

bool WifiScreen::createNavigationBar() {
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
        {"HOME", NavigationDestination::Dashboard, false, true},
        {"WI-FI", NavigationDestination::Wifi, true, true},
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
                &WifiScreen::navigationEventCallback,
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

void WifiScreen::refresh() {
    if (inventory_ == nullptr) return;

    const std::uint32_t currentGeneration =
        inventory_->generation();

    if (currentGeneration != renderedGeneration_) {
        reloadInventory();
        renderPage();
    }

    if (statusLabel_ != nullptr && coordinator_ != nullptr) {
        const bool scanning =
            coordinator_->scanInProgress(core::RadioType::Wifi);

        const char* status = nullptr;

        if (scanning) {
            status = "Wi-Fi scan running";
        } else if (deviceCount_ == 0) {
            status = "No Wi-Fi networks discovered";
        } else {
            status = "Wi-Fi inventory";
        }

        lv_label_set_text(statusLabel_, status);
    }
}

void WifiScreen::reloadInventory() {
    if (inventory_ == nullptr) return;

    deviceCount_ = inventory_->count(core::RadioType::Wifi);

    const std::size_t pageCount =
        deviceCount_ == 0
            ? 1
            : (deviceCount_ + kItemsPerPage - 1) / kItemsPerPage;

    if (pageIndex_ >= pageCount) {
        pageIndex_ = pageCount - 1;
    }

    const std::size_t offset = pageIndex_ * kItemsPerPage;

    pageItemCount_ = inventory_->copyRadioPage(
        core::RadioType::Wifi,
        offset,
        pageDevices_.data(),
        pageDevices_.size()
    );

    renderedGeneration_ = inventory_->generation();
}

void WifiScreen::renderPage() {
    if (listContainer_ == nullptr) return;

    lv_obj_clean(listContainer_);

    if (pageItemCount_ == 0) {
        lv_obj_t* empty = lv_obj_create(listContainer_);

        if (empty != nullptr) {
            lv_obj_set_width(empty, lv_pct(100));
            lv_obj_set_height(empty, 72);
            applyPanel(empty);

            lv_obj_set_style_pad_all(
                empty, Spacing::standard, LV_PART_MAIN
            );

            makeLabel(
                empty,
                "No networks found yet. Start a Wi-Fi scan from the dashboard.",
                Colors::textSecondary
            );
        }

        updatePageControls();

        if (contentContainer_ != nullptr) {
            lv_obj_scroll_to_y(
                contentContainer_, 0, LV_ANIM_OFF
            );
        }

        return;
    }

    for (std::size_t i = 0; i < pageItemCount_; ++i) {
        const core::Device& device = pageDevices_[i];

        lv_obj_t* card = lv_obj_create(listContainer_);
        if (card == nullptr) continue;

        lv_obj_set_width(card, lv_pct(100));
        lv_obj_set_height(card, kNetworkCardHeight);
        applyPanel(card);

        // More room between the card edge and its text.
        lv_obj_set_style_pad_left(
            card, Spacing::networkCardHorizontal, LV_PART_MAIN
        );
        lv_obj_set_style_pad_right(
            card, Spacing::networkCardHorizontal, LV_PART_MAIN
        );
        lv_obj_set_style_pad_top(
            card, Spacing::networkCardVertical, LV_PART_MAIN
        );
        lv_obj_set_style_pad_bottom(
            card, Spacing::networkCardVertical, LV_PART_MAIN
        );

        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(
            card,
            LV_FLEX_ALIGN_START,
            LV_FLEX_ALIGN_START,
            LV_FLEX_ALIGN_START
        );

        lv_obj_set_style_pad_row(
            card, Spacing::networkCardLineGap, LV_PART_MAIN
        );

        const char* ssid = nullptr;

        if (!device.wifi.ssid.empty()) {
            ssid = device.wifi.ssid.c_str();
        } else if (device.wifi.hidden) {
            ssid = "<hidden network>";
        } else {
            ssid = "<unnamed network>";
        }

        char identity[80];

        std::snprintf(
            identity,
            sizeof(identity),
            "BSSID: %s",
            device.address.empty()
                ? "unavailable"
                : device.address.c_str()
        );

        makeLabel(card, ssid, Colors::primary);
        makeLabel(card, identity, Colors::textSecondary);

        // encryption is platform-native. Do not infer a security type.
        char details[96];

        if (device.hasRssi) {
            std::snprintf(
                details,
                sizeof(details),
                "%d dBm | CH %d | ENC %u | %s",
                static_cast<int>(device.rssi),
                static_cast<int>(device.wifi.channel),
                static_cast<unsigned>(device.wifi.encryption),
                device.currentlyVisible ? "VISIBLE" : "NOT SEEN"
            );
        } else {
            std::snprintf(
                details,
                sizeof(details),
                "RSSI n/a | CH %d | ENC %u | %s",
                static_cast<int>(device.wifi.channel),
                static_cast<unsigned>(device.wifi.encryption),
                device.currentlyVisible ? "VISIBLE" : "NOT SEEN"
            );
        }

        makeLabel(card, details, Colors::textSecondary);
    }

    updatePageControls();

    if (contentContainer_ != nullptr) {
        lv_obj_scroll_to_y(
            contentContainer_, 0, LV_ANIM_OFF
        );
    }
}

void WifiScreen::updatePageControls() {
    const std::size_t pageCount =
        deviceCount_ == 0
            ? 1
            : (deviceCount_ + kItemsPerPage - 1) / kItemsPerPage;

    if (pageLabel_ != nullptr) {
        char text[32];

        std::snprintf(
            text,
            sizeof(text),
            "%u / %u",
            static_cast<unsigned>(pageIndex_ + 1),
            static_cast<unsigned>(pageCount)
        );

        lv_label_set_text(pageLabel_, text);
        lv_obj_align(pageLabel_, LV_ALIGN_CENTER, 0, 0);
    }

    if (previousButton_ != nullptr) {
        if (pageIndex_ == 0) {
            lv_obj_add_state(previousButton_, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(previousButton_, LV_STATE_DISABLED);
        }
    }

    if (nextButton_ != nullptr) {
        if (pageIndex_ + 1 >= pageCount) {
            lv_obj_add_state(nextButton_, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(nextButton_, LV_STATE_DISABLED);
        }
    }
}

void WifiScreen::changePage(int direction) {
    const std::size_t pageCount =
        deviceCount_ == 0
            ? 1
            : (deviceCount_ + kItemsPerPage - 1) / kItemsPerPage;

    if (direction < 0 && pageIndex_ > 0) {
        --pageIndex_;
    } else if (direction > 0 && pageIndex_ + 1 < pageCount) {
        ++pageIndex_;
    } else {
        return;
    }

    reloadInventory();
    renderPage();
}

void WifiScreen::goBack() {
    if (backCallback_ != nullptr) {
        backCallback_(backContext_);
    }
}

void WifiScreen::backEventCallback(lv_event_t* event) {
    if (event == nullptr) return;

    auto* self = static_cast<WifiScreen*>(
        lv_event_get_user_data(event)
    );

    if (self != nullptr) self->goBack();
}

void WifiScreen::previousEventCallback(lv_event_t* event) {
    if (event == nullptr) return;

    auto* self = static_cast<WifiScreen*>(
        lv_event_get_user_data(event)
    );

    if (self != nullptr) self->changePage(-1);
}

void WifiScreen::nextEventCallback(lv_event_t* event) {
    if (event == nullptr) return;

    auto* self = static_cast<WifiScreen*>(
        lv_event_get_user_data(event)
    );

    if (self != nullptr) self->changePage(1);
}

void WifiScreen::navigationEventCallback(lv_event_t* event) {
    if (event == nullptr) return;

    auto* self = static_cast<WifiScreen*>(
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

} // namespace wirelessrecon::ui::screens
