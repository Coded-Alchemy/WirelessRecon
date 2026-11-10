//
// Created by dedlee on 11/9/26.
//

#include "hardware/touch/TouchManager.h"

namespace wirelessrecon::hardware::touch {

    TouchManager::TouchManager(ITouch& touch)
        : touch_(touch) {
    }

    bool TouchManager::begin() {
        if (initialized_) {
            return true;
        }

        initialized_ = touch_.begin();
        return initialized_;
    }

    bool TouchManager::isTouched() {
        return initialized_ && touch_.isTouched();
    }

    bool TouchManager::read(TouchPoint& point) {
        if (!initialized_) {
            point = TouchPoint{};
            return false;
        }

        return touch_.read(point);
    }

    void TouchManager::setRotation(uint8_t rotation) {
        touch_.setRotation(rotation);
    }

    bool TouchManager::isInitialized() const {
        return initialized_;
    }

}
