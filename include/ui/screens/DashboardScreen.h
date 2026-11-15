//
// Created by dedlee on 11/15/26.
//


/**
 * @file DashboardScreen.h
 * @brief Dashboard presentation component for WirelessRecon.
 *
 * @details
 * DashboardScreen renders the primary application overview.
 *
 * Responsibilities:
 * - Display Wi-Fi, BLE, and Bluetooth Classic inventory counts.
 * - Display total discovered devices.
 * - Provide user controls for supported scan requests.
 * - Display current scan activity and request results.
 *
 * Architectural boundaries:
 * - WirelessCoordinator owns scan orchestration.
 * - DeviceInventory owns discovered-device records.
 * - DashboardScreen only reads inventory state and submits scan requests.
 * - DashboardScreen never creates or owns radio scanner implementations.
 *
 * Ownership:
 * - The LVGL screen parent owns all widgets created by this component.
 * - The component retains non-owning pointers to its generated widgets.
 * - The coordinator and inventory references must outlive this component.
 *
 * Threading:
 * - All public methods must be called from the LVGL-owning application task.
 * - Radio callbacks must never modify LVGL objects directly.
 *
 * Lifecycle:
 *   bind() -> create() -> refresh()
 *
 * The component may be recreated when the application changes screens.
 */

#pragma once

#include <lvgl.h>

#include "core/DeviceInventory.h"
#include "core/WirelessCoordinator.h"

namespace wirelessrecon::ui::screens {

class DashboardScreen {
public:
    DashboardScreen() = default;

    DashboardScreen(const DashboardScreen&) = delete;
    DashboardScreen& operator=(const DashboardScreen&) = delete;

    /**
     * @brief Connects the dashboard to existing application services.
     *
     * @param coordinator Existing wireless scan coordinator.
     * @param inventory Existing device inventory.
     *
     * @note Neither service is owned or deleted by DashboardScreen.
     */
    void bind(
        core::WirelessCoordinator& coordinator,
        core::DeviceInventory& inventory
    );

    /**
     * @brief Creates dashboard widgets inside the supplied LVGL parent.
     *
     * @param parent LVGL object that owns the dashboard widgets.
     *
     * @return true when the dashboard has been created.
     *
     * @pre bind() must have been called.
     * @pre parent must be a valid LVGL object.
     *
     * Calling create() again replaces the component's widget references.
     * The parent is responsible for cleaning up its previous children.
     */
    bool create(lv_obj_t* parent);

    /**
     * @brief Updates displayed counts and scan status.
     *
     * @details
     * This method reads authoritative values from DeviceInventory.
     * It does not initiate scans or process radio observations.
     *
     * Call periodically from the LVGL application task rather than from
     * an asynchronous scanner callback.
     */
    void refresh();

private:
    core::WirelessCoordinator* coordinator_ = nullptr;
    core::DeviceInventory* inventory_ = nullptr;

    lv_obj_t* wifiCountLabel_ = nullptr;
    lv_obj_t* bleCountLabel_ = nullptr;
    lv_obj_t* classicCountLabel_ = nullptr;
    lv_obj_t* totalCountLabel_ = nullptr;
    lv_obj_t* scanStatusLabel_ = nullptr;

    /**
     * Root container for this dashboard.
     *
     * LVGL owns this object and its child widget tree. This class keeps
     * a non-owning pointer so refresh and layout operations can access it.
     */
    lv_obj_t* root_ = nullptr;

    /**
     * @brief Identifies the requested radio for a scan-button event.
     *
     * The event callback receives this component through LVGL user data.
     * It forwards the request to WirelessCoordinator.
     */
    static void scanButtonEventCallback(lv_event_t* event);

    /**
     * @brief Requests a scan and updates the visible result message.
     *
     * @param radio Radio technology requested by the user.
     *
     * The coordinator validates board support, scanner availability,
     * scanner state, and Bluetooth controller conflicts.
     */
    void requestScan(core::RadioType radio);

    /**
     * @brief Creates one scan button.
     *
     * @param parent Parent LVGL container.
     * @param text Visible button text.
     * @param radio Radio associated with the button.
     * @return Newly created button, or nullptr on failure.
     */
    lv_obj_t* createScanButton(
        lv_obj_t* parent,
        const char* text,
        core::RadioType radio
    );

    /**
     * @brief Creates a metric row and retains its value label.
     *
     * @param parent Parent LVGL container.
     * @param title Human-readable metric title.
     * @param value Initial metric value.
     * @return Value label updated by refresh().
     */
    lv_obj_t* createMetricRow(
        lv_obj_t* parent,
        const char* title,
        const char* value
    );

    /** Returns a user-facing message for a coordinator result. */
    static const char* scanResultMessage(
        core::ScanRequestResult result
    );
};

} // namespace wirelessrecon::ui::screens
