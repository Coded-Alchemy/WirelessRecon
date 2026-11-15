//
// Created by dedlee on 11/15/26.
//

/**
 * @file ApplicationRuntime.cpp
 * @brief Implements application composition, startup, and runtime servicing.
 *
 * ApplicationRuntime is responsible for constructing and coordinating the
 * board-selected application components. It does not reimplement scanner,
 * inventory, display, touch, or LVGL behavior. Those responsibilities remain
 * in their respective subsystem classes.
 *
 * Startup sequence:
 *
 *   Arduino setup()
 *       |
 *       v
 *   ApplicationRuntime::begin()
 *       |
 *       +--> CoreMemoryServices
 *       |       +--> ObservationQueue
 *       |       +--> DeviceInventory (PSRAM)
 *       |       +--> WirelessCoordinator
 *       |
 *       +--> DisplayManager
 *       |
 *       +--> TouchManager
 *       |
 *       +--> Normal mode
 *       |       +--> LVGLManager
 *       |       +--> RadioController
 *       |               +--> Register Wi-Fi
 *       |               +--> Register BLE when supported
 *       |               +--> Start Wi-Fi
 *       |
 *       +--> Diagnostic mode
 *               +--> Display diagnostics
 *               +--> Wi-Fi diagnostics
 *               +--> BLE diagnostics after Wi-Fi terminates
 *
 * Normal runtime:
 *
 *   Arduino loop()
 *       |
 *       v
 *   ApplicationRuntime::loop()
 *       |
 *       +--> Radio processing
 *       |
 *       +--> LVGLManager::handle()
 *
 * Diagnostic runtime:
 *
 *   Arduino loop()
 *       |
 *       v
 *   ApplicationRuntime::loop()
 *       |
 *       +--> Touch diagnostics
 *       +--> Wi-Fi diagnostic polling
 *       +--> BLE diagnostic sequencing and polling
 *
 * Threading:
 * - begin() and loop() execute from the Arduino application task.
 * - LVGL is serviced only through LVGLManager::handle().
 * - Radio callbacks continue to submit observations through ObservationQueue.
 *
 * Ownership:
 * - CoreMemoryServices owns the observation queue, inventory, and coordinator.
 * - ApplicationRuntime owns the hardware adapters and managers.
 * - RadioController and diagnostic objects hold non-owning references.
 */

#include "application/ApplicationRuntime.h"

#include <Arduino.h>

#include <new>

#include "diagnostics/MemoryDiagnostics.h"

