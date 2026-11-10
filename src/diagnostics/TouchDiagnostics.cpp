//
// Created by dedlee on 11/10/26.
//

#include <Arduino.h>

#include "diagnostics/TouchDiagnostics.h"

namespace wirelessrecon::diagnostics {

    using namespace hardware::touch;

    TouchDiagnostics::TouchDiagnostics(
        TouchManager& touchManager
    )
        : touchManager_(touchManager) {
    }

    void TouchDiagnostics::poll() {

        if (!touchManager_.isInitialized()) {
            return;
        }

        TouchPoint point;

        const bool touched = touchManager_.read(point);

        if (touched && point.touched) {

            if (!previousTouchState_ ||
                point.x != previousX_ ||
                point.y != previousY_) {

                Serial.printf(
                    "[DIAGNOSTICS][TOUCH] X: %u | Y: %u\n",
                    point.x,
                    point.y
                );
                }

            previousX_ = point.x;
            previousY_ = point.y;
            previousTouchState_ = true;

        } else if (previousTouchState_) {

            Serial.println(
                "[DIAGNOSTICS][TOUCH] Released."
            );

            previousTouchState_ = false;
        }
    }

    void TouchDiagnostics::reset() {

        previousTouchState_ = false;
        previousX_ = 0;
        previousY_ = 0;
    }

}