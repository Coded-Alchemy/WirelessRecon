
#pragma once

static constexpr char kBoardName[] =
        "ThingPulse Color Kit Grande / ESP32-WROVER";

    // Wireless capabilities
    static constexpr bool kHasWifi = true;
    static constexpr bool kHasBle = true;
    static constexpr bool kHasBluetoothClassic = true;
    static constexpr bool kHasPsram = true;

    // Display capabilities
    static constexpr bool kHasDisplay = true;
    static constexpr bool kHasTouch = true;
    static constexpr bool kHasCapacitiveTouch = true;
    static constexpr bool kHasResistiveTouch = false;

    // Native display resolution
    static constexpr uint16_t kDisplayWidth = 320;
    static constexpr uint16_t kDisplayHeight = 480;

    // Hardware controllers
    static constexpr DisplayController kDisplayController =
        DisplayController::ILI9488;

    static constexpr TouchController kTouchController =
        TouchController::FT6236;

    // Interfaces
    static constexpr DisplayBus kDisplayBus = DisplayBus::Unknown;
    static constexpr TouchBus kTouchBus = TouchBus::I2C;

    // Hardware identification
    static constexpr char kDisplayNotes[] =
        "ILI9488 display; FT6236 capacitive touch; "
        "existing Phase 7.1 baseline wiring";

