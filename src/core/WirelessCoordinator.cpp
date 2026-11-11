//
// Created by dedlee on 11/10/26.
//
#include "core/WirelessCoordinator.h"

namespace wirelessrecon::core {

WirelessCoordinator::WirelessCoordinator(
    DeviceInventory& inventory,
    ObservationQueue& observations,
    RadioCapabilities capabilities)
    : inventory_(inventory),
      observations_(observations),
      capabilities_(capabilities) {
}

std::size_t WirelessCoordinator::radioIndex(RadioType radio) {
    return static_cast<std::size_t>(radio);
}

bool WirelessCoordinator::isSupported(RadioType radio) const {
    switch (radio) {
        case RadioType::Wifi:
            return capabilities_.wifi;

        case RadioType::Ble:
            return capabilities_.ble;

        case RadioType::BluetoothClassic:
            return capabilities_.bluetoothClassic;

        default:
            return false;
    }
}

bool WirelessCoordinator::registerScanner(IRadioScanner& scanner) {
    const RadioType radio = scanner.radio();
    const std::size_t index = radioIndex(radio);

    if (index >= kScannerCount ||
        !isSupported(radio) ||
        scanners_[index] != nullptr ||
        scanner.state() != ScannerState::Idle) {
        return false;
    }

    scanners_[index] = &scanner;
    return true;
}

bool WirelessCoordinator::hasBluetoothConflict(
    RadioType requested) const {

    if (!capabilities_.bluetoothControllerShared ||
        (requested != RadioType::Ble &&
         requested != RadioType::BluetoothClassic)) {
        return false;
    }

    const RadioType other =
        requested == RadioType::Ble
            ? RadioType::BluetoothClassic
            : RadioType::Ble;

    const std::size_t index = radioIndex(other);
    IRadioScanner* scanner = scanners_[index];

    if (scanner == nullptr) {
        return false;
    }

    // Check scanner state as well as inventory state. A cancelled scan
    // may have ended its inventory session but still be stopping hardware.
    return inventory_.scanInProgress(other) ||
           scanner->state() != ScannerState::Idle;
}

ScanRequestResult WirelessCoordinator::requestScan(RadioType radio) {
    const std::size_t index = radioIndex(radio);

    if (index >= kScannerCount || !isSupported(radio)) {
        return ScanRequestResult::UnsupportedRadio;
    }

    IRadioScanner* scanner = scanners_[index];

    if (scanner == nullptr) {
        return ScanRequestResult::ScannerUnavailable;
    }

    if (scanner->state() != ScannerState::Idle ||
        inventory_.scanInProgress(radio)) {
        return ScanRequestResult::ScannerBusy;
    }

    if (hasBluetoothConflict(radio)) {
        return ScanRequestResult::RadioConflict;
    }

    if (!inventory_.beginScan(radio)) {
        return ScanRequestResult::ScannerBusy;
    }

    // If the hardware adapter cannot start, roll back the inventory session.
    if (!scanner->start()) {
    	inventory_.cancelScan(radio);

    	// A failed start may leave the adapter in a terminal state.
    	// Reset it so a later request can retry.
    	const ScannerState state = scanner->state();

    	if (state == ScannerState::Completed ||
        	state == ScannerState::Failed) {
        	scanner->reset();
    	}

    	return ScanRequestResult::StartFailed;
	}

    return ScanRequestResult::Started;
}

bool WirelessCoordinator::cancelScan(RadioType radio) {
    const std::size_t index = radioIndex(radio);

    if (index >= kScannerCount ||
        !inventory_.scanInProgress(radio)) {
        return false;
    }

    IRadioScanner* scanner = scanners_[index];

    if (scanner == nullptr) {
        inventory_.cancelScan(radio);
        return true;
    }

    scanner->stop();

    // Cancel the inventory session immediately. Any observations arriving
    // during asynchronous hardware shutdown will be rejected by upsert().
    inventory_.cancelScan(radio);

    return true;
}

std::size_t WirelessCoordinator::process() {
    std::size_t accepted = 0;
    Device observation;

    // Process queued observations before resolving scanner states.
    // This ensures final observations are included in completed scans.
    while (observations_.tryPop(observation)) {
        if (inventory_.upsert(observation)) {
            ++accepted;
        }
    }

    // Resolve scanner lifecycle transitions.
    for (std::size_t index = 0; index < scanners_.size(); ++index) {
        IRadioScanner* scanner = scanners_[index];

        if (scanner == nullptr) {
            continue;
        }

        const RadioType radio = scanner->radio();

        if (!inventory_.scanInProgress(radio)) {
            continue;
        }

        switch (scanner->state()) {
            case ScannerState::Completed:
                // Commit observations from the successful scan.
                inventory_.completeScan(radio);

                // Acknowledge completion and make the scanner reusable.
                scanner->reset();
                break;

            case ScannerState::Failed:
                // Preserve historical observations without marking
                // partial results as a successful scan.
                inventory_.cancelScan(radio);

                // Return the failed scanner to its ready state.
                scanner->reset();
                break;

            case ScannerState::Idle:
                // Unexpected idle transition: cancel partial results.
                inventory_.cancelScan(radio);
                break;

            case ScannerState::Running:
            case ScannerState::Stopping:
                // The scanner has not reached a terminal state.
                break;
        }
    }

    return accepted;
}

bool WirelessCoordinator::scanInProgress(RadioType radio) const {
    return inventory_.scanInProgress(radio);
}

} // namespace wirelessrecon::core