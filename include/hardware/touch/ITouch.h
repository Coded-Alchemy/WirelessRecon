//
// Created by dedlee on 11/9/26.
//

#pragma once

#include <cstdint>

namespace wirelessrecon::hardware::touch {

    struct TouchPoint {
        uint16_t x = 0;
        uint16_t y = 0;
        bool touched = false;
    };

    class ITouch {
    public:
        virtual ~ITouch() = default;

        virtual bool begin() = 0;

        virtual bool isTouched() = 0;

        virtual bool read(TouchPoint& point) = 0;

        virtual void setRotation(uint8_t rotation) = 0;
    };

}
