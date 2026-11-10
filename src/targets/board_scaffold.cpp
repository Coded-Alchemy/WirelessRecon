
#include <Arduino.h>

#include "hardware/BoardProfile.h"

#include "hardware/display/DisplayConfigFactory.h"
#include "hardware/display/DisplayManager.h"
#include "hardware/display/drivers/LovyanGFXDisplay.h"

#include "hardware/touch/TouchConfigFactory.h"
#include "hardware/touch/TouchManager.h"
#include "hardware/touch/drivers/FT6236Touch.h"

#include "ui/lvgl/LVGLManager.h"

using namespace wirelessrecon::hardware;
using namespace wirelessrecon::hardware::display;
using namespace wirelessrecon::hardware::touch;

namespace {

    // --------------------------------------------------------
    // Display components
    // --------------------------------------------------------

    DisplayConfig displayConfig;
    LovyanGFXDisplay display(displayConfig);
    DisplayManager displayManager(display);

    // --------------------------------------------------------
    // Touch components
    // --------------------------------------------------------

    TouchConfig touchConfig;
    FT6236Touch touch(touchConfig);
    TouchManager touchManager(touch);

    // LVGL application manager
    wirelessrecon::ui::lvgl::LVGLManager lvglManager(
        displayManager,
        touchManager
    );

    // Touch diagnostic state
    bool previousTouchState = false;
    uint16_t previousX = 0;
    uint16_t previousY = 0;

    // --------------------------------------------------------
    // Board information
    // --------------------------------------------------------

    void printBoardInformation() {

        Serial.println();
        Serial.println("================================");
        Serial.println(" WirelessRecon Board Profile");
        Serial.println("================================");

        Serial.printf("Board: %s\n", kBoardName);
        Serial.printf("WiFi: %s\n", kHasWifi ? "yes" : "no");
        Serial.printf("BLE: %s\n", kHasBle ? "yes" : "no");

        Serial.printf(
            "Bluetooth Classic: %s\n",
            kHasBluetoothClassic ? "yes" : "no"
        );

        Serial.printf("PSRAM: %s\n", kHasPsram ? "yes" : "no");
        Serial.printf("Display: %s\n", kHasDisplay ? "yes" : "no");

        if (kHasDisplay) {
            Serial.printf(
                "Resolution: %u x %u\n",
                kDisplayWidth,
                kDisplayHeight
            );

            Serial.printf("Touch: %s\n", kHasTouch ? "yes" : "no");

            Serial.printf(
                "Resistive Touch: %s\n",
                kHasResistiveTouch ? "yes" : "no"
            );

            Serial.printf("Display notes: %s\n", kDisplayNotes);
        }

        Serial.println("================================");
    }

    // --------------------------------------------------------
    // Display test
    // --------------------------------------------------------

    void testDisplay() {

        Serial.println();
        Serial.println("[DISPLAY] Starting display initialization...");

        if (!makeDisplayConfig(displayConfig)) {
            Serial.println("[DISPLAY] Configuration unavailable.");
            return;
        }

        if (!displayManager.begin()) {
            Serial.println("[DISPLAY] Initialization failed.");
            return;
        }

        IDisplay& screen = displayManager.display();

        Serial.println("[DISPLAY] Initialization successful.");

        screen.clear(Colors::Black);

        screen.drawRect(
            2,
            2,
            screen.width() - 4,
            screen.height() - 4,
            Colors::White
        );

        const int16_t barWidth = screen.width() / 4;
        const int16_t barHeight = 30;
        const int16_t barY = 50;

        screen.fillRect(
            0, barY, barWidth, barHeight, Colors::Red
        );

        screen.fillRect(
            barWidth, barY, barWidth, barHeight, Colors::Green
        );

        screen.fillRect(
            barWidth * 2, barY, barWidth, barHeight, Colors::Blue
        );

        screen.fillRect(
            barWidth * 3,
            barY,
            screen.width() - (barWidth * 3),
            barHeight,
            Colors::Yellow
        );

        screen.drawText(
            20, 100, "WirelessRecon",
            Colors::White, Colors::Black
        );

        screen.drawText(
            20, 130, "Display abstraction OK",
            Colors::White, Colors::Black
        );

        Serial.println("[DISPLAY] Test pattern rendered.");
    }

    // --------------------------------------------------------
    // Touch initialization
    // --------------------------------------------------------

    void testTouch() {

        Serial.println();
        Serial.println("[TOUCH] Starting touch initialization...");

        if (!kHasTouch) {
            Serial.println("[TOUCH] Board has no touch controller.");
            return;
        }

        if (!makeTouchConfig(touchConfig)) {
            Serial.println("[TOUCH] Configuration unavailable.");
            return;
        }

        Serial.printf(
            "[TOUCH] Controller: FT6236\n"
            "[TOUCH] SDA: %d\n"
            "[TOUCH] SCL: %d\n"
            "[TOUCH] I2C address: 0x%02X\n"
            "[TOUCH] Resolution: %u x %u\n",
            touchConfig.pinSda,
            touchConfig.pinScl,
            touchConfig.i2cAddress,
            touchConfig.width,
            touchConfig.height
        );

        if (!touchManager.begin()) {
            Serial.println(
                "[TOUCH] Initialization failed. "
                "Check I2C wiring and controller response."
            );
            return;
        }

        touchManager.setRotation(0);

        Serial.println("[TOUCH] Initialization successful.");
        Serial.println("[TOUCH] Touch the screen to test coordinates.");
    }

    // --------------------------------------------------------
    // LVGL initialization
    // --------------------------------------------------------

    void testLVGL() {
        if (!kHasDisplay || !kHasTouch) {
            Serial.println(
                "[LVGL] This Phase 4 target requires display and touch."
            );
            return;
        }

        if (!displayManager.isInitialized() ||
            !touchManager.isInitialized()) {
            Serial.println(
                "[LVGL] Display or touch hardware is not initialized."
            );
            return;
        }

        if (!lvglManager.begin()) {
            Serial.println("[LVGL] Initialization failed.");
            return;
        }

        Serial.println("[LVGL] Test interface is running.");
    }

    // --------------------------------------------------------
    // Touch polling
    // --------------------------------------------------------

    void pollTouch() {

        if (!touchManager.isInitialized()) {
            return;
        }

        TouchPoint point;

        const bool touched = touchManager.read(point);

        if (touched && point.touched) {

            // Print only when touch begins or coordinates change.
            if (!previousTouchState ||
                point.x != previousX ||
                point.y != previousY) {

                Serial.printf(
                    "[TOUCH] X: %u | Y: %u\n",
                    point.x,
                    point.y
                );
            }

            previousX = point.x;
            previousY = point.y;
            previousTouchState = true;

        } else if (previousTouchState) {

            Serial.println("[TOUCH] Released.");
            previousTouchState = false;
        }
    }
}

// ============================================================
// Arduino entry points
// ============================================================

void setup() {

    Serial.begin(115200);
    delay(500);

    printBoardInformation();

    if (kHasDisplay) {
        testDisplay();
    }

    if (kHasTouch) {
        testTouch();
    }

    testLVGL();
}

void loop() {
    if (lvglManager.isInitialized()) {
        lvglManager.handle();
        return;
    }

    pollTouch();
    delay(20);
}
