//
// Created by dedlee on 11/12/26.
//

#pragma once

#include "core/DeviceInventory.h"
#include "core/WirelessCoordinator.h"
#include "hardware/radio/BleScanner.h"

namespace wirelessrecon::diagnostics {

    /**
     * Runs BLE hardware diagnostics through the normal radio lifecycle.
     *
     * The scanner owns Bluedroid and BLE callbacks. The coordinator owns
     * inventory updates. This class reports results only.
     */
    class BleDiagnostics {
    public:
        BleDiagnostics(
            hardware::radio::BleScanner& scanner,
            core::WirelessCoordinator& coordinator,
            core::DeviceInventory& inventory
        );

        /** Registers the BLE scanner and starts a diagnostic scan. */
        bool begin();

        /** Processes queued observations and reports terminal scan results. */
        void poll();

        /** True after the scan succeeds, fails, or cannot be started. */
        bool finished() const;

    private:
        hardware::radio::BleScanner& scanner_;
        core::WirelessCoordinator& coordinator_;
        core::DeviceInventory& inventory_;

        bool started_ = false;
        bool finished_ = false;

        void printScanStatistics() const;
    };

} // namespace wirelessrecon::diagnostics