namespace wirelessrecon::application {

using core::RadioType;
using core::ScanRequestResult;
using hardware::display::makeDisplayConfig;
using hardware::touch::makeTouchConfig;

// ============================================================
// Construction and destruction
// ============================================================

ApplicationRuntime::ApplicationRuntime()
    : radioCapabilities_(makeRadioCapabilities()),
      coreMemoryServices_(radioCapabilities_),
      wifiScanner_(coreMemoryServices_.observationQueue())
#ifdef WR_RUNTIME_HAS_BLUEDROID_BLE_SCANNER
      ,
      bleScanner_(coreMemoryServices_.observationQueue())
#endif
      ,
      display_(displayConfig_),
      displayManager_(display_),
      touch_(touchConfig_),
      touchManager_(touch_),
      lvglManager_(displayManager_, touchManager_)
#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS
      ,
      displayDiagnostics_(displayManager_),
      touchDiagnostics_(touchManager_)
#endif
{
}

ApplicationRuntime::~ApplicationRuntime() {
#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS

    // Diagnostic objects hold references to core services and scanners.
    // Destroy them before CoreMemoryServices releases those services.
#ifdef WR_RUNTIME_HAS_BLUEDROID_BLE_SCANNER
    delete bleDiagnostics_;
    bleDiagnostics_ = nullptr;
#endif

    delete wifiDiagnostics_;
    wifiDiagnostics_ = nullptr;

#endif

#ifdef WR_MODULAR_BUILD

    // RadioController also holds non-owning references to core services.
    delete radioController_;
    radioController_ = nullptr;

#endif

    // CoreMemoryServices' destructor also calls shutdown(). Calling it here
    // ensures dependent controller/diagnostic objects are already destroyed.
    coreMemoryServices_.shutdown();

    deviceInventory_ = nullptr;
    wirelessCoordinator_ = nullptr;
}

// ============================================================
// Board capabilities
// ============================================================

core::RadioCapabilities ApplicationRuntime::makeRadioCapabilities() {
    core::RadioCapabilities capabilities;

    capabilities.wifi = hardware::kHasWifi;
    capabilities.ble = hardware::kHasBle;
    capabilities.bluetoothClassic = hardware::kHasBluetoothClassic;

    // Preserve the existing original-ESP32 shared-controller declaration.
    capabilities.bluetoothControllerShared = true;

    return capabilities;
}

// ============================================================
// Application startup
// ============================================================

bool ApplicationRuntime::begin() {
    if (beginAttempted_) {
        return ready_;
    }

    beginAttempted_ = true;

    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("================================");
    Serial.println(" WirelessRecon Modular Firmware");
    Serial.println("================================");

    diagnostics::MemoryDiagnostics::printSnapshot(
        "Startup baseline"
    );

#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS

    diagnostics::printBoardInformation();

#endif

    // Core services must exist before scanners, controllers, or the UI
    // receive references to the coordinator and inventory.
    if (!initializeCoreMemory()) {
        Serial.println(
            "[SYSTEM] Core memory initialization failed."
        );
        return false;
    }

    ready_ = true;

    // Preserve the established hardware initialization sequence.
    const bool displayReady = initializeDisplay();
    const bool touchReady = initializeTouch();

    if (!displayReady || !touchReady) {
        Serial.println(
            "[SYSTEM] Hardware initialization incomplete."
        );
        return true;
    }

#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS

    // Diagnostic mode retains exclusive control of touch polling.
    displayDiagnostics_.run();

    Serial.println();
    Serial.println(
        "[DIAGNOSTICS] Hardware diagnostic mode active."
    );
    Serial.println(
        "[DIAGNOSTICS] Touch the display to test coordinates."
    );

    // Wi-Fi diagnostics run first. BLE begins after Wi-Fi terminates.
    initializeWifiDiagnostics();

#else

    // Normal application mode.
    if (!initializeLVGL()) {
        Serial.println(
            "[SYSTEM] LVGL initialization failed."
        );
    }

    // Preserve existing behavior: radios are initialized after LVGL.
    initializeRadios();

#endif

    return true;
}

// ============================================================
// Core memory initialization
// ============================================================

bool ApplicationRuntime::initializeCoreMemory() {
    if (!coreMemoryServices_.begin()) {
        Serial.println(
            "[SYSTEM] Core memory services initialization failed."
        );
        return false;
    }

    // CoreMemoryServices owns these objects. Runtime and subsystem
    // components use non-owning pointers to them.
    deviceInventory_ = coreMemoryServices_.deviceInventory();
    wirelessCoordinator_ =
        coreMemoryServices_.wirelessCoordinator();

    if (deviceInventory_ == nullptr ||
        wirelessCoordinator_ == nullptr) {
        Serial.println(
            "[MEMORY] Core services returned invalid references."
        );

        coreMemoryServices_.shutdown();
        deviceInventory_ = nullptr;
        wirelessCoordinator_ = nullptr;

        return false;
    }

#ifdef WR_MODULAR_BUILD

    radioController_ =
        new (std::nothrow) hardware::radio::RadioController(
            *wirelessCoordinator_,
            *deviceInventory_,
            wifiScanner_
#ifdef WR_RUNTIME_HAS_BLUEDROID_BLE_SCANNER
            , &bleScanner_
#else
            , nullptr
#endif
        );

    if (radioController_ == nullptr) {
        Serial.println(
            "[MEMORY] Failed to allocate RadioController."
        );

        coreMemoryServices_.shutdown();
        deviceInventory_ = nullptr;
        wirelessCoordinator_ = nullptr;

        return false;
    }

#endif

    diagnostics::MemoryDiagnostics::printSnapshot(
        "Core initialized"
    );

    return true;
}

// ============================================================
// Display initialization
// ============================================================

bool ApplicationRuntime::initializeDisplay() {
    Serial.println("[DISPLAY] Initializing display...");

    if (!hardware::kHasDisplay) {
        Serial.println("[DISPLAY] Board has no display.");
        return false;
    }

    if (!makeDisplayConfig(displayConfig_)) {
        Serial.println("[DISPLAY] Configuration unavailable.");
        return false;
    }

    if (!displayManager_.begin()) {
        Serial.println("[DISPLAY] Initialization failed.");
        return false;
    }

    Serial.printf(
        "[DISPLAY] Ready: %u x %u\n",
        displayManager_.display().width(),
        displayManager_.display().height()
    );

    diagnostics::MemoryDiagnostics::printSnapshot(
        "Display initialized"
    );

    return true;
}

// ============================================================
// Touch initialization
// ============================================================

bool ApplicationRuntime::initializeTouch() {
    Serial.println("[TOUCH] Initializing touch...");

    if (!hardware::kHasTouch) {
        Serial.println("[TOUCH] Board has no touch controller.");
        return false;
    }

    if (!makeTouchConfig(touchConfig_)) {
        Serial.println("[TOUCH] Configuration unavailable.");
        return false;
    }

    if (!touchManager_.begin()) {
        Serial.println("[TOUCH] Initialization failed.");
        return false;
    }

    touchManager_.setRotation(0);

    Serial.println("[TOUCH] Ready.");

    diagnostics::MemoryDiagnostics::printSnapshot(
        "Touch initialized"
    );

    return true;
}

// ============================================================
// LVGL initialization
// ============================================================

bool ApplicationRuntime::initializeLVGL() {
    if (!hardware::kHasDisplay || !hardware::kHasTouch) {
        Serial.println(
            "[LVGL] This target requires display and touch."
        );
        return false;
    }

    if (!displayManager_.isInitialized() ||
        !touchManager_.isInitialized()) {
        Serial.println(
            "[LVGL] Display or touch is not initialized."
        );
        return false;
    }

    Serial.println("[LVGL] Starting application...");

    if (wirelessCoordinator_ == nullptr ||
        deviceInventory_ == nullptr) {
        Serial.println(
            "[LVGL] Core application services are unavailable."
        );
        return false;
    }

    lvglManager_.setApplicationContext(
        *wirelessCoordinator_,
        *deviceInventory_
    );

    if (!lvglManager_.begin()) {
        Serial.println("[LVGL] Initialization failed.");
        return false;
    }

    Serial.println("[LVGL] Application ready.");

    diagnostics::MemoryDiagnostics::printSnapshot(
        "LVGL initialized"
    );

    return true;
}

// ============================================================
// Radio initialization
// ============================================================

bool ApplicationRuntime::initializeRadios() {
#ifdef WR_MODULAR_BUILD

    if (radioController_ == nullptr) {
        Serial.println("[RADIO] RadioController is unavailable.");
        return false;
    }

    if (!radioController_->begin()) {
        return false;
    }

#else

    if (wirelessCoordinator_ == nullptr) {
        Serial.println("[RADIO] Coordinator is unavailable.");
        return false;
    }

    Serial.println("[RADIO] Initializing radio subsystem...");

    if (!wirelessCoordinator_->registerScanner(wifiScanner_)) {
        Serial.println("[RADIO] Wi-Fi scanner registration failed.");
        return false;
    }

    Serial.println("[RADIO] Wi-Fi scanner registered.");

#ifdef WR_RUNTIME_HAS_BLUEDROID_BLE_SCANNER

    if (radioCapabilities_.ble) {
        if (!wirelessCoordinator_->registerScanner(bleScanner_)) {
            Serial.println("[RADIO] BLE scanner registration failed.");
            return false;
        }

        Serial.println(
            "[RADIO] BLE scanner registered. "
            "Bluedroid remains uninitialized."
        );
    }

#endif

    const auto result = wirelessCoordinator_->requestScan(
        RadioType::Wifi
    );

    if (result != ScanRequestResult::Started) {
        Serial.printf(
            "[RADIO] Wi-Fi scan failed to start. Result: %u\n",
            static_cast<unsigned>(result)
        );
        return false;
    }

    Serial.println("[RADIO] Wi-Fi scan started.");

#endif

    diagnostics::MemoryDiagnostics::printSnapshot(
        "Radios initialized and Wi-Fi scan started"
    );

    return true;
}

// ============================================================
// Normal-mode radio processing
// ============================================================

void ApplicationRuntime::processRadios() {
    if (wirelessCoordinator_ == nullptr ||
        deviceInventory_ == nullptr) {
        return;
    }

#ifdef WR_MODULAR_BUILD

    if (radioController_ == nullptr) {
        return;
    }

    radioController_->process();

#else

    // Poll Wi-Fi before coordinator processing so completed results are
    // submitted before scanner lifecycle transitions are evaluated.
    wifiScanner_.poll();
    wirelessCoordinator_->process();

#endif

    const auto wifiState = wifiScanner_.state();

    if (wifiState == core::ScannerState::Completed) {
        if (!wifiCompletionMemoryReported_) {
            Serial.println("[RADIO] Wi-Fi scan completed.");

            diagnostics::MemoryDiagnostics::printSnapshot(
                "First Wi-Fi scan completed"
            );

            wifiCompletionMemoryReported_ = true;
        }
    }

    if (wifiState == core::ScannerState::Failed) {
        Serial.println("[RADIO] Wi-Fi scan failed.");
    }
}

// ============================================================
// Hardware diagnostic initialization and sequencing
// ============================================================

#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS

bool ApplicationRuntime::initializeWifiDiagnostics() {
    if (wirelessCoordinator_ == nullptr ||
        deviceInventory_ == nullptr) {
        Serial.println(
            "[DIAGNOSTICS][WIFI] Core radio services unavailable."
        );
        return false;
    }

    wifiDiagnostics_ =
        new (std::nothrow) diagnostics::WifiDiagnostics(
            wifiScanner_,
            *wirelessCoordinator_,
            *deviceInventory_
        );

    if (wifiDiagnostics_ == nullptr) {
        Serial.println(
            "[DIAGNOSTICS][WIFI] Failed to allocate diagnostics."
        );
        return false;
    }

    return wifiDiagnostics_->begin();
}

#ifdef WR_RUNTIME_HAS_BLUEDROID_BLE_SCANNER

void ApplicationRuntime::startBleDiagnosticsAfterWifi() {
    if (bleDiagnosticsAttempted_) {
        return;
    }

    // Do not start BLE until the Wi-Fi diagnostic scan has reached a
    // terminal state. This preserves the original coexistence test order.
    if (wifiDiagnostics_ != nullptr &&
        !wifiDiagnostics_->finished()) {
        return;
    }

    bleDiagnosticsAttempted_ = true;

    if (!radioCapabilities_.ble) {
        Serial.println(
            "[DIAGNOSTICS][BLE] BLE is not supported by this board."
        );
        return;
    }

    bleDiagnostics_ =
        new (std::nothrow) diagnostics::BleDiagnostics(
            bleScanner_,
            *wirelessCoordinator_,
            *deviceInventory_
        );

    if (bleDiagnostics_ == nullptr) {
        Serial.println(
            "[DIAGNOSTICS][BLE] Failed to allocate diagnostics."
        );
        return;
    }

    bleDiagnostics_->begin();
}

#endif
#endif

// ============================================================
// Application task
// ============================================================

void ApplicationRuntime::loop() {
    if (!ready_) {
        delay(20);
        return;
    }

#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS

    touchDiagnostics_.poll();

    if (wifiDiagnostics_ != nullptr) {
        wifiDiagnostics_->poll();
    }

#ifdef WR_RUNTIME_HAS_BLUEDROID_BLE_SCANNER

    startBleDiagnosticsAfterWifi();

    if (bleDiagnostics_ != nullptr) {
        bleDiagnostics_->poll();
    }

#endif

    delay(20);

#else

    processRadios();

    if (lvglManager_.isInitialized()) {
        // This remains the sole regular LVGL service point.
        lvglManager_.handle();
    } else {
        delay(20);
    }

#endif
}

bool ApplicationRuntime::isReady() const {
    return ready_;
}

} // namespace wirelessrecon::application