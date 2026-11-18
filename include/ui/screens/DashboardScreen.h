/**
 * @file DashboardScreen.h
 * @brief Dashboard screen controller and radio scan controls.
 */
#pragma once

#include <lvgl.h>

#include "core/DeviceInventory.h"
#include "core/WirelessCoordinator.h"
#include "ui/theme/UITheme.h"

namespace wirelessrecon::ui::screens {

/**
 * @class DashboardScreen
 * @brief Builds and refreshes the WirelessRecon operational dashboard.
 *
 * @details
 * DashboardScreen presents radio inventory counts, scan status, and explicit
 * Wi-Fi/BLE scan actions. It does not own the coordinator or inventory.
 *
 * All methods that create or modify LVGL objects must execute on the
 * application/UI task.
 */
class DashboardScreen {
public:
    using NavigationCallback = void (*)(
        void* context,
        theme::NavigationDestination destination
    );

    /**
     * @brief Binds non-owning references to application services.
     * @param coordinator Scan orchestration service.
     * @param inventory Authoritative device inventory.
     */
    void bind(
        core::WirelessCoordinator& coordinator,
        core::DeviceInventory& inventory
    );

    /**
     * @brief Registers the owner callback for bottom navigation.
     * @param callback Navigation dispatcher.
     * @param context Opaque owner context.
     */
    void setNavigationCallback(
        NavigationCallback callback,
        void* context
    );

    /**
     * @brief Creates the dashboard widget tree.
     * @param parent Active LVGL screen.
     * @return true when all required widgets were created.
     */
    bool create(lv_obj_t* parent);

    /**
     * @brief Refreshes inventory counts and scan status.
     */
    void refresh();

private:
    core::WirelessCoordinator* coordinator_ = nullptr;
    core::DeviceInventory* inventory_ = nullptr;

    NavigationCallback navigationCallback_ = nullptr;
    void* navigationContext_ = nullptr;

    lv_obj_t* root_ = nullptr;
    lv_obj_t* contentContainer_ = nullptr;
    lv_obj_t* wifiCountLabel_ = nullptr;
    lv_obj_t* bleCountLabel_ = nullptr;
    lv_obj_t* classicCountLabel_ = nullptr;
    lv_obj_t* scanStatusLabel_ = nullptr;
    lv_obj_t* navigationContainer_ = nullptr;

    bool scanWasActive_ = false;

    /**
     * @brief Creates one radio inventory metric card.
     */
    lv_obj_t* createMetricCard(
        lv_obj_t* parent,
        const char* title,
        const char* initialValue
    );

    /**
     * @brief Creates a dashboard scan action.
     */
    lv_obj_t* createScanButton(
        lv_obj_t* parent,
        const char* text,
        core::RadioType radio
    );

    /**
     * @brief Creates the persistent four-destination navigation bar.
     */
    bool createNavigationBar(lv_obj_t* parent);

    /**
     * @brief Creates one bottom-navigation destination.
     */
    lv_obj_t* createNavigationButton(
        lv_obj_t* parent,
        const char* text,
        theme::NavigationDestination destination,
        bool enabled,
        bool selected
    );

    /** @brief Handles Wi-Fi/BLE scan-button click events. */
    static void scanButtonEventCallback(lv_event_t* event);

    /** @brief Handles bottom-navigation click events. */
    static void navigationEventCallback(lv_event_t* event);

    /** @brief Submits a scan request to WirelessCoordinator. */
    void requestScan(core::RadioType radio);

    /** @brief Converts a coordinator result into user-facing status text. */
    static const char* scanResultMessage(
        core::ScanRequestResult result
    );

    /** @brief Updates the dashboard scan-status label. */
    void setStatus(const char* message);
};

} // namespace wirelessrecon::ui::screens