#include "hardware/radio/BleScanner.h"

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <esp_bt.h>
#include <esp_bt_main.h>

#include <string>
#include <utility>

namespace wirelessrecon::hardware::radio {

using core::Device;
using core::RadioType;
using core::ScannerState;

std::atomic<BleScanner*> BleScanner::activeScanner_{nullptr};

BleScanner::AdvertisedDeviceCallbacks::AdvertisedDeviceCallbacks(
    BleScanner& owner)
    : owner_(owner) {
}

void BleScanner::AdvertisedDeviceCallbacks::onResult(
    BLEAdvertisedDevice advertisedDevice) {
    owner_.handleAdvertisedDevice(std::move(advertisedDevice));
}

BleScanner::BleScanner(core::ObservationQueue& observations)
    : observations_(observations),
      callbacks_(*this) {
}

RadioType BleScanner::radio() const {
    return RadioType::Ble;
}

bool BleScanner::ensureBluetoothStackReady() {
    const esp_bt_controller_status_t controllerStatus =
        esp_bt_controller_get_status();
    const esp_bluedroid_status_t hostStatus =
        esp_bluedroid_get_status();

    bool controllerReady =
        controllerStatus == ESP_BT_CONTROLLER_STATUS_ENABLED;
    bool hostReady = hostStatus == ESP_BLUEDROID_STATUS_ENABLED;

    if (!controllerReady || !hostReady) {
        bool expected = false;
        if (initializationAttempted_.compare_exchange_strong(expected, true)) {
            BLEDevice::init("WirelessRecon");
            delay(250);
        }
    }

    const esp_bt_controller_status_t finalControllerStatus =
        esp_bt_controller_get_status();
    const esp_bluedroid_status_t finalHostStatus =
        esp_bluedroid_get_status();

    controllerReady =
        finalControllerStatus == ESP_BT_CONTROLLER_STATUS_ENABLED;
    hostReady = finalHostStatus == ESP_BLUEDROID_STATUS_ENABLED;

    return controllerReady && hostReady;
}

bool BleScanner::start() {
    if (state() != ScannerState::Idle) {
        return false;
    }

    devicesDiscovered_.store(0);
    observationsQueued_.store(0);
    observationsRejected_.store(0);
    scanStarted_.store(false);

    if (!ensureBluetoothStackReady()) {
        state_.store(ScannerState::Failed);
        return false;
    }

    scan_ = BLEDevice::getScan();
    if (scan_ == nullptr) {
        state_.store(ScannerState::Failed);
        return false;
    }

    scan_->clearResults();
    scan_->setAdvertisedDeviceCallbacks(&callbacks_, false, true);
    scan_->setActiveScan(true);
    scan_->setInterval(100);
    scan_->setWindow(80);

    BleScanner* expected = nullptr;
    if (!activeScanner_.compare_exchange_strong(expected, this)) {
        state_.store(ScannerState::Failed);
        return false;
    }

    state_.store(ScannerState::Running);
    // Latch the accepted start result. The scanner may subsequently return
    // to Idle after cancellation, but diagnostics must retain that it started.
    scanStarted_.store(true);

    const bool started = scan_->start(
        kScanDurationSeconds,
        &BleScanner::onScanComplete,
        false);

    if (!started) {
        scanStarted_.store(false);
        expected = this;
        activeScanner_.compare_exchange_strong(expected, nullptr);
        state_.store(ScannerState::Failed);
        return false;
    }

    return true;
}

void BleScanner::stop() {
    ScannerState expected = ScannerState::Running;
    if (!state_.compare_exchange_strong(expected, ScannerState::Stopping)) {
        return;
    }

    if (scan_ != nullptr) {
        scan_->stop();
    }
}

ScannerState BleScanner::state() const {
    return state_.load();
}

void BleScanner::reset() {
    const ScannerState currentState = state();
    if (currentState != ScannerState::Completed &&
        currentState != ScannerState::Failed) {
        return;
    }

    if (scan_ != nullptr) {
        scan_->clearResults();
    }

    BleScanner* expected = this;
    activeScanner_.compare_exchange_strong(expected, nullptr);
    state_.store(ScannerState::Idle);
    // Do not deinitialize Bluedroid: it is shared with Bluetooth Classic.
}

BleScanDiagnostics BleScanner::diagnostics() const {
    BleScanDiagnostics result;
    result.initializationAttempted = initializationAttempted_.load();

    const esp_bt_controller_status_t controllerStatus =
        esp_bt_controller_get_status();
    const esp_bluedroid_status_t hostStatus = esp_bluedroid_get_status();

    result.controllerStatus = static_cast<int>(controllerStatus);
    result.hostStatus = static_cast<int>(hostStatus);
    result.controllerReady =
        controllerStatus == ESP_BT_CONTROLLER_STATUS_ENABLED;
    result.hostReady = hostStatus == ESP_BLUEDROID_STATUS_ENABLED;
    result.scanStarted = scanStarted_.load();

    result.devicesDiscovered = devicesDiscovered_.load();
    result.observationsQueued = observationsQueued_.load();
    result.observationsRejected = observationsRejected_.load();
    return result;
}

void BleScanner::handleAdvertisedDevice(
    BLEAdvertisedDevice advertisedDevice) {
    devicesDiscovered_.fetch_add(1);

    Device observation;
    observation.radio = RadioType::Ble;
    observation.address = advertisedDevice.getAddress().toString();

    if (advertisedDevice.haveName()) {
        observation.name = advertisedDevice.getName();
    }

    if (advertisedDevice.haveRSSI()) {
        observation.rssi =
            static_cast<int16_t>(advertisedDevice.getRSSI());
        observation.hasRssi = true;
    }

    observation.firstSeenMs = millis();
    observation.lastSeenMs = observation.firstSeenMs;

    if (observations_.tryPush(std::move(observation))) {
        observationsQueued_.fetch_add(1);
    } else {
        observationsRejected_.fetch_add(1);
    }
}

void BleScanner::handleScanComplete() {
    const ScannerState currentState = state();

    if (currentState == ScannerState::Stopping) {
        state_.store(ScannerState::Idle);
    } else if (currentState == ScannerState::Running) {
        state_.store(ScannerState::Completed);
    }

    BleScanner* expected = this;
    activeScanner_.compare_exchange_strong(expected, nullptr);
}

void BleScanner::onScanComplete(BLEScanResults results) {
    (void)results;
    BleScanner* scanner = activeScanner_.load();
    if (scanner != nullptr) {
        scanner->handleScanComplete();
    }
}

} // namespace wirelessrecon::hardware::radio
