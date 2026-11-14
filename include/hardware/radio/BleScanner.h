#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <BLEAdvertisedDevice.h>

#include "core/IRadioScanner.h"
#include "core/ObservationQueue.h"

class BLEScan;
class BLEScanResults;

namespace wirelessrecon::hardware::radio {

struct BleScanDiagnostics {
    bool initializationAttempted = false;
    bool controllerReady = false;
    bool hostReady = false;
    bool scanStarted = false;

    int controllerStatus = -1;
    int hostStatus = -1;

    std::size_t devicesDiscovered = 0;
    std::size_t observationsQueued = 0;
    std::size_t observationsRejected = 0;
};

/**
 * Original ESP32 Bluedroid BLE scanner.
 *
 * Bluedroid is initialized lazily and remains initialized after a scan.
 * This preserves the shared Bluetooth host for future Bluetooth Classic
 * support. The scanner never deinitializes the controller or host.
 *
 * BLE callbacks submit observations to ObservationQueue. They never access
 * DeviceInventory or WirelessCoordinator directly.
 */
class BleScanner final : public core::IRadioScanner {
public:
    explicit BleScanner(core::ObservationQueue& observations);

    core::RadioType radio() const override;
    bool start() override;
    void stop() override;
    core::ScannerState state() const override;
    void reset() override;

    /** Returns a thread-safe snapshot of the most recent scan diagnostics. */
    BleScanDiagnostics diagnostics() const;

private:
    class AdvertisedDeviceCallbacks final
        : public BLEAdvertisedDeviceCallbacks {
    public:
        explicit AdvertisedDeviceCallbacks(BleScanner& owner);
        void onResult(BLEAdvertisedDevice advertisedDevice) override;
    private:
        BleScanner& owner_;
    };

    static constexpr uint32_t kScanDurationSeconds = 10;

    core::ObservationQueue& observations_;
    std::atomic<core::ScannerState> state_{core::ScannerState::Idle};
    std::atomic<bool> initializationAttempted_{false};
    std::atomic<bool> scanStarted_{false};

    std::atomic<std::size_t> devicesDiscovered_{0};
    std::atomic<std::size_t> observationsQueued_{0};
    std::atomic<std::size_t> observationsRejected_{0};

    BLEScan* scan_ = nullptr;
    AdvertisedDeviceCallbacks callbacks_;

    static std::atomic<BleScanner*> activeScanner_;

    bool ensureBluetoothStackReady();
    void handleAdvertisedDevice(BLEAdvertisedDevice advertisedDevice);
    void handleScanComplete();
    static void onScanComplete(BLEScanResults results);
};

} // namespace wirelessrecon::hardware::radio
