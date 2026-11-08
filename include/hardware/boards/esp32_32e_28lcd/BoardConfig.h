
#pragma once

static constexpr char kBoardName[] =
    "ELEGOO ESP32-32E N4 / 2.8-inch ILI9341 Display";

// ============================================================
// ESP32 capabilities
// ============================================================

static constexpr bool kHasWifi = true;
static constexpr bool kHasBle = true;
static constexpr bool kHasBluetoothClassic = true;
static constexpr bool kHasPsram = false;

// ============================================================
// Display capabilities
// ============================================================

static constexpr bool kHasDisplay = true;
static constexpr bool kHasTouch = true;
static constexpr bool kHasCapacitiveTouch = false;
static constexpr bool kHasResistiveTouch = true;

// ============================================================
// Display dimensions and controller
// ============================================================

static constexpr uint16_t kDisplayWidth = 240;
static constexpr uint16_t kDisplayHeight = 320;

static constexpr DisplayController kDisplayController =
    DisplayController::ILI9341;

static constexpr DisplayBus kDisplayBus =
    DisplayBus::SPI;

// ============================================================
// LCD GPIO configuration
// ============================================================

static constexpr int16_t kTftCs = 15;
static constexpr int16_t kTftDc = 2;

static constexpr int16_t kTftSclk = 14;
static constexpr int16_t kTftMosi = 13;
static constexpr int16_t kTftMiso = 12;

static constexpr int16_t kTftReset = -1;
static constexpr int16_t kTftBacklight = 21;

// LCD reset is shared with ESP32 EN.
static constexpr bool kTftResetSharedWithEn = true;

// Backlight is active HIGH.
static constexpr bool kTftBacklightActiveHigh = true;

// ============================================================
// Touch capabilities
// ============================================================

static constexpr TouchController kTouchController =
    TouchController::XPT2046;

static constexpr TouchBus kTouchBus =
    TouchBus::SPI;

// ============================================================
// Touch GPIO configuration
// ============================================================

static constexpr int16_t kTouchSclk = 25;
static constexpr int16_t kTouchMosi = 32;
static constexpr int16_t kTouchMiso = 39;

static constexpr int16_t kTouchCs = 33;
static constexpr int16_t kTouchInterrupt = 36;

// ============================================================
// SPI configuration
// ============================================================

static constexpr uint32_t kTftWriteFrequency = 40000000;
static constexpr uint32_t kTftReadFrequency = 16000000;

static constexpr uint32_t kTouchFrequency = 2500000;

// ============================================================
// Hardware documentation
// ============================================================

static constexpr char kDisplayNotes[] =
    "ILI9341V 240x320; XPT2046 resistive touch; "
    "LCD SPI GPIO 14/13/12; touch SPI GPIO 25/32/39; "
    "LCD reset shared with ESP32 EN";