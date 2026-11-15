/**
* @file board_scaffold.cpp
 * @brief Minimal Arduino entry point for the modular WirelessRecon runtime.
 *
 * Application construction, startup sequencing, diagnostics, and runtime
 * processing are implemented by ApplicationRuntime.
 *
 * This file intentionally contains no hardware initialization or scanner
 * orchestration logic.
 */

#include <Arduino.h>

#include "application/ApplicationRuntime.h"

namespace {

    /**
     * @brief Global application composition root.
     *
     * The runtime owns the application components and outlives both Arduino
     * entry-point functions.
     */
    wirelessrecon::application::ApplicationRuntime applicationRuntime;

} // namespace

/**
 * @brief Arduino startup entry point.
 *
 * Delegates all application initialization to ApplicationRuntime.
 */
void setup() {
    applicationRuntime.begin();
}

/**
 * @brief Arduino application loop.
 *
 * Delegates normal or diagnostic servicing to ApplicationRuntime.
 */
void loop() {
    applicationRuntime.loop();
}