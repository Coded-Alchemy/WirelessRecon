//
// Created by dedlee on 11/9/26.
//


#include "hardware/touch/drivers/FT6236Touch.h"

#include <Arduino.h>
#include <Wire.h>

namespace wirelessrecon::hardware::touch {

namespace {
constexpr uint8_t kRegisterTouchStatus = 0x02;
constexpr uint8_t kRegisterPoint1XHigh = 0x03;
constexpr uint8_t kRegisterPoint1DataLength = 5;
}

FT6236Touch::FT6236Touch(const TouchConfig& config)
    : config_(config),
      rotation_(config.rotation) {
}

bool FT6236Touch::begin() {
    initialized_ = false;

    if (config_.controller != TouchController::FT6236 ||
        config_.bus != TouchBus::I2C ||
        config_.pinSda < 0 ||
        config_.pinScl < 0 ||
        config_.width == 0 ||
        config_.height == 0) {
        return false;
    }

    if (!Wire.begin(
            config_.pinSda,
            config_.pinScl,
            config_.i2cFrequency)) {
        return false;
    }

    Wire.beginTransmission(config_.i2cAddress);

    if (Wire.endTransmission() != 0) {
        return false;
    }

    initialized_ = true;
    return true;
}

bool FT6236Touch::isTouched() {
    if (!initialized_) {
        return false;
    }

    uint8_t count = 0;

    return readTouchStatus(count) && count > 0;
}

bool FT6236Touch::read(TouchPoint& point) {
    point = TouchPoint{};

    if (!initialized_) {
        return false;
    }

    uint8_t data[kRegisterPoint1DataLength] = {};

    if (!readRegister(
            kRegisterTouchStatus,
            data,
            sizeof(data))) {
        return false;
    }

    const uint8_t touchCount = data[0] & 0x0F;

    if (touchCount == 0) {
        return false;
    }

    uint16_t x =
        ((data[1] & 0x0F) << 8) | data[2];

    uint16_t y =
        ((data[3] & 0x0F) << 8) | data[4];

    if (x >= config_.width || y >= config_.height) {
        return false;
    }

    transformCoordinates(x, y);

    point.x = x;
    point.y = y;
    point.touched = true;

    return true;
}

void FT6236Touch::setRotation(uint8_t rotation) {
    rotation_ = rotation % 4;
}

bool FT6236Touch::readRegister(
    uint8_t reg,
    uint8_t* buffer,
    size_t length) {

    if (buffer == nullptr || length == 0) {
        return false;
    }

    Wire.beginTransmission(config_.i2cAddress);
    Wire.write(reg);

    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    const size_t received = Wire.requestFrom(
        config_.i2cAddress,
        static_cast<uint8_t>(length));

    if (received != length) {
        while (Wire.available()) {
            Wire.read();
        }
        return false;
    }

    for (size_t i = 0; i < length; ++i) {
        buffer[i] = Wire.read();
    }

    return true;
}

bool FT6236Touch::readTouchStatus(uint8_t& count) {
    uint8_t status = 0;

    if (!readRegister(kRegisterTouchStatus, &status, 1)) {
        count = 0;
        return false;
    }

    count = status & 0x0F;
    return true;
}

void FT6236Touch::transformCoordinates(
    uint16_t& x,
    uint16_t& y) const {

    // Apply optional hardware-specific coordinate corrections.
    if (config_.swapXY) {
        const uint16_t temp = x;
        x = y;
        y = temp;
    }

    if (config_.invertX) {
        x = config_.width - 1 - x;
    }

    if (config_.invertY) {
        y = config_.height - 1 - y;
    }

    // Apply display rotation.
    const uint16_t originalX = x;
    const uint16_t originalY = y;

    switch (rotation_) {
        case 1:
            x = config_.height - 1 - originalY;
            y = originalX;
            break;

        case 2:
            x = config_.width - 1 - originalX;
            y = config_.height - 1 - originalY;
            break;

        case 3:
            x = originalY;
            y = config_.width - 1 - originalX;
            break;

        default:
            break;
    }
}

}
