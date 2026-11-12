#include <Arduino.h>
#include <esp32-hal-psram.h>

#include <new>

#include "hardware/BoardProfile.h"

#include "core/DeviceInventory.h"
#include "core/ObservationQueue.h"
#include "core/WirelessCoordinator.h"

#include "hardware/radio/WifiScanner.h"

#include "hardware/display/DisplayConfigFactory.h"
#include "hardware/display/DisplayManager.h"
#include "hardware/display/drivers/LovyanGFXDisplay.h"

#include "hardware/touch/TouchConfigFactory.h"
#include "hardware/touch/TouchManager.h"
#include "hardware/touch/drivers/FT6236Touch.h"

#include "ui/lvgl/LVGLManager.h"

#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS
#include "diagnostics/BoardDiagnostics.h"
#include "diagnostics/DisplayDiagnostics.h"
#include "diagnostics/TouchDiagnostics.h"
#include "diagnostics/WifiDiagnostics.h"
#endif

using namespace wirelessrecon::hardware;
using namespace wirelessrecon::hardware::display;
using namespace wirelessrecon::hardware::touch;

namespace {

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
// Radio subsystem
// ============================================================

// The queue and its mutex remain in internal RAM.
wirelessrecon::core::ObservationQueue observationQueue;

// The inventory is allocated in PSRAM during setup().
wirelessrecon::core::DeviceInventory* deviceInventory = nullptr;

// Construct the coordinator only after the inventory exists.
wirelessrecon::core::WirelessCoordinator* wirelessCoordinator = nullptr;

/**
 * Maps selected board capabilities into the portable core model.
 */
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

wirelessrecon::hardware::radio::WifiScanner wifiScanner(
    observationQueue
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

#endif

// ============================================================
// Core memory initialization
// ============================================================

/**
 * Creates the large historical device inventory in external PSRAM.
 *
 * DeviceInventory contains 150 Device objects, each of which includes
 * string fields. Keeping this storage out of internal DRAM leaves more
 * memory available for the display, LVGL, Wi-Fi, and Bluetooth stacks.
 *
 * The object is constructed only after Arduino has initialized PSRAM.
 */
bool initializeCoreMemory() {
    Serial.println("[MEMORY] Checking external PSRAM...");

    if (!psramFound()) {
        Serial.println("[MEMORY] PSRAM was not detected.");
        return false;
    }

    Serial.printf(
        "[MEMORY] PSRAM available: %u bytes\n",
        static_cast<unsigned>(ESP.getPsramSize())
    );

    void* inventoryMemory = ps_malloc(
        sizeof(wirelessrecon::core::DeviceInventory)
    );

    if (inventoryMemory == nullptr) {
        Serial.println(
            "[MEMORY] Failed to allocate DeviceInventory in PSRAM."
        );
        return false;
    }

    // Placement-new constructs the DeviceInventory in the PSRAM block.
    deviceInventory =
        new (inventoryMemory) wirelessrecon::core::DeviceInventory();

    wirelessCoordinator =
        new (std::nothrow) wirelessrecon::core::WirelessCoordinator(
            *deviceInventory,
            observationQueue,
            radioCapabilities
        );

    if (wirelessCoordinator == nullptr) {
        Serial.println(
            "[MEMORY] Failed to allocate WirelessCoordinator."
        );

        deviceInventory->~DeviceInventory();
        free(deviceInventory);
        deviceInventory = nullptr;

        return false;
    }

    Serial.printf(
        "[MEMORY] DeviceInventory allocated in PSRAM: %u bytes\n",
        static_cast<unsigned>(
            sizeof(wirelessrecon::core::DeviceInventory)
        )
    );

    Serial.println("[MEMORY] Core storage initialized.");

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

    if (!lvglManager.begin()) {
        Serial.println("[LVGL] Initialization failed.");
        return false;
    }

    Serial.println("[LVGL] Application ready.");

    return true;
}

// ============================================================
// Radio initialization
// ============================================================

bool initializeRadios() {
    if (wirelessCoordinator == nullptr) {
        Serial.println(
            "[RADIO] Coordinator is unavailable."
        );
        return false;
    }

    Serial.println("[RADIO] Initializing radio subsystem...");

    if (!wirelessCoordinator->registerScanner(wifiScanner)) {
        Serial.println(
            "[RADIO] Wi-Fi scanner registration failed."
        );
        return false;
    }

    Serial.println("[RADIO] Wi-Fi scanner registered.");

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

    // Poll the hardware adapter first so any final observations are
    // submitted before the coordinator evaluates scanner completion.
    wifiScanner.poll();

    // Capture terminal state before process() resets the scanner.
    const auto scannerState = wifiScanner.state();

    const std::size_t accepted = wirelessCoordinator->process();

    if (accepted > 0) {
        Serial.printf(
            "[RADIO] Accepted %u Wi-Fi observations. Inventory: %u\n",
            static_cast<unsigned>(accepted),
            static_cast<unsigned>(
                deviceInventory->count(
                    wirelessrecon::core::RadioType::Wifi
                )
            )
        );
    }

    if (scannerState ==
        wirelessrecon::core::ScannerState::Completed) {
        Serial.println("[RADIO] Wi-Fi scan completed.");
    }

    if (scannerState ==
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

#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS

    wirelessrecon::diagnostics::printBoardInformation();

#endif

    // Allocate the large inventory before initializing display and LVGL.
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