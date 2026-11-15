//
// Created by dedlee on 11/15/26.
//

#pragma once

#include <cstddef>

#include "core/DeviceInventory.h"
#include "core/WirelessCoordinator.h"

namespace wirelessrecon::hardware::radio {

    class WifiScanner;
    class BleScanner;

    /**
     * Application-task controller for the radio subsystem.
     *
     * This class does not own scanners, the coordinator, or the inventory.
     * It coordinates the existing instances and reports radio inventory
     * changes. All public methods must be called from the application task.
     *
     * Scanner callbacks continue to submit observations through ObservationQueue.
     */
    class RadioController final {
    public:
        RadioController(
            core::WirelessCoordinator& coordinator,
            core::DeviceInventory& inventory,
            WifiScanner& wifiScanner,
            BleScanner* bleScanner = nullptr
        );

        RadioController(const RadioController&) = delete;
        RadioController& operator=(const RadioController&) = delete;

        /**
         * Registers available scanners and starts the normal-mode Wi-Fi scan.
         *
         * BLE is registered when supported and supplied, but is not started.
         */
        bool begin();

        /**
         * Polls Wi-Fi and processes queued observations and scanner transitions.
         * The returned accepted count is aggregate across all registered radios.
         */
        std::size_t process();

    private:
        core::WirelessCoordinator& coordinator_;
        core::DeviceInventory& inventory_;
        WifiScanner& wifiScanner_;
        BleScanner* bleScanner_;

        std::size_t previousWifiCount_ = 0;
        std::size_t previousBleCount_ = 0;
        std::size_t previousBluetoothClassicCount_ = 0;
    };

} // namespace wirelessrecon::hardware::radio