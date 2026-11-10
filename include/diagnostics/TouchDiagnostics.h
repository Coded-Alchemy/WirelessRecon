//
// Created by dedlee on 11/10/26.
//

#pragma once

#include <cstdint>

#include "hardware/touch/TouchManager.h"

namespace wirelessrecon::diagnostics {

    class TouchDiagnostics {
    public:
        explicit TouchDiagnostics(
            hardware::touch::TouchManager& touchManager
        );

        void poll();

        void reset();

    private:
        hardware::touch::TouchManager& touchManager_;

        bool previousTouchState_ = false;
        uint16_t previousX_ = 0;
        uint16_t previousY_ = 0;
    };

}