//
// Created by dedlee on 11/11/26.
//

#pragma once

#include "core/DeviceInventory.h"
#include "core/WirelessCoordinator.h"
#include "hardware/radio/WifiScanner.h"

namespace wirelessrecon::diagnostics {

    /**
     * Runs Wi-Fi hardware diagnostics through the normal radio lifecycle.
     *
     * This module does not perform Wi-Fi scanning itself. It uses
     * WifiScanner and WirelessCoordinator so diagnostic behavior exercises
     * the same implementation used by the normal application.
     */
    class WifiDiagnostics {
    public:
        WifiDiagnostics(
            hardware::radio::WifiScanner& scanner,
            core::WirelessCoordinator& coordinator,
            core::DeviceInventory& inventory
        );

        /** Registers the scanner and starts a diagnostic scan. */
        bool begin();

        /** Advances the scan and reports its final results. */
        void poll();

    private:
        hardware::radio::WifiScanner& scanner_;
        core::WirelessCoordinator& coordinator_;
        core::DeviceInventory& inventory_;

        bool started_ = false;

        void printScanStatistics() const;
    };

} // namespace wirelessrecon::diagnostics