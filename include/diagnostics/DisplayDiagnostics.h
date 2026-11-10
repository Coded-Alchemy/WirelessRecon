//
// Created by dedlee on 11/10/26.
//

#pragma once

#include "hardware/display/DisplayManager.h"

namespace wirelessrecon::diagnostics {

    class DisplayDiagnostics {
    public:
        explicit DisplayDiagnostics(
            hardware::display::DisplayManager& displayManager
        );

        void run();

    private:
        hardware::display::DisplayManager& displayManager_;
    };

}