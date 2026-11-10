//
// Created by dedlee on 11/9/26.
//

#pragma once

#include <cstddef>

#include "hardware/touch/ITouch.h"
#include "hardware/touch/TouchConfig.h"

namespace wirelessrecon::hardware::touch {

    class FT6236Touch final : public ITouch {
    public:
        explicit FT6236Touch(const TouchConfig& config);

        bool begin() override;
        bool isTouched() override;
        bool read(TouchPoint& point) override;
        void setRotation(uint8_t rotation) override;

    private:
        const TouchConfig& config_;

        bool initialized_ = false;
        uint8_t rotation_ = 0;

        bool readRegister(uint8_t reg, uint8_t* buffer, size_t length);
        bool readTouchStatus(uint8_t& count);
        void transformCoordinates(uint16_t& x, uint16_t& y) const;
    };

}
