#include <Arduino.h>

#include "hardware/BoardProfile.h"

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
#endif

using namespace wirelessrecon::hardware;
using namespace wirelessrecon::hardware::display;
using namespace wirelessrecon::hardware::touch;

namespace {

    // --------------------------------------------------------
    // Hardware instances
    // --------------------------------------------------------

    DisplayConfig displayConfig;
    LovyanGFXDisplay display(displayConfig);
    DisplayManager displayManager(display);

    TouchConfig touchConfig;
    FT6236Touch touch(touchConfig);
    TouchManager touchManager(touch);

    // --------------------------------------------------------
    // Application manager
    // --------------------------------------------------------

    wirelessrecon::ui::lvgl::LVGLManager lvglManager(
        displayManager,
        touchManager
    );

    // --------------------------------------------------------
    // Optional diagnostic instances
    // --------------------------------------------------------

#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS

    wirelessrecon::diagnostics::DisplayDiagnostics
        displayDiagnostics(displayManager);

    wirelessrecon::diagnostics::TouchDiagnostics
        touchDiagnostics(touchManager);

#endif

    // --------------------------------------------------------
    // Hardware initialization
    // --------------------------------------------------------

    bool initializeDisplay() {

        Serial.println("[DISPLAY] Initializing display...");

        if (!kHasDisplay) {
            Serial.println("[DISPLAY] Board has no display.");
            return false;
        }

        if (!makeDisplayConfig(displayConfig)) {
            Serial.println(
                "[DISPLAY] Configuration unavailable."
            );
            return false;
        }

        if (!displayManager.begin()) {
            Serial.println(
                "[DISPLAY] Initialization failed."
            );
            return false;
        }

        Serial.printf(
            "[DISPLAY] Ready: %u x %u\n",
            displayManager.display().width(),
            displayManager.display().height()
        );

        return true;
    }

    bool initializeTouch() {

        Serial.println("[TOUCH] Initializing touch...");

        if (!kHasTouch) {
            Serial.println("[TOUCH] Board has no touch controller.");
            return false;
        }

        if (!makeTouchConfig(touchConfig)) {
            Serial.println(
                "[TOUCH] Configuration unavailable."
            );
            return false;
        }

        if (!touchManager.begin()) {
            Serial.println(
                "[TOUCH] Initialization failed."
            );
            return false;
        }

        touchManager.setRotation(0);

        Serial.println("[TOUCH] Ready.");

        return true;
    }

    // --------------------------------------------------------
    // LVGL application initialization
    // --------------------------------------------------------

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
            Serial.println(
                "[LVGL] Initialization failed."
            );
            return false;
        }

        Serial.println("[LVGL] Application ready.");

        return true;
    }

}

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

    const bool displayReady = initializeDisplay();
    const bool touchReady = initializeTouch();

    if (!displayReady || !touchReady) {
        Serial.println(
            "[SYSTEM] Hardware initialization incomplete."
        );
        return;
    }

#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS

    // Diagnostic mode:
    // Test the display and leave touch polling to diagnostics.
    displayDiagnostics.run();

    Serial.println();
    Serial.println(
        "[DIAGNOSTICS] Hardware diagnostic mode active."
    );
    Serial.println(
        "[DIAGNOSTICS] Touch the display to test coordinates."
    );

#else

    // Normal application mode:
    // LVGL owns touch input and display rendering.
    initializeLVGL();

#endif
}

void loop() {

#ifdef WR_ENABLE_HARDWARE_DIAGNOSTICS

    touchDiagnostics.poll();
    delay(20);

#else

    if (lvglManager.isInitialized()) {
        lvglManager.handle();
    } else {
        delay(20);
    }

#endif
}