
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lvgl.h>

#include "core/DeviceInventory.h"
#include "core/WirelessCoordinator.h"
#include "ui/theme/UITheme.h"

namespace wirelessrecon::ui::screens {

/**
 * @class WifiScreen
 * @brief Displays a paginated, read-only Wi-Fi inventory.
 *
 * @details
 * DeviceInventory remains the authoritative record owner. This screen
 * retains only four copied records for the currently selected page.
 *
 * Layout:
 * - Fixed header with Back and title.
 * - One scrollable middle content region.
 * - Fixed pagination controls.
 * - Fixed global bottom navigation.
 *
 * All inventory reads and LVGL operations run on the application task.
 */
class WifiScreen {
public:
    static constexpr std::size_t kItemsPerPage = 4;

    using BackCallback = void (*)(void* context);

    using NavigationCallback = void (*)(
        void* context,
        theme::NavigationDestination destination
    );

    void bind(
        core::WirelessCoordinator& coordinator,
        core::DeviceInventory& inventory
    );

    void setBackCallback(BackCallback callback, void* context);

    void setNavigationCallback(
        NavigationCallback callback,
        void* context
    );

    bool create(lv_obj_t* parent);
    void refresh();

private:
    core::WirelessCoordinator* coordinator_ = nullptr;
    core::DeviceInventory* inventory_ = nullptr;

    BackCallback backCallback_ = nullptr;
    void* backContext_ = nullptr;

    NavigationCallback navigationCallback_ = nullptr;
    void* navigationContext_ = nullptr;

    lv_obj_t* root_ = nullptr;
    lv_obj_t* headerContainer_ = nullptr;
    lv_obj_t* contentContainer_ = nullptr;
    lv_obj_t* statusLabel_ = nullptr;
    lv_obj_t* listContainer_ = nullptr;

    lv_obj_t* controlsContainer_ = nullptr;
    lv_obj_t* previousButton_ = nullptr;
    lv_obj_t* pageLabel_ = nullptr;
    lv_obj_t* nextButton_ = nullptr;

    lv_obj_t* navigationContainer_ = nullptr;

    std::array<core::Device, kItemsPerPage> pageDevices_{};

    std::size_t deviceCount_ = 0;
    std::size_t pageItemCount_ = 0;
    std::size_t pageIndex_ = 0;

    std::uint32_t renderedGeneration_ = UINT32_MAX;

    static void backEventCallback(lv_event_t* event);
    static void previousEventCallback(lv_event_t* event);
    static void nextEventCallback(lv_event_t* event);
    static void navigationEventCallback(lv_event_t* event);

    void goBack();
    void changePage(int direction);

    bool createNavigationBar();
    void reloadInventory();
    void renderPage();
    void updatePageControls();
};

} // namespace wirelessrecon::ui::screens
