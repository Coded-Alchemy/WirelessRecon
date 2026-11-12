#pragma once

#include <cstddef>
#include <cstdint>

#include "core/IRadioScanner.h"
#include "core/ObservationQueue.h"

namespace wirelessrecon::hardware::radio {

    /**
     * Statistics captured by WifiScanner.
     *
     * The diagnostic layer reads this information to report scan results.
     * Keeping the statistics here avoids duplicating scan operations.
     */
    struct WifiScanDiagnostics {
        bool stationModeReady = false;

        int16_t scanStartResult = -32768;
        int16_t scanCompletionResult = -32768;
        int16_t networkCount = 0;

        std::size_t observationsQueued = 0;
        std::size_t observationsRejected = 0;
    };

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

        /** Returns the most recent scan statistics. */
        const WifiScanDiagnostics& diagnostics() const;

    private:
        core::ObservationQueue& observations_;
        core::ScannerState state_ = core::ScannerState::Idle;

        WifiScanDiagnostics diagnostics_{};

        void collectResults(int16_t networkCount);
    };

} // namespace wirelessrecon::hardware::radio