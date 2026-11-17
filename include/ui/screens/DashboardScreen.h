
#pragma once

#include <lvgl.h>

#include "core/DeviceInventory.h"
#include "core/WirelessCoordinator.h"
#include "ui/theme/UITheme.h"

namespace wirelessrecon::ui::screens {

/**
 * @class DashboardScreen
 * @brief Top-level radio monitoring dashboard.
 *
 * @details
 * Layout:
 * - Fixed application header.
 * - One vertically scrollable content region.
 * - Fixed bottom navigation bar.
 *
 * The dashboard does not own radio hardware, scanners, coordinator, or
 * inventory services. All LVGL operations execute on the application task.
 */
class DashboardScreen {
public:
    using NavigationCallback = void (*)(
        void* context,
        theme::NavigationDestination destination
    );

    void bind(
        core::WirelessCoordinator& coordinator,
        core::DeviceInventory& inventory
    );

    void setNavigationCallback(
        NavigationCallback callback,
        void* context
    );

    bool create(lv_obj_t* parent);
    void refresh();

private:
    core::WirelessCoordinator* coordinator_ = nullptr;
    core::DeviceInventory* inventory_ = nullptr;

    NavigationCallback navigationCallback_ = nullptr;
    void* navigationContext_ = nullptr;

    lv_obj_t* root_ = nullptr;
    lv_obj_t* headerContainer_ = nullptr;
    lv_obj_t* contentContainer_ = nullptr;
    lv_obj_t* navigationContainer_ = nullptr;

    lv_obj_t* wifiCountLabel_ = nullptr;
    lv_obj_t* bleCountLabel_ = nullptr;
    lv_obj_t* classicCountLabel_ = nullptr;
    lv_obj_t* scanStatusLabel_ = nullptr;

    bool scanWasActive_ = false;

    lv_obj_t* createMetricCard(
        lv_obj_t* parent,
        const char* title,
        const char* initialValue
    );

    lv_obj_t* createScanButton(
        lv_obj_t* parent,
        const char* text,
        core::RadioType radio
    );

    bool createNavigationBar();

    static void scanButtonEventCallback(lv_event_t* event);
    static void navigationEventCallback(lv_event_t* event);

    void requestScan(core::RadioType radio);

    const char* scanResultMessage(
        core::ScanRequestResult result
    );

    void setStatus(const char* message);
};

} // namespace wirelessrecon::ui::screens
