
/**
 * @file DashboardScreen.h
 * @brief Primary WirelessRecon dashboard presentation component.
 *
 * The dashboard provides a concise operational overview:
 * - Wi-Fi, BLE, and Bluetooth Classic inventory counts.
 * - Direct Wi-Fi and BLE scan actions.
 * - A single status area positioned above the scan controls.
 *
 * It deliberately does not display a redundant total-inventory metric.
 * Radio-specific device exploration belongs on the corresponding radio
 * screens rather than being duplicated on the dashboard.
 *
 * Architecture:
 * - WirelessCoordinator owns scan orchestration.
 * - DeviceInventory owns discovered-device records.
 * - DashboardScreen reads inventory and submits scan requests.
 * - DashboardScreen does not own or access radio scanner implementations.
 *
 * Ownership:
 * - LVGL owns the widget tree created beneath the supplied parent.
 * - This component retains non-owning widget pointers.
 * - Coordinator and inventory references must outlive this component.
 *
 * Threading:
 * - All public methods must be called from the LVGL application task.
 * - Radio callbacks must never modify LVGL objects directly.
 */

#pragma once

#include <cstdint>

#include <lvgl.h>

#include "core/DeviceInventory.h"
#include "core/WirelessCoordinator.h"

namespace wirelessrecon::ui::screens {

class DashboardScreen final {
public:
    DashboardScreen() = default;

    DashboardScreen(const DashboardScreen&) = delete;
    DashboardScreen& operator=(const DashboardScreen&) = delete;

    /**
     * @brief Binds the dashboard to existing application services.
     *
     * The supplied services are non-owning references. Their lifetime is
     * controlled by the application composition root.
     */
    void bind(
        core::WirelessCoordinator& coordinator,
        core::DeviceInventory& inventory
    );

    /**
     * @brief Creates the dashboard widget tree.
     *
     * All dashboard widgets are created beneath the component's root
     * container. The root owns the dashboard layout and scrolling.
     *
     * The scan status panel is placed above the scan controls.
     *
     * @param parent LVGL parent, normally the active LVGL screen.
     * @return true when dashboard creation succeeds.
     */
    bool create(lv_obj_t* parent);

    /**
     * @brief Refreshes radio counts and scan activity.
     *
     * Must be called from the LVGL application task.
     */
    void refresh();

private:
    core::WirelessCoordinator* coordinator_ = nullptr;
    core::DeviceInventory* inventory_ = nullptr;

    // Root container and its child widgets are owned by LVGL.
    lv_obj_t* root_ = nullptr;

    // Radio metric value labels.
    lv_obj_t* wifiCountLabel_ = nullptr;
    lv_obj_t* bleCountLabel_ = nullptr;
    lv_obj_t* classicCountLabel_ = nullptr;

    // Scan feedback.
    lv_obj_t* scanStatusLabel_ = nullptr;

    // Tracks a running scan so completion can be reported once.
    bool scanWasActive_ = false;

    /**
     * @brief Creates one radio metric card.
     *
     * @param parent Parent metrics container.
     * @param title Radio name.
     * @param initialValue Initial displayed count.
     * @return Value label, or nullptr if creation fails.
     */
    lv_obj_t* createMetricCard(
        lv_obj_t* parent,
        const char* title,
        const char* initialValue
    );

    /**
     * @brief Creates a touch-friendly scan action.
     *
     * @param parent Parent controls container.
     * @param text Button label.
     * @param radio Radio requested by the button.
     * @return Created button, or nullptr.
     */
    lv_obj_t* createScanButton(
        lv_obj_t* parent,
        const char* text,
        core::RadioType radio
    );

    /** LVGL event callback for scan-button presses. */
    static void scanButtonEventCallback(lv_event_t* event);

    /** Forwards a user scan request to WirelessCoordinator. */
    void requestScan(core::RadioType radio);

    /** Returns concise feedback for a scan request result. */
    static const char* scanResultMessage(
        core::ScanRequestResult result
    );

    /** Updates the single dashboard status label. */
    void setStatus(const char* message);
};

} // namespace wirelessrecon::ui::screens
