
#include <Arduino.h>

#include <new>

#include "hardware/BoardProfile.h"

#include "core/CoreMemoryServices.h"
#include "core/DeviceInventory.h"
#include "core/WirelessCoordinator.h"

#include "hardware/radio/WifiScanner.h"

#ifdef WR_MODULAR_BUILD
#include "hardware/radio/RadioController.h"
#endif

#if defined(WR_BOARD_THINGPULSE_WROVER) || \
    defined(WR_BOARD_ESP32_32E)

#define WR_HAS_BLUEDROID_BLE_SCANNER

#include "hardware/radio/BleScanner.h"

#endif

#include "hardware/display/DisplayConfigFactory.h"
#include "hardware/display/DisplayManager.h"
#include "hardware/display/drivers/LovyanGFXDisplay.h"

#include "hardware/touch/TouchConfigFactory.h"
#include "hardware/touch/TouchManager.h"
#include "hardware/touch/drivers/FT6236Touch.h"

#include "ui/lvgl/LVGLManager.h"

#include "diagnostics/MemoryDiagnostics.h"

#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS

#include "diagnostics/BoardDiagnostics.h"
#include "diagnostics/DisplayDiagnostics.h"
#include "diagnostics/TouchDiagnostics.h"
#include "diagnostics/WifiDiagnostics.h"

#ifdef WR_HAS_BLUEDROID_BLE_SCANNER
#include "diagnostics/BleDiagnostics.h"
#endif

#endif

using namespace wirelessrecon::hardware;
using namespace wirelessrecon::hardware::display;
using namespace wirelessrecon::hardware::touch;

