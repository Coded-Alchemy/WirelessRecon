//
// Created by dedlee on 11/15/26.
//

#pragma once

/**
 * @file ApplicationRuntime.h
 * @brief Owns and coordinates the WirelessRecon application runtime.
 *
 * ApplicationRuntime is the application composition root. It constructs the
 * board-selected hardware adapters, core services, radio controller, UI
 * manager, and optional hardware diagnostics.
 *
 * It preserves the existing startup and runtime behavior:
 * - Core memory services initialize before dependent application services.
 * - Display and touch initialize before LVGL.
 * - Normal mode starts Wi-Fi through RadioController in modular builds.
 * - BLE remains registered but is not automatically started in normal mode.
 * - Diagnostic mode runs Wi-Fi first and starts BLE after Wi-Fi terminates.
 * - LVGL is serviced only from the application task.
 *
 * The Arduino entry point should only delegate to begin() and loop().
 */

#include <cstdint>

#include "hardware/BoardProfile.h"

#include "core/CoreMemoryServices.h"
#include "core/DeviceInventory.h"
#include "core/WirelessCoordinator.h"

#include "hardware/radio/WifiScanner.h"

#if defined(WR_BOARD_THINGPULSE_WROVER) || \
    defined(WR_BOARD_ESP32_32E)
#define WR_RUNTIME_HAS_BLUEDROID_BLE_SCANNER
#include "hardware/radio/BleScanner.h"
#endif

#include "hardware/display/DisplayConfigFactory.h"
#include "hardware/display/DisplayManager.h"
#include "hardware/display/drivers/LovyanGFXDisplay.h"

#include "hardware/touch/TouchConfigFactory.h"
#include "hardware/touch/TouchManager.h"
#include "hardware/touch/drivers/FT6236Touch.h"

#include "ui/lvgl/LVGLManager.h"

#ifdef WR_MODULAR_BUILD
#include "hardware/radio/RadioController.h"
#endif

#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS
#include "diagnostics/BoardDiagnostics.h"
#include "diagnostics/DisplayDiagnostics.h"
#include "diagnostics/MemoryDiagnostics.h"
#include "diagnostics/TouchDiagnostics.h"
#include "diagnostics/WifiDiagnostics.h"

#ifdef WR_RUNTIME_HAS_BLUEDROID_BLE_SCANNER
#include "diagnostics/BleDiagnostics.h"
#endif
#endif

namespace wirelessrecon::application {

/**
 * @brief Owns application components and coordinates their lifecycle.
 *
 * Hardware and application components are constructed in dependency order.
 * CoreMemoryServices owns the observation queue, inventory, and coordinator.
 * Scanner, display, touch, UI, and diagnostics objects are owned by this
 * runtime and use non-owning references to their dependencies.
 *
 * Call begin() once from Arduino setup(), then call loop() repeatedly from
 * Arduino loop().
 */
class ApplicationRuntime final {
public:
    ApplicationRuntime();

    ~ApplicationRuntime();

    ApplicationRuntime(const ApplicationRuntime&) = delete;
    ApplicationRuntime& operator=(const ApplicationRuntime&) = delete;

    /**
     * @brief Initializes the application.
     *
     * @return true when core services initialize successfully.
     *
     * A display, touch, LVGL, or radio initialization failure is reported
     * independently and does not change the existing startup policy.
     */
    bool begin();

    /**
     * @brief Services the active application mode.
     *
     * Normal mode processes radio observations and services LVGL.
     * Diagnostic mode polls touch and radio diagnostic workflows.
     *
     * Must be called repeatedly from the Arduino application task.
     */
    void loop();

    /** Returns whether core application services initialized successfully. */
    bool isReady() const;

private:
    // --------------------------------------------------------
    // Core capabilities and services
    // --------------------------------------------------------

    core::RadioCapabilities radioCapabilities_;

    core::CoreMemoryServices coreMemoryServices_;

    // Non-owning aliases. CoreMemoryServices owns these objects.
    core::DeviceInventory* deviceInventory_ = nullptr;
    core::WirelessCoordinator* wirelessCoordinator_ = nullptr;

    // --------------------------------------------------------
    // Radio subsystem
    // --------------------------------------------------------

    hardware::radio::WifiScanner wifiScanner_;

#ifdef WR_RUNTIME_HAS_BLUEDROID_BLE_SCANNER
    hardware::radio::BleScanner bleScanner_;
#endif

#ifdef WR_MODULAR_BUILD
    // Allocated after CoreMemoryServices has initialized.
    hardware::radio::RadioController* radioController_ = nullptr;
#endif

    // --------------------------------------------------------
    // Display hardware
    // --------------------------------------------------------

    hardware::display::DisplayConfig displayConfig_;
    hardware::display::LovyanGFXDisplay display_;
    hardware::display::DisplayManager displayManager_;

    // --------------------------------------------------------
    // Touch hardware
    // --------------------------------------------------------

    hardware::touch::TouchConfig touchConfig_;
    hardware::touch::FT6236Touch touch_;
    hardware::touch::TouchManager touchManager_;

    // --------------------------------------------------------
    // LVGL application
    // --------------------------------------------------------

    ui::lvgl::LVGLManager lvglManager_;

#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS

    // --------------------------------------------------------
    // Hardware diagnostics
    // --------------------------------------------------------

    diagnostics::DisplayDiagnostics displayDiagnostics_;
    diagnostics::TouchDiagnostics touchDiagnostics_;

    diagnostics::WifiDiagnostics* wifiDiagnostics_ = nullptr;

#ifdef WR_RUNTIME_HAS_BLUEDROID_BLE_SCANNER
    diagnostics::BleDiagnostics* bleDiagnostics_ = nullptr;
    bool bleDiagnosticsAttempted_ = false;
#endif

#endif

    // --------------------------------------------------------
    // Runtime state
    // --------------------------------------------------------

    bool ready_ = false;
    bool beginAttempted_ = false;
    bool wifiCompletionMemoryReported_ = false;

    // --------------------------------------------------------
    // Initialization
    // --------------------------------------------------------

    static core::RadioCapabilities makeRadioCapabilities();

    bool initializeCoreMemory();
    bool initializeDisplay();
    bool initializeTouch();
    bool initializeLVGL();
    bool initializeRadios();

    // --------------------------------------------------------
    // Runtime processing
    // --------------------------------------------------------

    void processRadios();

#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS
    bool initializeWifiDiagnostics();

#ifdef WR_RUNTIME_HAS_BLUEDROID_BLE_SCANNER
    void startBleDiagnosticsAfterWifi();
#endif
#endif
};

} // namespace wirelessrecon::application