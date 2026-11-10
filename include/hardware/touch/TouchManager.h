//
// Created by dedlee on 11/9/26.
//


#pragma once

#include "hardware/touch/ITouch.h"

namespace wirelessrecon::hardware::touch {

    class TouchManager {
    public:
        explicit TouchManager(ITouch& touch);

        bool begin();

        bool isTouched();

        bool read(TouchPoint& point);

        void setRotation(uint8_t rotation);

        bool isInitialized() const;

    private:
        ITouch& touch_;
        bool initialized_ = false;
    };

}
