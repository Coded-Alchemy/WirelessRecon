//
// Created by dedlee on 11/8/26.
//

#include "hardware/display/DisplayManager.h"

namespace wirelessrecon::hardware::display {

    DisplayManager::DisplayManager(IDisplay& display)
        : display_(display) {
    }

    bool DisplayManager::begin() {
        initialized_ = display_.begin();
        return initialized_;
    }

    IDisplay& DisplayManager::display() {
        return display_;
    }

    const IDisplay& DisplayManager::display() const {
        return display_;
    }

    bool DisplayManager::isInitialized() const {
        return initialized_;
    }

} // namespace wirelessrecon::hardware::display