namespace {

// ============================================================
// Radio capabilities
// ============================================================

wirelessrecon::core::RadioCapabilities makeRadioCapabilities() {
    wirelessrecon::core::RadioCapabilities capabilities;

    capabilities.wifi = kHasWifi;
    capabilities.ble = kHasBle;
    capabilities.bluetoothClassic = kHasBluetoothClassic;

    // Original ESP32 BLE and Bluetooth Classic share a controller.
    capabilities.bluetoothControllerShared = true;

    return capabilities;
}

wirelessrecon::core::RadioCapabilities radioCapabilities =
    makeRadioCapabilities();

// ============================================================
// Core memory services
// ============================================================

// Owns ObservationQueue and manages the PSRAM inventory and
// WirelessCoordinator lifetimes.
wirelessrecon::core::CoreMemoryServices coreMemoryServices(
    radioCapabilities
);

// Non-owning aliases used by the application components.
wirelessrecon::core::DeviceInventory* deviceInventory = nullptr;

wirelessrecon::core::WirelessCoordinator* wirelessCoordinator =
    nullptr;

// ============================================================
// Radio subsystem
// ============================================================

wirelessrecon::hardware::radio::WifiScanner wifiScanner(
    coreMemoryServices.observationQueue()
);

#ifdef WR_HAS_BLUEDROID_BLE_SCANNER

wirelessrecon::hardware::radio::BleScanner bleScanner(
    coreMemoryServices.observationQueue()
);

#endif

#ifdef WR_MODULAR_BUILD

// Non-owning controller around the existing radio services.
wirelessrecon::hardware::radio::RadioController* radioController =
    nullptr;

#endif

// ============================================================
// Display hardware
// ============================================================

DisplayConfig displayConfig;
LovyanGFXDisplay display(displayConfig);
DisplayManager displayManager(display);

// ============================================================
// Touch hardware
// ============================================================

TouchConfig touchConfig;
FT6236Touch touch(touchConfig);
TouchManager touchManager(touch);

// ============================================================
// LVGL application
// ============================================================

wirelessrecon::ui::lvgl::LVGLManager lvglManager(
    displayManager,
    touchManager
);

// ============================================================
// Optional diagnostic instances
// ============================================================

#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS

wirelessrecon::diagnostics::DisplayDiagnostics displayDiagnostics(
    displayManager
);

wirelessrecon::diagnostics::TouchDiagnostics touchDiagnostics(
    touchManager
);

wirelessrecon::diagnostics::WifiDiagnostics* wifiDiagnostics = nullptr;

#ifdef WR_HAS_BLUEDROID_BLE_SCANNER

wirelessrecon::diagnostics::BleDiagnostics* bleDiagnostics = nullptr;

bool bleDiagnosticsAttempted = false;

#endif

#endif

// ============================================================
// Core memory initialization
// ============================================================

bool initializeCoreMemory() {
    if (!coreMemoryServices.begin()) {
        Serial.println(
            "[SYSTEM] Core memory services initialization failed."
        );
        return false;
    }

    // These pointers are non-owning. CoreMemoryServices controls
    // their allocation and lifetime.
    deviceInventory = coreMemoryServices.deviceInventory();
    wirelessCoordinator =
        coreMemoryServices.wirelessCoordinator();

    if (deviceInventory == nullptr ||
        wirelessCoordinator == nullptr) {
        Serial.println(
            "[MEMORY] Core services returned invalid references."
        );

        coreMemoryServices.shutdown();
        deviceInventory = nullptr;
        wirelessCoordinator = nullptr;

        return false;
    }

#ifdef WR_MODULAR_BUILD

    radioController =
        new (std::nothrow) wirelessrecon::hardware::radio::RadioController(
            *wirelessCoordinator,
            *deviceInventory,
            wifiScanner
#ifdef WR_HAS_BLUEDROID_BLE_SCANNER
            , &bleScanner
#else
            , nullptr
#endif
        );

    if (radioController == nullptr) {
        Serial.println(
            "[MEMORY] Failed to allocate RadioController."
        );

        coreMemoryServices.shutdown();
        deviceInventory = nullptr;
        wirelessCoordinator = nullptr;

        return false;
    }

#endif

    wirelessrecon::diagnostics::MemoryDiagnostics::printSnapshot(
        "Core initialized"
    );

    return true;
}

// ============================================================
// Display initialization
// ============================================================

bool initializeDisplay() {
    Serial.println("[DISPLAY] Initializing display...");

    if (!kHasDisplay) {
        Serial.println("[DISPLAY] Board has no display.");
        return false;
    }

    if (!makeDisplayConfig(displayConfig)) {
        Serial.println("[DISPLAY] Configuration unavailable.");
        return false;
    }

    if (!displayManager.begin()) {
        Serial.println("[DISPLAY] Initialization failed.");
        return false;
    }

    Serial.printf(
        "[DISPLAY] Ready: %u x %u\n",
        displayManager.display().width(),
        displayManager.display().height()
    );

    wirelessrecon::diagnostics::MemoryDiagnostics::printSnapshot(
        "Display initialized"
    );

    return true;
}

// ============================================================
// Touch initialization
// ============================================================

bool initializeTouch() {
    Serial.println("[TOUCH] Initializing touch...");

    if (!kHasTouch) {
        Serial.println("[TOUCH] Board has no touch controller.");
        return false;
    }

    if (!makeTouchConfig(touchConfig)) {
        Serial.println("[TOUCH] Configuration unavailable.");
        return false;
    }

    if (!touchManager.begin()) {
        Serial.println("[TOUCH] Initialization failed.");
        return false;
    }

    touchManager.setRotation(0);

    Serial.println("[TOUCH] Ready.");

    wirelessrecon::diagnostics::MemoryDiagnostics::printSnapshot(
        "Touch initialized"
    );

    return true;
}

// ============================================================
// LVGL initialization
// ============================================================

bool initializeLVGL() {
    if (!kHasDisplay || !kHasTouch) {
        Serial.println(
            "[LVGL] This target requires display and touch."
        );
        return false;
    }

    if (!displayManager.isInitialized() ||
        !touchManager.isInitialized()) {
        Serial.println(
            "[LVGL] Display or touch is not initialized."
        );
        return false;
    }

    Serial.println("[LVGL] Starting application...");

    if (wirelessCoordinator == nullptr ||
        deviceInventory == nullptr) {
        Serial.println(
            "[LVGL] Core application services are unavailable."
        );
        return false;
    }

    lvglManager.setApplicationContext(
        *wirelessCoordinator,
        *deviceInventory
    );

    if (!lvglManager.begin()) {
        Serial.println("[LVGL] Initialization failed.");
        return false;
    }

    Serial.println("[LVGL] Application ready.");

    wirelessrecon::diagnostics::MemoryDiagnostics::printSnapshot(
        "LVGL initialized"
    );

    return true;
}

// ============================================================
// Radio initialization
// ============================================================

bool initializeRadios() {
#ifdef WR_MODULAR_BUILD

    if (radioController == nullptr) {
        Serial.println("[RADIO] RadioController is unavailable.");
        return false;
    }

    if (!radioController->begin()) {
        return false;
    }

#else

    if (wirelessCoordinator == nullptr) {
        Serial.println("[RADIO] Coordinator is unavailable.");
        return false;
    }

    Serial.println("[RADIO] Initializing radio subsystem...");

    if (!wirelessCoordinator->registerScanner(wifiScanner)) {
        Serial.println("[RADIO] Wi-Fi scanner registration failed.");
        return false;
    }

    Serial.println("[RADIO] Wi-Fi scanner registered.");

#ifdef WR_HAS_BLUEDROID_BLE_SCANNER

    if (radioCapabilities.ble) {
        if (!wirelessCoordinator->registerScanner(bleScanner)) {
            Serial.println("[RADIO] BLE scanner registration failed.");
            return false;
        }

        Serial.println(
            "[RADIO] BLE scanner registered. "
            "Bluedroid remains uninitialized."
        );
    }

#endif

    const auto result = wirelessCoordinator->requestScan(
        wirelessrecon::core::RadioType::Wifi
    );

    if (result !=
        wirelessrecon::core::ScanRequestResult::Started) {
        Serial.printf(
            "[RADIO] Wi-Fi scan failed to start. Result: %u\n",
            static_cast<unsigned>(result)
        );
        return false;
    }

    Serial.println("[RADIO] Wi-Fi scan started.");

#endif

    wirelessrecon::diagnostics::MemoryDiagnostics::printSnapshot(
        "Radios initialized and Wi-Fi scan started"
    );

    return true;
}

// ============================================================
// Radio processing
// ============================================================

void processRadios() {
    if (wirelessCoordinator == nullptr ||
        deviceInventory == nullptr) {
        return;
    }

#ifdef WR_MODULAR_BUILD

    if (radioController == nullptr) {
        return;
    }

    radioController->process();

#else

    // Poll Wi-Fi before coordinator processing so completed results
    // are submitted before scanner lifecycle transitions are evaluated.
    wifiScanner.poll();
    wirelessCoordinator->process();

#endif

    static bool wifiCompletionMemoryReported = false;

    const auto wifiState = wifiScanner.state();

    if (wifiState ==
        wirelessrecon::core::ScannerState::Completed) {

        if (!wifiCompletionMemoryReported) {
            Serial.println("[RADIO] Wi-Fi scan completed.");

            wirelessrecon::diagnostics::MemoryDiagnostics::printSnapshot(
                "First Wi-Fi scan completed"
            );

            wifiCompletionMemoryReported = true;
        }
    }

    if (wifiState ==
        wirelessrecon::core::ScannerState::Failed) {
        Serial.println("[RADIO] Wi-Fi scan failed.");
    }
}

#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS

// ============================================================
// Wi-Fi diagnostic initialization
// ============================================================

bool initializeWifiDiagnostics() {
    if (wirelessCoordinator == nullptr ||
        deviceInventory == nullptr) {
        Serial.println(
            "[DIAGNOSTICS][WIFI] Core radio services unavailable."
        );
        return false;
    }

    wifiDiagnostics =
        new (std::nothrow) wirelessrecon::diagnostics::WifiDiagnostics(
            wifiScanner,
            *wirelessCoordinator,
            *deviceInventory
        );

    if (wifiDiagnostics == nullptr) {
        Serial.println(
            "[DIAGNOSTICS][WIFI] Failed to allocate diagnostics."
        );
        return false;
    }

    return wifiDiagnostics->begin();
}

#ifdef WR_HAS_BLUEDROID_BLE_SCANNER

// ============================================================
// BLE diagnostic initialization
// ============================================================

void startBleDiagnosticsAfterWifi() {
    if (bleDiagnosticsAttempted) {
        return;
    }

    // Do not start BLE until the Wi-Fi diagnostic scan has reached a
    // terminal state. This gives us a clean first coexistence test.
    if (wifiDiagnostics != nullptr &&
        !wifiDiagnostics->finished()) {
        return;
    }

    bleDiagnosticsAttempted = true;

    if (!radioCapabilities.ble) {
        Serial.println(
            "[DIAGNOSTICS][BLE] BLE is not supported by this board."
        );
        return;
    }

    bleDiagnostics =
        new (std::nothrow) wirelessrecon::diagnostics::BleDiagnostics(
            bleScanner,
            *wirelessCoordinator,
            *deviceInventory
        );

    if (bleDiagnostics == nullptr) {
        Serial.println(
            "[DIAGNOSTICS][BLE] Failed to allocate diagnostics."
        );
        return;
    }

    bleDiagnostics->begin();
}

#endif
#endif

} // namespace

