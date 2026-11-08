
#pragma once

#include <cstdint>

namespace wirelessrecon::hardware {

    // ============================================================
    // Hardware types
    // ============================================================

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

    // ============================================================
    // Board selection
    // ============================================================

#if (defined(WR_BOARD_THINGPULSE_WROVER) + \
defined(WR_BOARD_ESP32_32E) + \
defined(WR_BOARD_ESP32_S3) + \
defined(WR_BOARD_ESP32_C3)) > 1

#error "Multiple WirelessRecon board profiles selected"

#elif defined(WR_BOARD_THINGPULSE_WROVER)

#include "boards/thingpulse_wrover/BoardConfig.h"

#elif defined(WR_BOARD_ESP32_32E)

#include "boards/esp32_32e_28lcd/BoardConfig.h"

#elif defined(WR_BOARD_ESP32_S3)

#include "boards/esp32_s3/BoardConfig.h"

#elif defined(WR_BOARD_ESP32_C3)

#include "boards/esp32_c3/BoardConfig.h"

#else

#error "No WirelessRecon board profile selected"

#endif

} // namespace wirelessrecon::hardware
