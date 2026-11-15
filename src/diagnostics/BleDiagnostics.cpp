
/**
 * @file BleDiagnostics.cpp
 * @brief Implements the BLE hardware diagnostic workflow.
 *
 * @details
 * BleDiagnostics validates the BLE scanner through the normal
 * WirelessCoordinator lifecycle. It does not directly initialize or
 * deinitialize the Bluetooth controller, Bluedroid, or BLE scanner.
 *
 * Responsibilities:
 * - Register the existing BLE scanner with WirelessCoordinator.
 * - Request a BLE scan through the coordinator.
 * - Capture memory snapshots around Bluetooth initialization.
 * - Process queued BLE observations through the coordinator.
 * - Report scanner initialization, scan, and inventory statistics.
 * - Capture memory after scan completion or failure.
 *
 * Architecture:
 *
 *   BleDiagnostics
 *        |
 *        | registerScanner() / requestScan()
 *        v
 *   WirelessCoordinator
 *        |
 *        | start()
 *        v
 *   BleScanner
 *        |
 *        | BLEDevice::init()
 *        v
 *   Bluetooth Controller / Bluedroid
 *
 *   BLE advertisement callback
 *        |
 *        v
 *   ObservationQueue
 *        |
 *        | coordinator.process()
 *        v
 *   DeviceInventory
 *
 * Memory diagnostics:
 * - The pre-initialization snapshot establishes the baseline.
 * - The post-start snapshot captures memory after the BLE start attempt.
 * - The terminal snapshot captures retained memory after scan processing.
 *
 * These measurements report available heap memory. They do not attribute
 * individual allocations to Bluetooth, the scanner, or the inventory.
 *
 * Threading:
 * - begin() and poll() must execute from the application task.
 * - BLE callbacks execute asynchronously and must not access the inventory
 *   or call MemoryDiagnostics directly.
 *
 * Ownership:
 * - BleDiagnostics holds non-owning references to the scanner,
 *   coordinator, and inventory.
 * - It does not own or destroy any of these objects.
 */

#include "diagnostics/BleDiagnostics.h"

#include <Arduino.h>

#include "diagnostics/MemoryDiagnostics.h"

