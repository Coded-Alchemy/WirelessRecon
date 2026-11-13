//
// Created by dedlee on 11/12/26.
//


#include <Arduino.h>

#include "diagnostics/BleDiagnostics.h"

namespace wirelessrecon::diagnostics {

using core::RadioType;
using core::ScanRequestResult;
using core::ScannerState;

BleDiagnostics::BleDiagnostics(
    hardware::radio::BleScanner& scanner,
    core::WirelessCoordinator& coordinator,
    core::DeviceInventory& inventory)
    : scanner_(scanner),
      coordinator_(coordinator),
      inventory_(inventory) {
}

bool BleDiagnostics::begin() {
    Serial.println();
    Serial.println("[DIAGNOSTICS][BLE] Starting BLE diagnostics...");

    if (!coordinator_.registerScanner(scanner_)) {
        Serial.println(
            "[DIAGNOSTICS][BLE] Scanner registration failed."
        );

        finished_ = true;
        return false;
    }

    Serial.println(
        "[DIAGNOSTICS][BLE] Scanner registration successful."
    );

    const ScanRequestResult result =
        coordinator_.requestScan(RadioType::Ble);

    if (result != ScanRequestResult::Started) {
        Serial.printf(
            "[DIAGNOSTICS][BLE] Scan request failed. Result: %u\n",
            static_cast<unsigned>(result)
        );

        printScanStatistics();
        finished_ = true;
        return false;
    }

    started_ = true;

    Serial.println(
        "[DIAGNOSTICS][BLE] Asynchronous scan started."
    );

    return true;
}

void BleDiagnostics::poll() {
    if (!started_ || finished_) {
        return;
    }

    // Bluedroid delivers advertisements asynchronously. The scanner
    // submits them to ObservationQueue; the coordinator updates inventory.
    const ScannerState scannerState = scanner_.state();

    const std::size_t accepted = coordinator_.process();

    if (accepted > 0) {
        Serial.printf(
            "[DIAGNOSTICS][BLE] Inventory accepted: %u observations.\n",
            static_cast<unsigned>(accepted)
        );

        Serial.printf(
            "[DIAGNOSTICS][BLE] Inventory now contains: %u BLE devices.\n",
            static_cast<unsigned>(
                inventory_.count(RadioType::Ble)
            )
        );
    }

    if (scannerState == ScannerState::Completed) {
        Serial.println(
            "[DIAGNOSTICS][BLE] Scan completed successfully."
        );

        printScanStatistics();

        started_ = false;
        finished_ = true;
        return;
    }

    if (scannerState == ScannerState::Failed) {
        Serial.println("[DIAGNOSTICS][BLE] Scan failed.");

        printScanStatistics();

        started_ = false;
        finished_ = true;
        return;
    }

    if (scannerState == ScannerState::Idle) {
        Serial.println(
            "[DIAGNOSTICS][BLE] Scanner returned to idle unexpectedly."
        );

        printScanStatistics();

        started_ = false;
        finished_ = true;
    }
}

bool BleDiagnostics::finished() const {
    return finished_;
}

void BleDiagnostics::printScanStatistics() const {
    const hardware::radio::BleScanDiagnostics& stats =
        scanner_.diagnostics();

    Serial.println();
    Serial.println("[DIAGNOSTICS][BLE] Scan report");

    Serial.printf(
        "  Initialization attempted: %s\n",
        stats.initializationAttempted ? "yes" : "no"
    );

    Serial.printf(
        "  Bluetooth controller: %s\n",
        stats.controllerReady ? "ready" : "not ready"
    );

    Serial.printf(
        "  Bluedroid host: %s\n",
        stats.hostReady ? "ready" : "not ready"
    );

    Serial.printf(
        "  Controller status: %d\n",
        stats.controllerStatus
    );

    Serial.printf(
        "  Host status: %d\n",
        stats.hostStatus
    );

    Serial.printf(
        "  Scan started: %s\n",
        stats.scanStarted ? "yes" : "no"
    );

    Serial.printf(
        "  Devices discovered: %u\n",
        static_cast<unsigned>(stats.devicesDiscovered)
    );

    Serial.printf(
        "  Observations queued: %u\n",
        static_cast<unsigned>(stats.observationsQueued)
    );

    Serial.printf(
        "  Observations rejected: %u\n",
        static_cast<unsigned>(stats.observationsRejected)
    );

    Serial.printf(
        "  Inventory records: %u\n",
        static_cast<unsigned>(
            inventory_.count(RadioType::Ble)
        )
    );

    Serial.println("[DIAGNOSTICS][BLE] Report complete.");
    Serial.println();
}

} // namespace wirelessrecon::diagnostics
