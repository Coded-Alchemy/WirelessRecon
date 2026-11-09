
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
static constexpr DisplayBus kDisplayBus = DisplayBus::SPI;
static constexpr TouchBus kTouchBus = TouchBus::I2C;

// Hardware identification
static constexpr char kDisplayNotes[] =
    "ILI9488 display; FT6236 capacitive touch; "
    "existing Phase 7.1 baseline wiring";


// Confirmed from the working Phase 7.1 implementation.
static constexpr int16_t kTftSclk = 5;
static constexpr int16_t kTftMosi = 18;
static constexpr int16_t kTftMiso = 19;
static constexpr int16_t kTftCs = 15;
static constexpr int16_t kTftDc = 2;
static constexpr int16_t kTftReset = 4;
static constexpr int16_t kTftBacklight = 32;

static constexpr uint32_t kTftWriteFrequency = 20000000;
static constexpr uint32_t kTftReadFrequency = 16000000;

// ============================================================
// ILI9488 panel configuration
// ============================================================

static constexpr uint16_t kTftMemoryWidth = 320;
static constexpr uint16_t kTftMemoryHeight = 480;

static constexpr uint16_t kTftPanelWidth = 320;
static constexpr uint16_t kTftPanelHeight = 480;

static constexpr int16_t kTftOffsetX = 0;
static constexpr int16_t kTftOffsetY = 0;

static constexpr uint8_t kTftSpiMode = 0;

static constexpr uint8_t kTftDummyReadPixel = 8;
static constexpr uint8_t kTftDummyReadBits = 1;

static constexpr bool kTftReadable = false;
static constexpr bool kTftInvertColors = false;
static constexpr bool kTftRgbOrder = false;
static constexpr bool kTftBusShared = false;