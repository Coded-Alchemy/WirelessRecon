//
// Created by dedlee on 11/10/26.
//

#pragma once

#include "DeviceInventory.h"
#include "IRadioScanner.h"
#include "ObservationQueue.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace wirelessrecon::core {

/**
 * Radio capabilities supplied by the selected board profile.
 *
 * This deliberately contains no hardware or Arduino types. The target
 * layer maps BoardProfile.h constants into this structure.
 */
struct RadioCapabilities {
    bool wifi = false;
    bool ble = false;
    bool bluetoothClassic = false;

    /**
     * True when BLE and Bluetooth Classic cannot scan concurrently
     * because they share one Bluetooth controller.
     */
    bool bluetoothControllerShared = true;
};

enum class ScanRequestResult : uint8_t {
    Started = 0,
    UnsupportedRadio,
    ScannerUnavailable,
    ScannerBusy,
    RadioConflict,
    StartFailed
};

/**
 * Coordinates radio scanner lifecycles and owns inventory updates.
 *
 * Responsibilities:
 * - Register one scanner for each supported radio.
 * - Validate scan requests against board capabilities.
 * - Prevent conflicting BLE and Classic scans when the controller is shared.
 * - Start and cancel DeviceInventory scan sessions.
 * - Drain asynchronous observations into DeviceInventory.
 * - Commit or cancel inventory scans when scanners reach terminal states.
 *
 * All public methods must be called from the application task.
 * Radio callbacks communicate through ObservationQueue.
 *
 * The coordinator does not implement scheduling or access radio hardware.
 */
class WirelessCoordinator final {
public:
    WirelessCoordinator(
        DeviceInventory& inventory,
        ObservationQueue& observations,
        RadioCapabilities capabilities);

    WirelessCoordinator(const WirelessCoordinator&) = delete;
    WirelessCoordinator& operator=(const WirelessCoordinator&) = delete;

    /** Registers the scanner for its radio. Registration is one-time. */
    bool registerScanner(IRadioScanner& scanner);

    /** Requests an asynchronous scan for the specified radio. */
    ScanRequestResult requestScan(RadioType radio);

    /** Requests cancellation of an active scan. */
    bool cancelScan(RadioType radio);

    /**
     * Processes queued observations and scanner lifecycle transitions.
     *
     * Call regularly from the application task.
     *
     * @return Number of observations accepted by DeviceInventory.
     */
    std::size_t process();

    /** Returns whether the coordinator has an active inventory scan. */
    bool scanInProgress(RadioType radio) const;

    /** Returns the configured board capabilities. */
    const RadioCapabilities& capabilities() const {
        return capabilities_;
    }

private:
    static constexpr std::size_t kScannerCount =
        DeviceInventory::kRadioCount;

    DeviceInventory& inventory_;
    ObservationQueue& observations_;
    RadioCapabilities capabilities_;

    std::array<IRadioScanner*, kScannerCount> scanners_{};

    static std::size_t radioIndex(RadioType radio);
    bool isSupported(RadioType radio) const;
    bool hasBluetoothConflict(RadioType requested) const;
};

} // namespace wirelessrecon::core