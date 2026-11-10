//
// This holds display-specific configuration separately from the application.
// Created by Taji Abdullah on 11/8/26.
//

#pragma once

#include <cstdint>
#include "hardware/HardwareTypes.h"

namespace wirelessrecon::hardware::display {

    struct DisplayConfig {

        // ========================================================
        // Controller and bus
        // ========================================================

        wirelessrecon::hardware::DisplayController controller =
            wirelessrecon::hardware::DisplayController::Unknown;

        wirelessrecon::hardware::DisplayBus bus =
            wirelessrecon::hardware::DisplayBus::Unknown;

        // ========================================================
        // Display dimensions
        // ========================================================

        uint16_t width = 0;
        uint16_t height = 0;

        uint16_t memoryWidth = 0;
        uint16_t memoryHeight = 0;

        uint16_t panelWidth = 0;
        uint16_t panelHeight = 0;

        int16_t offsetX = 0;
        int16_t offsetY = 0;

        // ========================================================
        // SPI GPIO
        // ========================================================

        int16_t pinSclk = -1;
        int16_t pinMosi = -1;
        int16_t pinMiso = -1;
        int16_t pinCs = -1;
        int16_t pinDc = -1;
        int16_t pinReset = -1;
        int16_t pinBacklight = -1;

        // ========================================================
        // SPI settings
        // ========================================================

        uint8_t spiMode = 0;

        uint32_t writeFrequency = 20000000;
        uint32_t readFrequency = 16000000;

        // ========================================================
        // Panel settings
        // ========================================================

        uint8_t dummyReadPixel = 8;
        uint8_t dummyReadBits = 1;
        uint8_t rotation = 0;

        bool readable = false;
        bool invertColors = false;
        bool rgbOrder = false;
        bool busShared = false;
        bool useDma = true;
    };

} // namespace wirelessrecon::hardware::display