// ============================================================
// Arduino entry points
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("================================");
    Serial.println(" WirelessRecon Modular Firmware");
    Serial.println("================================");

    wirelessrecon::diagnostics::MemoryDiagnostics::printSnapshot(
        "Startup baseline"
    );

#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS

    wirelessrecon::diagnostics::printBoardInformation();

#endif

    // Initialize the queue, PSRAM inventory, coordinator, and
    // modular radio controller before display and LVGL initialization.
    if (!initializeCoreMemory()) {
        Serial.println(
            "[SYSTEM] Core memory initialization failed."
        );
        return;
    }

    const bool displayReady = initializeDisplay();
    const bool touchReady = initializeTouch();

    if (!displayReady || !touchReady) {
        Serial.println(
            "[SYSTEM] Hardware initialization incomplete."
        );
        return;
    }

#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS

    // Diagnostic mode retains exclusive control of touch polling.
    displayDiagnostics.run();

    Serial.println();
    Serial.println(
        "[DIAGNOSTICS] Hardware diagnostic mode active."
    );
    Serial.println(
        "[DIAGNOSTICS] Touch the display to test coordinates."
    );

    // Wi-Fi diagnostics run first. BLE diagnostics begin after Wi-Fi ends.
    initializeWifiDiagnostics();

#else

    // Normal application mode.
    if (!initializeLVGL()) {
        Serial.println(
            "[SYSTEM] LVGL initialization failed."
        );
    }

    initializeRadios();

#endif
}

void loop() {
#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS

    touchDiagnostics.poll();

    if (wifiDiagnostics != nullptr) {
        wifiDiagnostics->poll();
    }

#ifdef WR_HAS_BLUEDROID_BLE_SCANNER

    startBleDiagnosticsAfterWifi();

    if (bleDiagnostics != nullptr) {
        bleDiagnostics->poll();
    }

#endif

    delay(20);

#else

    processRadios();

    if (lvglManager.isInitialized()) {
        lvglManager.handle();
    } else {
        delay(20);
    }

#endif
}
