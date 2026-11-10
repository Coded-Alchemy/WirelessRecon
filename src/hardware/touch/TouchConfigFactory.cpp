//
// Created by dedlee on 11/9/26.
//


#include "hardware/touch/TouchConfigFactory.h"
#include "hardware/BoardProfile.h"

namespace wirelessrecon::hardware::touch {

    bool makeTouchConfig(TouchConfig& config) {

        config = TouchConfig{};

#if defined(WR_BOARD_THINGPULSE_WROVER)

        if (!kHasTouch ||
            kTouchController != TouchController::FT6236 ||
            kTouchBus != TouchBus::I2C) {
            return false;
            }

        config.controller = kTouchController;
        config.bus = kTouchBus;

        config.pinSda = kTouchSda;
        config.pinScl = kTouchScl;

        config.i2cAddress = kTouchI2cAddress;
        config.i2cFrequency = kTouchI2cFrequency;

        config.width = kTouchWidth;
        config.height = kTouchHeight;

        config.rotation = 0;

        config.swapXY = false;
        config.invertX = false;
        config.invertY = false;

        return true;

#else

        return false;

#endif
    }

}
