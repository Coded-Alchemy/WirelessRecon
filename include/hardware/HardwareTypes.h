//
// Created by dedlee on 11/8/26.
//

#pragma once

#include <cstdint>

namespace wirelessrecon::hardware {

    enum class DisplayController : uint8_t {
        Unknown,
        ILI9341,
        ILI9488
    };

    enum class TouchController : uint8_t {
        None,
        Unknown,
        FT6236,
        XPT2046
    };

    enum class DisplayBus : uint8_t {
        Unknown,
        SPI,
        Parallel
    };

    enum class TouchBus : uint8_t {
        Unknown,
        I2C,
        SPI
    };

} // namespace wirelessrecon::hardware
