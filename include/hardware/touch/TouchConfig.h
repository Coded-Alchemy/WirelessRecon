//
// Created by dedlee on 11/9/26.
//

#pragma once

#include <cstdint>
#include "hardware/HardwareTypes.h"

namespace wirelessrecon::hardware::touch {

    struct TouchConfig {
        TouchController controller = TouchController::Unknown;
        TouchBus bus = TouchBus::Unknown;

        int16_t pinSda = -1;
        int16_t pinScl = -1;

        uint8_t i2cAddress = 0x38;
        uint32_t i2cFrequency = 400000;

        uint16_t width = 0;
        uint16_t height = 0;

        uint8_t rotation = 0;

        bool swapXY = false;
        bool invertX = false;
        bool invertY = false;
    };

}
