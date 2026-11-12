//
// Created by dedlee on 11/10/26.
//

#pragma once

#include "Device.h"

namespace wirelessrecon::core {

    /**
     * Lifecycle state reported by an asynchronous radio scanner.
     *
     * Completed and Failed are terminal states. A scanner returns to Idle
     * when it is ready to accept another scan request.
     */
    enum class ScannerState : uint8_t {
        Idle = 0,
        Running,
        Stopping,
        Completed,
        Failed
    };

    /**
     * Hardware-independent contract for a radio scanner.
     *
     * Implementations own radio hardware and platform-specific callbacks.
     * They submit observations to the supplied ObservationQueue through
     * their adapter implementation, not directly to DeviceInventory.
     *
     * Important callback contract:
     * - start() returns true only after entering Running.
     * - stop() requests cancellation; the scanner may enter Stopping.
     * - Completed must not be reported until all final observation callbacks
     *   have finished submitting observations to the queue.
     * - Idle after cancellation means no further callbacks from that scan
     *   can submit observations.
     *
     * Scanner implementations must serialize their internal state transitions
     * so state() can safely be queried by the application task.
     */
    class IRadioScanner {
    public:
        virtual ~IRadioScanner() = default;

        /** Identifies the radio this scanner controls. */
        virtual RadioType radio() const = 0;

        /**
         * Starts an asynchronous scan.
         *
         * @return true if the scan was accepted and state becomes Running.
         */
        virtual bool start() = 0;

        /** Requests cancellation of the current scan. */
        virtual void stop() = 0;

        /** Returns the scanner's current lifecycle state. */
        virtual ScannerState state() const = 0;

        /**
         * Acknowledges a Completed or Failed state and returns the scanner
         * to Idle so another scan can be started.
         *
         * Implementations must not reset a Running or Stopping scanner.
         */
         virtual void reset() = 0;
    };

} // namespace wirelessrecon::core