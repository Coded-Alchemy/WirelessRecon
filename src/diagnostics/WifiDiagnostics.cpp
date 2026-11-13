#include <Arduino.h>

#include "diagnostics/WifiDiagnostics.h"

namespace wirelessrecon::diagnostics {

using core::RadioType;
using core::ScanRequestResult;
using core::ScannerState;

WifiDiagnostics::WifiDiagnostics(
    hardware::radio::WifiScanner& scanner,
    core::WirelessCoordinator& coordinator,
    core::DeviceInventory& inventory)
    : scanner_(scanner),
      coordinator_(coordinator),
      inventory_(inventory) {
}

bool WifiDiagnostics::begin() {
    Serial.println();
    Serial.println("[DIAGNOSTICS][WIFI] Starting Wi-Fi diagnostics...");

    if (!coordinator_.registerScanner(scanner_)) {
        Serial.println(
            "[DIAGNOSTICS][WIFI] Scanner registration failed."
        );

        finished_ = true;
        return false;
    }

    Serial.println(
        "[DIAGNOSTICS][WIFI] Scanner registration successful."
    );

    const ScanRequestResult result =
        coordinator_.requestScan(RadioType::Wifi);

    if (result != ScanRequestResult::Started) {
        Serial.printf(
            "[DIAGNOSTICS][WIFI] Scan request failed. Result: %u\n",
            static_cast<unsigned>(result)
        );

        printScanStatistics();
        finished_ = true;
        return false;
    }

    started_ = true;

    Serial.println(
        "[DIAGNOSTICS][WIFI] Asynchronous scan started."
    );

    return true;
}

void WifiDiagnostics::poll() {
    if (!started_ || finished_) {
        return;
    }

    // Advance the hardware adapter before processing the queue.
    scanner_.poll();

    // Capture state before coordinator.process() resets a terminal scanner.
    const ScannerState scannerState = scanner_.state();

    const std::size_t accepted = coordinator_.process();

    if (accepted > 0) {
        Serial.printf(
            "[DIAGNOSTICS][WIFI] Inventory accepted: %u observations.\n",
            static_cast<unsigned>(accepted)
        );

        Serial.printf(
            "[DIAGNOSTICS][WIFI] Inventory now contains: %u Wi-Fi devices.\n",
            static_cast<unsigned>(
                inventory_.count(RadioType::Wifi)
            )
        );
    }

    if (scannerState == ScannerState::Completed) {
        Serial.println(
            "[DIAGNOSTICS][WIFI] Scan completed successfully."
        );

        printScanStatistics();

        started_ = false;
        finished_ = true;
        return;
    }

    if (scannerState == ScannerState::Failed) {
        Serial.println("[DIAGNOSTICS][WIFI] Scan failed.");

        printScanStatistics();

        started_ = false;
        finished_ = true;
        return;
    }

    if (scannerState == ScannerState::Idle) {
        Serial.println(
            "[DIAGNOSTICS][WIFI] Scanner returned to idle unexpectedly."
        );

        started_ = false;
        finished_ = true;
    }
}

bool WifiDiagnostics::finished() const {
    return finished_;
}

void WifiDiagnostics::printScanStatistics() const {
    const hardware::radio::WifiScanDiagnostics& stats =
        scanner_.diagnostics();

    Serial.println();
    Serial.println("[DIAGNOSTICS][WIFI] Scan report");

    Serial.printf(
        "  Station mode: %s\n",
        stats.stationModeReady ? "ready" : "failed"
    );

    Serial.printf(
        "  Scan start result: %d\n",
        static_cast<int>(stats.scanStartResult)
    );

    Serial.printf(
        "  Scan completion result: %d\n",
        static_cast<int>(stats.scanCompletionResult)
    );

    Serial.printf(
        "  Networks discovered: %d\n",
        static_cast<int>(stats.networkCount)
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
            inventory_.count(RadioType::Wifi)
        )
    );

    Serial.println("[DIAGNOSTICS][WIFI] Report complete.");
    Serial.println();
}

} // namespace wirelessrecon::diagnostics