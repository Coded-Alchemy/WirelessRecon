
#pragma once

namespace wirelessrecon::hardware {

    static constexpr char kBoardName[] = "ESP32-S3";

    // Wireless capabilities
    static constexpr bool kHasWifi = true;
    static constexpr bool kHasBle = true;
    static constexpr bool kHasBluetoothClassic = false;

    // Verify against the actual S3 module.
    static constexpr bool kHasPsram = true;

    // Display capabilities
    static constexpr bool kHasDisplay = false;
    static constexpr bool kHasTouch = false;
    static constexpr bool kHasCapacitiveTouch = false;
    static constexpr bool kHasResistiveTouch = false;

    static constexpr uint16_t kDisplayWidth = 0;
    static constexpr uint16_t kDisplayHeight = 0;

    static constexpr DisplayController kDisplayController =
        DisplayController::Unknown;

    static constexpr TouchController kTouchController =
        TouchController::None;

    static constexpr DisplayBus kDisplayBus = DisplayBus::Unknown;
    static constexpr TouchBus kTouchBus = TouchBus::Unknown;

    static constexpr char kDisplayNotes[] =
        "No display profile configured";

} // namespace wirelessrecon::hardware
