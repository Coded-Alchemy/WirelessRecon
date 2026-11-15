//
// Created by dedlee on 11/15/26.
//

#include "hardware/radio/RadioController.h"

#include <Arduino.h>

#include "hardware/radio/WifiScanner.h"

#if defined(WR_BOARD_THINGPULSE_WROVER) || \
    defined(WR_BOARD_ESP32_32E)
#include "hardware/radio/BleScanner.h"
#endif

namespace wirelessrecon::hardware::radio {

RadioController::RadioController(
    core::WirelessCoordinator& coordinator,
    core::DeviceInventory& inventory,
    WifiScanner& wifiScanner,
    BleScanner* bleScanner
)
    : coordinator_(coordinator),
      inventory_(inventory),
      wifiScanner_(wifiScanner),
      bleScanner_(bleScanner) {
}

bool RadioController::begin() {
    Serial.println("[RADIO] Initializing radio subsystem...");

    if (!coordinator_.registerScanner(wifiScanner_)) {
        Serial.println("[RADIO] Wi-Fi scanner registration failed.");
        return false;
    }

    Serial.println("[RADIO] Wi-Fi scanner registered.");

#if defined(WR_BOARD_THINGPULSE_WROVER) || \
    defined(WR_BOARD_ESP32_32E)

    if (coordinator_.capabilities().ble && bleScanner_ != nullptr) {
        if (!coordinator_.registerScanner(*bleScanner_)) {
            Serial.println("[RADIO] BLE scanner registration failed.");
            return false;
        }

        Serial.println(
            "[RADIO] BLE scanner registered. "
            "Bluedroid remains uninitialized."
        );
    }

#endif

    // Preserve existing normal-mode behavior: Wi-Fi starts first.
    // BLE remains available for the separate diagnostic sequence.
    const auto result = coordinator_.requestScan(
        core::RadioType::Wifi
    );

    if (result != core::ScanRequestResult::Started) {
        Serial.printf(
            "[RADIO] Wi-Fi scan failed to start. Result: %u\n",
            static_cast<unsigned>(result)
        );
        return false;
    }

    Serial.println("[RADIO] Wi-Fi scan started.");
    return true;
}

std::size_t RadioController::process() {
    // Poll the adapter before processing the coordinator so completed
    // Wi-Fi results are queued before scanner lifecycle transitions.
    wifiScanner_.poll();

    // This is an aggregate count across all registered radio scanners.
    const std::size_t accepted = coordinator_.process();

    const std::size_t wifiCount =
        inventory_.count(core::RadioType::Wifi);

    const std::size_t bleCount =
        inventory_.count(core::RadioType::Ble);

    const std::size_t bluetoothClassicCount =
        inventory_.count(core::RadioType::BluetoothClassic);

    if (accepted > 0) {
        Serial.printf(
            "[RADIO] Accepted %u observations. "
            "Inventory: Wi-Fi=%u, BLE=%u, Bluetooth Classic=%u\n",
            static_cast<unsigned>(accepted),
            static_cast<unsigned>(wifiCount),
            static_cast<unsigned>(bleCount),
            static_cast<unsigned>(bluetoothClassicCount)
        );
    }

    // These counts represent unique tracked devices, not observations.
    if (wifiCount != previousWifiCount_ ||
        bleCount != previousBleCount_ ||
        bluetoothClassicCount != previousBluetoothClassicCount_) {

        Serial.printf(
            "[RADIO] Inventory update: "
            "Wi-Fi=%u, BLE=%u, Bluetooth Classic=%u\n",
            static_cast<unsigned>(wifiCount),
            static_cast<unsigned>(bleCount),
            static_cast<unsigned>(bluetoothClassicCount)
        );

        previousWifiCount_ = wifiCount;
        previousBleCount_ = bleCount;
        previousBluetoothClassicCount_ = bluetoothClassicCount;
    }

    return accepted;
}

} // namespace wirelessrecon::hardware::radio