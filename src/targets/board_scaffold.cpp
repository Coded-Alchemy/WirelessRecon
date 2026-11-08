
#include <Arduino.h>
#include "hardware/BoardProfile.h"

using namespace wirelessrecon::hardware;

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("================================");
    Serial.println(" WirelessRecon Board Profile");
    Serial.println("================================");

    Serial.printf("Board: %s\n", kBoardName);

    Serial.printf(
        "WiFi: %s\n",
        kHasWifi ? "yes" : "no"
    );

    Serial.printf(
        "BLE: %s\n",
        kHasBle ? "yes" : "no"
    );

    Serial.printf(
        "Bluetooth Classic: %s\n",
        kHasBluetoothClassic ? "yes" : "no"
    );

    Serial.printf(
        "PSRAM: %s\n",
        kHasPsram ? "yes" : "no"
    );

    Serial.printf(
        "Display: %s\n",
        kHasDisplay ? "yes" : "no"
    );

    if (kHasDisplay) {
        Serial.printf(
            "Resolution: %u x %u\n",
            kDisplayWidth,
            kDisplayHeight
        );

        Serial.printf(
            "Touch: %s\n",
            kHasTouch ? "yes" : "no"
        );

        Serial.printf(
            "Resistive Touch: %s\n",
            kHasResistiveTouch ? "yes" : "no"
        );

        Serial.printf(
            "Display notes: %s\n",
            kDisplayNotes
        );
    }

    Serial.println("================================");
}

void loop() {
    delay(1000);
}
