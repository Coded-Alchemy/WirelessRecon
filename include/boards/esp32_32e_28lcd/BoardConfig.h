
#pragma once

namespace wirelessrecon::hardware {

    static constexpr char kBoardName[] =
        "ELEGOO ESP32-32E N4 / 2.8-inch display";

    // Wireless capabilities
    static constexpr bool kHasWifi = true;
    static constexpr bool kHasBle = true;
    static constexpr bool kHasBluetoothClassic = true;
    static constexpr bool kHasPsram = false;

    // Display capabilities
    static constexpr bool kHasDisplay = true;
    static constexpr bool kHasTouch = true;
    static constexpr bool kHasCapacitiveTouch = false;
    static constexpr bool kHasResistiveTouch = true;

    // Native display resolution
    static constexpr uint16_t kDisplayWidth = 240;
    static constexpr uint16_t kDisplayHeight = 320;

    // Hardware controllers
    static constexpr DisplayController kDisplayController =
        DisplayController::ILI9341;

    // Touch controller not yet verified against the exact PCB revision.
    static constexpr TouchController kTouchController =
        TouchController::Unknown;

    // Interfaces not yet verified against the exact PCB revision.
    static constexpr DisplayBus kDisplayBus = DisplayBus::Unknown;
    static constexpr TouchBus kTouchBus = TouchBus::Unknown;

    static constexpr char kDisplayNotes[] =
        "ILI9341 reported by product description; "
        "resistive touch; controller and GPIO mapping require verification";

} // namespace wirelessrecon::hardware
