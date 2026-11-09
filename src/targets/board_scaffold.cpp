#include <Arduino.h>

#include "hardware/BoardProfile.h"

#include "hardware/display/DisplayConfigFactory.h"
#include "hardware/display/DisplayManager.h"
#include "hardware/display/drivers/LovyanGFXDisplay.h"

using namespace wirelessrecon::hardware;
using namespace wirelessrecon::hardware::display;

namespace {

    DisplayConfig displayConfig;

    LovyanGFXDisplay display(displayConfig);

    DisplayManager displayManager(display);

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

        // Screen border
        screen.drawRect(
            2,
            2,
            screen.width() - 4,
            screen.height() - 4,
            Colors::White
        );

        // Color test bars
        const int16_t barWidth = screen.width() / 4;
        const int16_t barHeight = 30;
        const int16_t barY = 50;

        screen.fillRect(0, barY, barWidth, barHeight, Colors::Red);

        screen.fillRect(
            barWidth,
            barY,
            barWidth,
            barHeight,
            Colors::Green
        );

        screen.fillRect(
            barWidth * 2,
            barY,
            barWidth,
            barHeight,
            Colors::Blue
        );

        screen.fillRect(
            barWidth * 3,
            barY,
            screen.width() - (barWidth * 3),
            barHeight,
            Colors::Yellow
        );

        screen.drawText(
            20,
            100,
            "WirelessRecon",
            Colors::White,
            Colors::Black
        );

        screen.drawText(
            20,
            130,
            "Display abstraction OK",
            Colors::White,
            Colors::Black
        );

        Serial.println("[DISPLAY] Test pattern rendered.");
    }

} // namespace

void setup() {

    Serial.begin(115200);
    delay(500);

    printBoardInformation();

    if (kHasDisplay) {
        testDisplay();
    }
}

void loop() {
    delay(1000);
}