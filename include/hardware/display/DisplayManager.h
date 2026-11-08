//
// The manager will provide a single access point to the display.
// Created by dedlee on 11/8/26.
//

#pragma once

#include "hardware/display/IDisplay.h"

namespace wirelessrecon::hardware::display {

    class DisplayManager {
    public:
        explicit DisplayManager(IDisplay& display);

        bool begin();

        IDisplay& display();
        const IDisplay& display() const;

        bool isInitialized() const;

    private:
        IDisplay& display_;
        bool initialized_ = false;
    };

} // namespace wirelessrecon::hardware::display
