//
// Created by dedlee on 11/11/26.
//


#pragma once

#include "core/IRadioScanner.h"
#include "core/ObservationQueue.h"

namespace wirelessrecon::hardware::radio {

    /**
     * ESP32 Arduino Wi-Fi scanner.
     *
     * Scans asynchronously and transfers access-point observations
     * into the shared ObservationQueue.
     *
     * poll() must be called regularly from the application task.
     */
    class WifiScanner final : public core::IRadioScanner {
    public:
        explicit WifiScanner(core::ObservationQueue& observations);

        core::RadioType radio() const override;
        bool start() override;
        void stop() override;
        core::ScannerState state() const override;
        void reset() override;

        /** Advances the asynchronous scan and collects completed results. */
        void poll();

    private:
        core::ObservationQueue& observations_;
        core::ScannerState state_ = core::ScannerState::Idle;

        void collectResults(int16_t networkCount);
    };

} // namespace wirelessrecon::hardware::radio