namespace wirelessrecon::diagnostics {

using core::RadioType;
using core::ScanRequestResult;
using core::ScannerState;

/**
 * @brief Constructs the BLE diagnostic workflow.
 *
 * @param scanner Existing BLE scanner instance.
 * @param coordinator Coordinator responsible for scanner registration,
 *        scan orchestration, observation processing, and inventory updates.
 * @param inventory Authoritative device inventory used for BLE statistics.
 *
 * @note All arguments are non-owning references. Their objects must remain
 *       alive for the lifetime of this diagnostic instance.
 */
BleDiagnostics::BleDiagnostics(
    hardware::radio::BleScanner& scanner,
    core::WirelessCoordinator& coordinator,
    core::DeviceInventory& inventory)
    : scanner_(scanner),
      coordinator_(coordinator),
      inventory_(inventory) {
}

/**
 * @brief Registers the BLE scanner and requests a diagnostic scan.
 *
 * @return true if the coordinator successfully starts the BLE scan.
 * @return false if scanner registration or scan startup fails.
 *
 * @details
 * BLE initialization is intentionally lazy.
 *
 * Registering the scanner does not initialize the Bluetooth controller
 * or Bluedroid. The actual initialization occurs when the coordinator
 * requests a BLE scan and invokes BleScanner::start().
 *
 * BleScanner::start() calls ensureBluetoothStackReady(), which may invoke
 * BLEDevice::init() to initialize the Bluetooth stack.
 *
 * Memory snapshots:
 *
 * 1. Before BLE initialization:
 *    Establishes the available heap baseline after scanner registration.
 *
 * 2. After the BLE start attempt:
 *    Captures available memory after the coordinator has invoked the
 *    scanner's start operation. This includes initialization and any
 *    allocations made during the scan-start operation.
 *
 * @note The second snapshot is taken even if the scan request fails.
 *       A failed start may still have initialized or allocated Bluetooth
 *       resources.
 *
 * @note A successful return means the asynchronous scan was accepted.
 *       It does not mean the scan has completed or discovered devices.
 */
bool BleDiagnostics::begin() {
    Serial.println();
    Serial.println("[DIAGNOSTICS][BLE] Starting BLE diagnostics...");

    // Register the existing scanner with the coordinator.
    //
    // Registration makes BLE available to the coordinator but does not
    // initialize Bluedroid or start a scan.
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

    // Capture the memory baseline before requesting a BLE scan.
    //
    // At this point, scanner registration has completed, but the normal
    // BLE lazy-initialization path has not yet been triggered.
    MemoryDiagnostics::printSnapshot(
        "Before BLE initialization"
    );

    // Request the scan through the normal coordinator lifecycle.
    //
    // The coordinator invokes BleScanner::start(), which initializes
    // the Bluetooth stack when necessary and starts the asynchronous scan.
    const ScanRequestResult result =
        coordinator_.requestScan(RadioType::Ble);

    // Capture memory immediately after the BLE start attempt.
    //
    // This snapshot is useful even when startup fails because Bluetooth
    // initialization may have partially succeeded or allocated resources.
    MemoryDiagnostics::printSnapshot(
        "After BLE initialization and scan start attempt"
    );

    // Handle a rejected or unsuccessful scan request.
    if (result != ScanRequestResult::Started) {
        Serial.printf(
            "[DIAGNOSTICS][BLE] Scan request failed. Result: %u\n",
            static_cast<unsigned>(result)
        );

        // Report the scanner's actual initialization and startup state.
        printScanStatistics();

        finished_ = true;
        return false;
    }

    // The scan is asynchronous. poll() will process observations and
    // detect the scanner's terminal state.
    started_ = true;

    Serial.println(
        "[DIAGNOSTICS][BLE] Asynchronous scan started."
    );

    return true;
}

/**
 * @brief Processes BLE observations and monitors scan completion.
 *
 * @details
 * This method is intended to be called repeatedly from the application
 * loop or application task.
 *
 * BLE advertisement callbacks do not update DeviceInventory directly.
 * Instead, BleScanner submits observations to ObservationQueue.
 *
 * The coordinator processes those observations and updates the
 * authoritative DeviceInventory.
 *
 * Processing sequence:
 *
 * 1. Read the scanner state.
 * 2. Process queued observations through WirelessCoordinator.
 * 3. Report accepted observations and inventory size.
 * 4. Detect completion, failure, or unexpected idle state.
 * 5. Print terminal statistics and a final memory snapshot.
 *
 * @note Memory snapshots are taken only at terminal states. This avoids
 *       repeatedly printing large diagnostic reports during normal polling.
 *
 * @note The terminal memory snapshot represents retained heap after the
 *       coordinator processes the observations available during this poll.
 *       It is not a measurement of peak memory during scanning.
 */
void BleDiagnostics::poll() {
    // Do nothing when the scan has not started or diagnostics have ended.
    if (!started_ || finished_) {
        return;
    }

    // Read the current scanner lifecycle state.
    //
    // The BLE scanner updates this state asynchronously when its scan
    // completes or transitions to another terminal state.
    const ScannerState scannerState = scanner_.state();

    // Process observations submitted by BLE advertisement callbacks.
    //
    // The coordinator is responsible for updating DeviceInventory.
    const std::size_t accepted = coordinator_.process();

    // Report newly accepted observations and the current number of
    // unique BLE devices tracked by DeviceInventory.
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

    // Handle successful scan completion.
    if (scannerState == ScannerState::Completed) {
        Serial.println(
            "[DIAGNOSTICS][BLE] Scan completed successfully."
        );

        // Report Bluetooth initialization state and scan statistics.
        printScanStatistics();

        // Capture retained memory after the completed scan and the
        // coordinator's observation-processing pass.
        MemoryDiagnostics::printSnapshot(
            "After BLE scan completion and inventory processing"
        );

        started_ = false;
        finished_ = true;
        return;
    }

    // Handle a scanner-reported failure.
    if (scannerState == ScannerState::Failed) {
        Serial.println("[DIAGNOSTICS][BLE] Scan failed.");

        // Preserve diagnostic information even when scanning fails.
        printScanStatistics();

        // Capture memory after the failed scan attempt. This can help
        // identify resources retained after unsuccessful initialization.
        MemoryDiagnostics::printSnapshot(
            "After BLE scan failure"
        );

        started_ = false;
        finished_ = true;
        return;
    }

    // An idle state while diagnostics are active is unexpected.
    //
    // The scanner normally transitions to Completed or Failed after
    // an active scan. Report the condition instead of treating it as
    // a successful scan.
    if (scannerState == ScannerState::Idle) {
        Serial.println(
            "[DIAGNOSTICS][BLE] Scanner returned to idle unexpectedly."
        );

        printScanStatistics();

        MemoryDiagnostics::printSnapshot(
            "After unexpected BLE scanner termination"
        );

        started_ = false;
        finished_ = true;
    }
}

/**
 * @brief Reports whether the diagnostic sequence has terminated.
 *
 * @return true if the scan completed, failed, or could not be started.
 *
 * @note This method does not indicate whether the BLE scan succeeded.
 *       Use scanner diagnostics and the reported scan state for that.
 */
bool BleDiagnostics::finished() const {
    return finished_;
}

/**
 * @brief Prints Bluetooth initialization and BLE scan statistics.
 *
 * @details
 * The scanner provides a thread-safe diagnostic snapshot containing:
 *
 * - Whether Bluetooth initialization was attempted.
 * - Whether the Bluetooth controller is enabled.
 * - Whether the Bluedroid host is enabled.
 * - Raw controller and host status values.
 * - Whether the asynchronous scan successfully started.
 * - Number of discovered BLE devices.
 * - Number of observations submitted to ObservationQueue.
 * - Number of observations rejected by ObservationQueue.
 *
 * The inventory count represents unique tracked BLE devices. It is not
 * equivalent to the number of advertisements received or observations
 * submitted.
 *
 * @note This method reports state only. It does not initialize, stop,
 *       reset, or deinitialize the Bluetooth stack.
 */
void BleDiagnostics::printScanStatistics() const {
    // Obtain a thread-safe snapshot from the BLE scanner.
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

    // Inventory records are unique tracked devices, not raw advertisements.
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
