//
// This holds display-specific configuration separately from the application.
// Created by Taji Abdullah on 11/8/26.
//


#pragma once

#include <cstdint>

namespace wirelessrecon::hardware::display {

    uint8_t spiMode = 0;

    uint16_t memoryWidth = 0;
    uint16_t memoryHeight = 0;

    uint16_t panelWidth = 0;
    uint16_t panelHeight = 0;

    int16_t offsetX = 0;
    int16_t offsetY = 0;

    uint8_t dummyReadPixel = 8;
    uint8_t dummyReadBits = 1;

    bool readable = false;
    bool rgbOrder = false;
    bool busShared = false;

    enum class DisplayController : uint8_t {
        Unknown,
        ILI9341,
        ILI9488
    };

    enum class DisplayBus : uint8_t {
        Unknown,
        SPI,
        Parallel
    };

    struct DisplayConfig {
        DisplayController controller = DisplayController::Unknown;
        DisplayBus bus = DisplayBus::Unknown;

        uint16_t width = 0;
        uint16_t height = 0;

        int16_t pinSclk = -1;
        int16_t pinMosi = -1;
        int16_t pinMiso = -1;
        int16_t pinCs = -1;
        int16_t pinDc = -1;
        int16_t pinReset = -1;
        int16_t pinBacklight = -1;

        uint32_t writeFrequency = 20000000;
        uint32_t readFrequency = 16000000;

        uint8_t rotation = 0;
        bool invertColors = false;
        bool useDma = true;
    };

} // namespace wirelessrecon::hardware::display
