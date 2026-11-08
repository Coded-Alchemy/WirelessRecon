//
// Created by dedlee on 11/8/26.
//

#include "hardware/display/LovyanGFXDisplay.h"

#include <Arduino.h>
#include <LovyanGFX.hpp>

namespace wirelessrecon::hardware::display {

// ============================================================
// Constructor
// ============================================================

LovyanGFXDisplay::LovyanGFXDisplay(const DisplayConfig& config)
    : config_(config) {
}

// ============================================================
// Initialize display
// ============================================================

bool LovyanGFXDisplay::begin() {

    // Validate required hardware configuration.
    if (config_.bus != DisplayBus::SPI ||
        config_.controller != DisplayController::ILI9488 ||
        config_.pinSclk < 0 ||
        config_.pinMosi < 0 ||
        config_.pinCs < 0 ||
        config_.pinDc < 0 ||
        config_.pinReset < 0) {

        initialized_ = false;
        return false;
    }

    configureBus();

    // Configure the ILI9488 panel.
    panel9488_.setBus(&bus_);

    lgfx::Panel_ILI9488::config_t panelConfig =
        panel9488_.config();

    panelConfig.pin_cs = config_.pinCs;
    panelConfig.pin_rst = config_.pinReset;
    panelConfig.pin_busy = -1;

    panelConfig.memory_width = config_.memoryWidth;
    panelConfig.memory_height = config_.memoryHeight;

    panelConfig.panel_width = config_.panelWidth;
    panelConfig.panel_height = config_.panelHeight;

    panelConfig.offset_x = config_.offsetX;
    panelConfig.offset_y = config_.offsetY;

    panelConfig.offset_rotation = 0;

    panelConfig.dummy_read_pixel = config_.dummyReadPixel;
    panelConfig.dummy_read_bits = config_.dummyReadBits;

    panelConfig.readable = config_.readable;
    panelConfig.invert = config_.invertColors;
    panelConfig.rgb_order = config_.rgbOrder;
    panelConfig.dlen_16bit = false;
    panelConfig.bus_shared = config_.busShared;

    panel9488_.config(panelConfig);

    // Attach the configured panel to the LovyanGFX device.
    device_.setPanel(&panel9488_);

    // Initialize the display.
    device_.init();

    // Configure rotation.
    device_.setRotation(config_.rotation);

    // Enable backlight.
    setBacklight(true);

    initialized_ = true;

    return true;
}

// ============================================================
// Configure SPI bus
// ============================================================

void LovyanGFXDisplay::configureBus() {

    lgfx::Bus_SPI::config_t busConfig = bus_.config();

    busConfig.spi_host = VSPI_HOST;

    busConfig.spi_mode = config_.spiMode;

    busConfig.freq_write = config_.writeFrequency;
    busConfig.freq_read = config_.readFrequency;

    busConfig.spi_3wire = false;
    busConfig.use_lock = true;

    busConfig.dma_channel = SPI_DMA_CH_AUTO;

    busConfig.pin_sclk = config_.pinSclk;
    busConfig.pin_mosi = config_.pinMosi;
    busConfig.pin_miso = config_.pinMiso;
    busConfig.pin_dc = config_.pinDc;

    bus_.config(busConfig);
}

// ============================================================
// Display dimensions
// ============================================================

uint16_t LovyanGFXDisplay::width() const {
    return device_.width();
}

uint16_t LovyanGFXDisplay::height() const {
    return device_.height();
}

// ============================================================
// Rotation
// ============================================================

void LovyanGFXDisplay::setRotation(uint8_t rotation) {

    if (!initialized_) {
        return;
    }

    device_.setRotation(rotation);
}

// ============================================================
// Clear display
// ============================================================

void LovyanGFXDisplay::clear(Color color) {

    if (!initialized_) {
        return;
    }

    device_.fillScreen(color);
}

// ============================================================
// Draw pixel
// ============================================================

void LovyanGFXDisplay::drawPixel(
    int16_t x,
    int16_t y,
    Color color
) {

    if (!initialized_) {
        return;
    }

    device_.drawPixel(x, y, color);
}

// ============================================================
// Draw line
// ============================================================

void LovyanGFXDisplay::drawLine(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    Color color
) {

    if (!initialized_) {
        return;
    }

    device_.drawLine(x0, y0, x1, y1, color);
}

// ============================================================
// Draw rectangle
// ============================================================

void LovyanGFXDisplay::drawRect(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    Color color
) {

    if (!initialized_) {
        return;
    }

    device_.drawRect(x, y, width, height, color);
}

// ============================================================
// Draw filled rectangle
// ============================================================

void LovyanGFXDisplay::fillRect(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    Color color
) {

    if (!initialized_) {
        return;
    }

    device_.fillRect(x, y, width, height, color);
}

// ============================================================
// Draw text
// ============================================================

void LovyanGFXDisplay::drawText(
    int16_t x,
    int16_t y,
    const char* text,
    Color foreground,
    Color background
) {

    if (!initialized_ || text == nullptr) {
        return;
    }

    device_.setTextColor(foreground, background);
    device_.drawString(text, x, y);
}

// ============================================================
// Backlight control
// ============================================================

void LovyanGFXDisplay::setBacklight(bool enabled) {

    if (config_.pinBacklight < 0) {
        return;
    }

    pinMode(config_.pinBacklight, OUTPUT);

    digitalWrite(
        config_.pinBacklight,
        enabled ? HIGH : LOW
    );
}

} // namespace wirelessrecon::hardware::display