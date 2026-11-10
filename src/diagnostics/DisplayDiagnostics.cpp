//
// Created by dedlee on 11/10/26.
//

#include <Arduino.h>

#include "diagnostics/DisplayDiagnostics.h"

namespace wirelessrecon::diagnostics {

    using namespace hardware::display;

    DisplayDiagnostics::DisplayDiagnostics(
        DisplayManager& displayManager
    )
        : displayManager_(displayManager) {
    }

    void DisplayDiagnostics::run() {

        Serial.println();
        Serial.println("[DIAGNOSTICS][DISPLAY] Running display test...");

        if (!displayManager_.isInitialized()) {
            Serial.println(
                "[DIAGNOSTICS][DISPLAY] Display is not initialized."
            );
            return;
        }

        IDisplay& screen = displayManager_.display();

        const int16_t screenWidth = screen.width();
        const int16_t screenHeight = screen.height();

        if (screenWidth == 0 || screenHeight == 0) {
            Serial.println(
                "[DIAGNOSTICS][DISPLAY] Invalid display dimensions."
            );
            return;
        }

        screen.clear(Colors::Black);

        screen.drawRect(
            2,
            2,
            screenWidth - 4,
            screenHeight - 4,
            Colors::White
        );

        const int16_t barWidth = screenWidth / 4;
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
            screenWidth - (barWidth * 3),
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

        Serial.println(
            "[DIAGNOSTICS][DISPLAY] Test pattern rendered."
        );
    }

}