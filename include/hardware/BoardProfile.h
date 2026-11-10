#pragma once

#include "hardware/HardwareTypes.h"

namespace wirelessrecon::hardware {

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