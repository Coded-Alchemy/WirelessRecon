/**
 * @file WifiScreen.cpp
 * @brief Paginated Wi-Fi inventory view and navigation controls.
 *
 * @details
 * Encryption is deliberately displayed as a raw adapter value because
 * Device::wifi.encryption stores a platform-native enum, not a portable enum.
 *
 * DeviceInventory remains the authoritative record owner. This screen only
 * keeps a bounded snapshot for predictable LVGL rendering.
 */

#include "ui/screens/WifiScreen.h"

#include <cstdio>

#include "ui/theme/UITheme.h"

namespace wirelessrecon::ui::screens {

using namespace wirelessrecon::ui::theme;

namespace {

constexpr int32_t kHeaderHeight = 42;
constexpr int32_t kBackButtonWidth = 72;
constexpr int32_t kBackButtonHeight = 36;
constexpr int32_t kPaginationHeight = 42;
constexpr int32_t kNavigationHeight = 42;
constexpr int32_t kPaginationButtonWidth = 92;
constexpr int32_t kPaginationButtonHeight = 36;
constexpr int32_t kNetworkCardHeight = 88;
constexpr int32_t kScrollbarGutter = 5;
constexpr int32_t kScrollbarWidth = 3;
constexpr int32_t kCardHorizontalPadding = 10;
constexpr int32_t kCardVerticalPadding = 4;
constexpr int32_t kCardSpacing = 7;

/**
 * @brief Clears default container decoration and padding.
 */
void applyTransparent(lv_obj_t* object) {
    if (object == nullptr) {
        return;
    }

    lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
}

/**
 * @brief Creates a Wi-Fi card label with consistent typography.
 *
 * @param parent Parent card.
 * @param text Label content.
 * @param color Theme color token.
 * @param font Font used by the label.
 * @return Label object or nullptr.
 */
lv_obj_t* makeLabel(
    lv_obj_t* parent,
    const char* text,
    uint32_t color,
    const lv_font_t* font = &lv_font_montserrat_14
) {
    if (parent == nullptr) {
        return nullptr;
    }

    lv_obj_t* label = lv_label_create(parent);
    if (label == nullptr) {
        return nullptr;
    }

    lv_label_set_text(label, text);
    lv_obj_set_width(label, lv_pct(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(
        label,
        lv_color_hex(color),
        LV_PART_MAIN
    );
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);

    return label;
}

/**
 * @brief Creates a primary-action button for Wi-Fi pagination.
 *
 * @details
 * Previous and Next intentionally share the existing Dashboard scan-action
 * appearance. They do not use the neutral Back/bottom-navigation appearance.
 */
void applyPaginationButtonStyle(lv_obj_t* button) {
    applyPrimaryActionButton(button);
}

/**
 * @brief Configures the narrow, subdued scrollbar.
 *
 * The content container reserves a small right-side gutter so the scrollbar
 * does not consume a large portion of the available card width.
 */
void configureContentScrollbar(lv_obj_t* object) {
    if (object == nullptr) {
        return;
    }

    lv_obj_set_scrollbar_mode(object, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(object, LV_DIR_VER);

    lv_obj_set_style_width(
        object,
        kScrollbarWidth,
        LV_PART_SCROLLBAR
    );

    lv_obj_set_style_bg_color(
        object,
        lv_color_hex(Colors::textMuted),
        LV_PART_SCROLLBAR
    );

    lv_obj_set_style_bg_opa(
        object,
        LV_OPA_50,
        LV_PART_SCROLLBAR
    );

    lv_obj_set_style_radius(
        object,
        2,
        LV_PART_SCROLLBAR
    );
}

} // namespace

/**
 * @brief Stores borrowed references to the scan coordinator and inventory.
 *
 * @param coordinator Supplies read-only scan lifecycle state.
 * @param inventory Supplies the authoritative Wi-Fi records.
 */
void WifiScreen::bind(
    core::WirelessCoordinator& coordinator,
    core::DeviceInventory& inventory
) {
    coordinator_ = &coordinator;
    inventory_ = &inventory;
}

/**
 * @brief Registers the owner callback used by the Back button.
 *
 * @param callback Called synchronously from the UI task after a click.
 * @param context Opaque owner context passed unchanged to callback.
 */
void WifiScreen::setBackCallback(
    BackCallback callback,
    void* context
) {
    backCallback_ = callback;
    backContext_ = context;
}

/**
 * @brief Registers the persistent bottom-navigation callback.
 */
void WifiScreen::setNavigationCallback(
    NavigationCallback callback,
    void* context
) {
    navigationCallback_ = callback;
    navigationContext_ = context;
}

/**
 * @brief Builds the Wi-Fi inventory view and its fixed controls.
 *
 * @param parent LVGL screen object that owns the view.
 * @return true if the required widget tree was created.
 *
 * The header, pagination footer, and global navigation bar are fixed.
 * Only contentContainer_ scrolls. The content container contains the status
 * label and the list of up to four Wi-Fi records for the current page.
 */
bool WifiScreen::create(lv_obj_t* parent) {
    if (parent == nullptr ||
        inventory_ == nullptr ||
        coordinator_ == nullptr) {
        return false;
    }

    root_ = lv_obj_create(parent);
    if (root_ == nullptr) {
        return false;
    }

    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    applyBackground(root_);
    lv_obj_set_style_pad_all(
        root_,
        Spacing::screenMargin,
        LV_PART_MAIN
    );
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        root_,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START
    );
    lv_obj_set_style_pad_row(
        root_,
        Spacing::standard,
        LV_PART_MAIN
    );
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(root_, LV_SCROLLBAR_MODE_OFF);

    headerContainer_ = nullptr;
    contentContainer_ = nullptr;
    statusLabel_ = nullptr;
    listContainer_ = nullptr;
    controlsContainer_ = nullptr;
    previousButton_ = nullptr;
    pageLabel_ = nullptr;
    nextButton_ = nullptr;
    navigationContainer_ = nullptr;

    // --------------------------------------------------------
    // Fixed header: Back + screen title.
    // --------------------------------------------------------

    headerContainer_ = lv_obj_create(root_);
    if (headerContainer_ == nullptr) {
        return false;
    }

    lv_obj_set_width(headerContainer_, lv_pct(100));
    lv_obj_set_height(headerContainer_, kHeaderHeight);
    lv_obj_set_flex_grow(headerContainer_, 0);
    applyTransparent(headerContainer_);

    lv_obj_t* back = lv_btn_create(headerContainer_);
    if (back == nullptr) {
        return false;
    }

    // Preserve the original Back-button dimensions and appearance.
    lv_obj_set_size(
        back,
        kBackButtonWidth,
        kBackButtonHeight
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
    if (backText == nullptr) {
        return false;
    }

    lv_label_set_text(backText, "Back");
    lv_obj_center(backText);
    lv_obj_set_style_text_color(
        backText,
        lv_color_hex(Colors::textPrimary),
        LV_PART_MAIN
    );
    lv_obj_set_style_text_font(
        backText,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_t* title = lv_label_create(headerContainer_);
    if (title == nullptr) {
        return false;
    }

    lv_label_set_text(title, "WI-FI NETWORKS");
    lv_obj_align(title, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_color(
        title,
        lv_color_hex(Colors::primary),
        LV_PART_MAIN
    );
    lv_obj_set_style_text_font(
        title,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    // --------------------------------------------------------
    // Scrollable middle content.
    // --------------------------------------------------------

    contentContainer_ = lv_obj_create(root_);
    if (contentContainer_ == nullptr) {
        return false;
    }

    lv_obj_set_width(contentContainer_, lv_pct(100));
    lv_obj_set_height(contentContainer_, 0);
    lv_obj_set_flex_grow(contentContainer_, 1);
    applyTransparent(contentContainer_);
    lv_obj_set_flex_flow(contentContainer_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        contentContainer_,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START
    );

    lv_obj_set_style_pad_left(
        contentContainer_,
        0,
        LV_PART_MAIN
    );
    lv_obj_set_style_pad_right(
        contentContainer_,
        kScrollbarGutter,
        LV_PART_MAIN
    );
    lv_obj_set_style_pad_top(
        contentContainer_,
        0,
        LV_PART_MAIN
    );
    lv_obj_set_style_pad_bottom(
        contentContainer_,
        0,
        LV_PART_MAIN
    );
    lv_obj_set_style_pad_row(
        contentContainer_,
        Spacing::standard,
        LV_PART_MAIN
    );

    configureContentScrollbar(contentContainer_);

    statusLabel_ = makeLabel(
        contentContainer_,
        "Loading Wi-Fi inventory...",
        Colors::textSecondary
    );

    if (statusLabel_ == nullptr) {
        return false;
    }

    listContainer_ = lv_obj_create(contentContainer_);
    if (listContainer_ == nullptr) {
        return false;
    }

    lv_obj_set_width(listContainer_, lv_pct(100));
    lv_obj_set_height(listContainer_, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(listContainer_, 0);
    lv_obj_set_flex_flow(listContainer_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        listContainer_,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START
    );
    lv_obj_set_style_pad_all(
        listContainer_,
        0,
        LV_PART_MAIN
    );
    lv_obj_set_style_pad_row(
        listContainer_,
        kCardSpacing,
        LV_PART_MAIN
    );
    lv_obj_set_style_bg_opa(
        listContainer_,
        LV_OPA_TRANSP,
        LV_PART_MAIN
    );
    lv_obj_set_style_border_width(
        listContainer_,
        0,
        LV_PART_MAIN
    );
    lv_obj_clear_flag(listContainer_, LV_OBJ_FLAG_SCROLLABLE);

    // --------------------------------------------------------
    // Fixed pagination footer.
    // --------------------------------------------------------

    controlsContainer_ = lv_obj_create(root_);
    if (controlsContainer_ == nullptr) {
        return false;
    }

    lv_obj_set_width(controlsContainer_, lv_pct(100));
    lv_obj_set_height(
        controlsContainer_,
        kPaginationHeight
    );
    lv_obj_set_flex_grow(controlsContainer_, 0);
    applyTransparent(controlsContainer_);
    lv_obj_set_flex_flow(
        controlsContainer_,
        LV_FLEX_FLOW_ROW
    );
    lv_obj_set_flex_align(
        controlsContainer_,
        LV_FLEX_ALIGN_SPACE_BETWEEN,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    previousButton_ = lv_btn_create(controlsContainer_);
    nextButton_ = lv_btn_create(controlsContainer_);

    if (previousButton_ == nullptr || nextButton_ == nullptr) {
        return false;
    }

    lv_obj_set_size(
        previousButton_,
        kPaginationButtonWidth,
        kPaginationButtonHeight
    );

    lv_obj_set_size(
        nextButton_,
        kPaginationButtonWidth,
        kPaginationButtonHeight
    );

    applyPaginationButtonStyle(previousButton_);
    applyPaginationButtonStyle(nextButton_);

    lv_obj_add_event_cb(
        previousButton_,
        &WifiScreen::previousEventCallback,
        LV_EVENT_CLICKED,
        this
    );

    lv_obj_add_event_cb(
        nextButton_,
        &WifiScreen::nextEventCallback,
        LV_EVENT_CLICKED,
        this
    );

    lv_obj_t* previousText = lv_label_create(previousButton_);
    lv_obj_t* nextText = lv_label_create(nextButton_);

    if (previousText == nullptr || nextText == nullptr) {
        return false;
    }

    lv_label_set_text(previousText, "Previous");
    lv_label_set_text(nextText, "Next");
    lv_obj_center(previousText);
    lv_obj_center(nextText);

    lv_obj_set_style_text_color(
        previousText,
        lv_color_hex(Colors::textPrimary),
        LV_PART_MAIN
    );
    lv_obj_set_style_text_color(
        nextText,
        lv_color_hex(Colors::textPrimary),
        LV_PART_MAIN
    );
    lv_obj_set_style_text_font(
        previousText,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );
    lv_obj_set_style_text_font(
        nextText,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    pageLabel_ = lv_label_create(controlsContainer_);
    if (pageLabel_ == nullptr) {
        return false;
    }

    lv_obj_set_style_text_color(
        pageLabel_,
        lv_color_hex(Colors::textSecondary),
        LV_PART_MAIN
    );
    lv_obj_set_style_text_font(
        pageLabel_,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    // --------------------------------------------------------
    // Fixed global navigation.
    // --------------------------------------------------------

    if (!createNavigationBar(root_)) {
        return false;
    }

    reloadInventory();
    renderPage();
    refresh();

    return true;
}

/**
 * @brief Creates the persistent four-destination navigation bar.
 *
 * @param parent Wi-Fi screen root.
 * @return true if all navigation buttons were created.
 */
bool WifiScreen::createNavigationBar(lv_obj_t* parent) {
    if (parent == nullptr) {
        return false;
    }

    navigationContainer_ = lv_obj_create(parent);
    if (navigationContainer_ == nullptr) {
        return false;
    }

    lv_obj_set_width(navigationContainer_, lv_pct(100));
    lv_obj_set_height(
        navigationContainer_,
        kNavigationHeight
    );
    lv_obj_set_flex_grow(navigationContainer_, 0);
    applyTransparent(navigationContainer_);
    lv_obj_set_flex_flow(
        navigationContainer_,
        LV_FLEX_FLOW_ROW
    );
    lv_obj_set_flex_align(
        navigationContainer_,
        LV_FLEX_ALIGN_SPACE_BETWEEN,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );
    lv_obj_set_style_pad_column(
        navigationContainer_,
        Spacing::compact,
        LV_PART_MAIN
    );

    const bool dashboardCreated =
        createNavigationButton(
            navigationContainer_,
            "HOME",
            NavigationDestination::Dashboard,
            true,
            false
        ) != nullptr;

    const bool wifiCreated =
        createNavigationButton(
            navigationContainer_,
            "WI-FI",
            NavigationDestination::Wifi,
            true,
            true
        ) != nullptr;

    const bool bluetoothCreated =
        createNavigationButton(
            navigationContainer_,
            "BT",
            NavigationDestination::Bluetooth,
            false,
            false
        ) != nullptr;

    const bool settingsCreated =
        createNavigationButton(
            navigationContainer_,
            "SET",
            NavigationDestination::Settings,
            false,
            false
        ) != nullptr;

    return dashboardCreated &&
           wifiCreated &&
           bluetoothCreated &&
           settingsCreated;
}

/**
 * @brief Creates one neutral-style bottom-navigation button.
 *
 * @param parent Navigation row.
 * @param text Short destination label.
 * @param destination Destination passed to the navigation callback.
 * @param enabled Whether the destination is implemented.
 * @param selected Whether this is the active screen.
 * @return Button object or nullptr.
 */
lv_obj_t* WifiScreen::createNavigationButton(
    lv_obj_t* parent,
    const char* text,
    NavigationDestination destination,
    bool enabled,
    bool selected
) {
    if (parent == nullptr) {
        return nullptr;
    }

    lv_obj_t* button = lv_btn_create(parent);
    if (button == nullptr) {
        return nullptr;
    }

    lv_obj_set_flex_grow(button, 1);
    lv_obj_set_height(button, 36);
    applyNavigationButton(button);

    if (selected) {
        applyNavigationSelected(button);
    }

    if (!enabled) {
        applyNavigationDisabled(button);
    } else {
        lv_obj_set_user_data(
            button,
            reinterpret_cast<void*>(
                static_cast<uintptr_t>(destination)
            )
        );

        lv_obj_add_event_cb(
            button,
            &WifiScreen::navigationEventCallback,
            LV_EVENT_CLICKED,
            this
        );
    }

    lv_obj_t* label = lv_label_create(button);
    if (label == nullptr) {
        return nullptr;
    }

    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_set_style_text_color(
        label,
        lv_color_hex(
            selected ? Colors::primary :
            (enabled ? Colors::textPrimary : Colors::textMuted)
        ),
        LV_PART_MAIN
    );
    lv_obj_set_style_text_font(
        label,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    return button;
}

/**
 * @brief Refreshes scan-state text and changed inventory snapshots.
 *
 * A changed inventory generation triggers a bounded copy and page redraw.
 * If only scan state changes, the Wi-Fi cards are not rebuilt.
 */
void WifiScreen::refresh() {
    if (inventory_ == nullptr) {
        return;
    }

    if (inventory_->generation() != renderedGeneration_) {
        reloadInventory();
        renderPage();
    }

    if (statusLabel_ != nullptr && coordinator_ != nullptr) {
        const bool scanning =
            coordinator_->scanInProgress(core::RadioType::Wifi);

        lv_label_set_text(
            statusLabel_,
            scanning
                ? "Wi-Fi scan running"
                : (deviceCount_ == 0
                    ? "No Wi-Fi networks discovered"
                    : "Wi-Fi inventory")
        );
    }
}

/**
 * @brief Copies Wi-Fi records into the screen's fixed-size snapshot.
 *
 * DeviceInventory remains authoritative. The local array is deliberately
 * bounded to kItemsPerPage records, so this screen never duplicates the
 * entire historical inventory.
 */
void WifiScreen::reloadInventory() {
    if (inventory_ == nullptr) {
        return;
    }

    const std::size_t totalWifiRecords =
        inventory_->count(core::RadioType::Wifi);

    const std::size_t pageCount =
        totalWifiRecords == 0
            ? 1
            : (totalWifiRecords + kItemsPerPage - 1) /
                kItemsPerPage;

    if (pageIndex_ >= pageCount) {
        pageIndex_ = pageCount - 1;
    }

    const std::size_t offset = pageIndex_ * kItemsPerPage;

    deviceCount_ = inventory_->copyRadioPage(
        core::RadioType::Wifi,
        offset,
        devices_.data(),
        devices_.size()
    );

    if (deviceCount_ > devices_.size()) {
        deviceCount_ = devices_.size();
    }

    renderedGeneration_ = inventory_->generation();
}

/**
 * @brief Rebuilds the visible page from the local inventory snapshot.
 *
 * Each card shows SSID, BSSID, RSSI, channel, raw encryption value, and
 * current visibility. No SSID-based de-duplication is performed.
 */
void WifiScreen::renderPage() {
    if (listContainer_ == nullptr) {
        return;
    }

    lv_obj_clean(listContainer_);

    if (deviceCount_ == 0) {
        lv_obj_t* empty = lv_obj_create(listContainer_);
        if (empty != nullptr) {
            lv_obj_set_width(empty, lv_pct(100));
            lv_obj_set_height(empty, 72);
            applyPanel(empty);

            lv_obj_set_style_pad_left(
                empty,
                kCardHorizontalPadding,
                LV_PART_MAIN
            );
            lv_obj_set_style_pad_right(
                empty,
                kCardHorizontalPadding,
                LV_PART_MAIN
            );
            lv_obj_set_style_pad_top(
                empty,
                Spacing::standard,
                LV_PART_MAIN
            );
            lv_obj_set_style_pad_bottom(
                empty,
                Spacing::standard,
                LV_PART_MAIN
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
                contentContainer_,
                0,
                LV_ANIM_OFF
            );
        }

        return;
    }

    for (std::size_t i = 0; i < deviceCount_; ++i) {
        const core::Device& device = devices_[i];

        lv_obj_t* card = lv_obj_create(listContainer_);
        if (card == nullptr) {
            continue;
        }

        lv_obj_set_width(card, lv_pct(100));
        lv_obj_set_height(card, kNetworkCardHeight);
        applyPanel(card);

        lv_obj_set_style_pad_left(
            card,
            kCardHorizontalPadding,
            LV_PART_MAIN
        );
        lv_obj_set_style_pad_right(
            card,
            kCardHorizontalPadding,
            LV_PART_MAIN
        );
        lv_obj_set_style_pad_top(
            card,
            kCardVerticalPadding,
            LV_PART_MAIN
        );
        lv_obj_set_style_pad_bottom(
            card,
            kCardVerticalPadding,
            LV_PART_MAIN
        );

        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(
            card,
            LV_FLEX_ALIGN_START,
            LV_FLEX_ALIGN_START,
            LV_FLEX_ALIGN_START
        );
        lv_obj_set_style_pad_row(
            card,
            Spacing::compact,
            LV_PART_MAIN
        );
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        const char* ssid = device.wifi.ssid.empty()
            ? (device.wifi.hidden
                ? "<hidden network>"
                : "<unnamed network>")
            : device.wifi.ssid.c_str();

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

        char details[112];

        if (device.hasRssi) {
            std::snprintf(
                details,
                sizeof(details),
                "%d dBm  |  CH %d  |  ENC %u  |  %s",
                static_cast<int>(device.rssi),
                static_cast<int>(device.wifi.channel),
                static_cast<unsigned>(device.wifi.encryption),
                device.currentlyVisible ? "VISIBLE" : "NOT SEEN"
            );
        } else {
            std::snprintf(
                details,
                sizeof(details),
                "RSSI n/a  |  CH %d  |  ENC %u  |  %s",
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
            contentContainer_,
            0,
            LV_ANIM_OFF
        );
    }
}

/**
 * @brief Updates page indicator text and disables out-of-range navigation.
 */
void WifiScreen::updatePageControls() {
    if (inventory_ == nullptr) {
        return;
    }

    const std::size_t totalRecords =
        inventory_->count(core::RadioType::Wifi);

    const std::size_t pageCount =
        totalRecords == 0
            ? 1
            : (totalRecords + kItemsPerPage - 1) /
                kItemsPerPage;

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
    }

    if (previousButton_ != nullptr) {
        if (pageIndex_ == 0) {
            lv_obj_add_state(
                previousButton_,
                LV_STATE_DISABLED
            );
        } else {
            lv_obj_clear_state(
                previousButton_,
                LV_STATE_DISABLED
            );
        }
    }

    if (nextButton_ != nullptr) {
        if (pageIndex_ + 1 >= pageCount) {
            lv_obj_add_state(
                nextButton_,
                LV_STATE_DISABLED
            );
        } else {
            lv_obj_clear_state(
                nextButton_,
                LV_STATE_DISABLED
            );
        }
    }
}

/**
 * @brief Changes the zero-based page index by one within valid bounds.
 *
 * @param direction Negative selects the previous page; positive selects next.
 */
void WifiScreen::changePage(int direction) {
    if (inventory_ == nullptr) {
        return;
    }

    const std::size_t totalRecords =
        inventory_->count(core::RadioType::Wifi);

    const std::size_t pageCount =
        totalRecords == 0
            ? 1
            : (totalRecords + kItemsPerPage - 1) /
                kItemsPerPage;

    if (direction < 0 && pageIndex_ > 0) {
        --pageIndex_;
    }

    if (direction > 0 && pageIndex_ + 1 < pageCount) {
        ++pageIndex_;
    }

    reloadInventory();
    renderPage();
}

/**
 * @brief Forwards the Back action to the registered screen owner.
 */
void WifiScreen::goBack() {
    if (backCallback_ != nullptr) {
        backCallback_(backContext_);
    }
}

/**
 * @brief LVGL event adapter for the Back button.
 */
void WifiScreen::backEventCallback(lv_event_t* event) {
    if (event == nullptr) {
        return;
    }

    auto* self = static_cast<WifiScreen*>(
        lv_event_get_user_data(event)
    );

    if (self != nullptr) {
        self->goBack();
    }
}

/**
 * @brief LVGL event adapter for the Previous page button.
 */
void WifiScreen::previousEventCallback(lv_event_t* event) {
    if (event == nullptr) {
        return;
    }

    auto* self = static_cast<WifiScreen*>(
        lv_event_get_user_data(event)
    );

    if (self != nullptr) {
        self->changePage(-1);
    }
}

/**
 * @brief LVGL event adapter for the Next page button.
 */
void WifiScreen::nextEventCallback(lv_event_t* event) {
    if (event == nullptr) {
        return;
    }

    auto* self = static_cast<WifiScreen*>(
        lv_event_get_user_data(event)
    );

    if (self != nullptr) {
        self->changePage(1);
    }
}

/**
 * @brief Dispatches an enabled bottom-navigation click to the owner.
 */
void WifiScreen::navigationEventCallback(lv_event_t* event) {
    if (event == nullptr) {
        return;
    }

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

    const uintptr_t destinationValue = reinterpret_cast<uintptr_t>(
        lv_obj_get_user_data(button)
    );

    self->navigationCallback_(
        self->navigationContext_,
        static_cast<NavigationDestination>(destinationValue)
    );
}

} // namespace wirelessrecon::ui::screens