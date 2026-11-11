#include "hardware/radio/WifiScanner.h"

#include <Arduino.h>
#include <WiFi.h>

#include <cstdio>
#include <string>
#include <utility>

namespace wirelessrecon::hardware::radio {

using core::Device;
using core::RadioType;
using core::ScannerState;

WifiScanner::WifiScanner(core::ObservationQueue& observations)
    : observations_(observations) {
}

RadioType WifiScanner::radio() const {
    return RadioType::Wifi;
}

bool WifiScanner::start() {
    if (state_ != ScannerState::Idle) {
        Serial.println("[WIFI] Start rejected: scanner is not idle.");
        return false;
    }

    Serial.println("[WIFI] Initializing station mode...");

    const bool modeReady = WiFi.mode(WIFI_STA);

    Serial.printf(
        "[WIFI] Station mode result: %s\n",
        modeReady ? "success" : "failure"
    );

    if (!modeReady) {
        state_ = ScannerState::Failed;
        return false;
    }

    Serial.printf(
        "[WIFI] Interface status: %d\n",
        static_cast<int>(WiFi.status())
    );

    WiFi.disconnect(false, false);
    delay(100);

    WiFi.scanDelete();

    Serial.println("[WIFI] Starting asynchronous scan...");

    const int16_t result = WiFi.scanNetworks(true, true);

    Serial.printf(
        "[WIFI] Scan start returned: %d (RUNNING=%d, FAILED=%d)\n",
        static_cast<int>(result),
        static_cast<int>(WIFI_SCAN_RUNNING),
        static_cast<int>(WIFI_SCAN_FAILED)
    );

    if (result == WIFI_SCAN_RUNNING || result >= 0) {
        state_ = ScannerState::Running;
        Serial.println("[WIFI] Scan accepted.");
        return true;
    }

    state_ = ScannerState::Failed;
    Serial.println("[WIFI] Scan could not be started.");

    return false;
}

void WifiScanner::stop() {
    if (state_ == ScannerState::Running) {
        state_ = ScannerState::Stopping;
        Serial.println("[WIFI] Scan cancellation requested.");
    }
}

ScannerState WifiScanner::state() const {
    return state_;
}

void WifiScanner::reset() {
    if (state_ == ScannerState::Completed ||
        state_ == ScannerState::Failed) {
        state_ = ScannerState::Idle;
    }
}

void WifiScanner::poll() {
    if (state_ != ScannerState::Running &&
        state_ != ScannerState::Stopping) {
        return;
    }

    const int16_t result = WiFi.scanComplete();

    if (result == WIFI_SCAN_RUNNING) {
        return;
    }

    if (state_ == ScannerState::Stopping) {
        Serial.println("[WIFI] Scan stopped; discarding results.");
        WiFi.scanDelete();
        state_ = ScannerState::Idle;
        return;
    }

    if (result == WIFI_SCAN_FAILED) {
        Serial.println("[WIFI] Scan operation failed.");
        WiFi.scanDelete();
        state_ = ScannerState::Failed;
        return;
    }

    Serial.printf("[WIFI] Scan returned %d networks.\n", result);

    collectResults(result);

    WiFi.scanDelete();
    state_ = ScannerState::Completed;
}

void WifiScanner::collectResults(int16_t networkCount) {
    std::size_t queued = 0;
    std::size_t rejected = 0;

    for (int16_t index = 0; index < networkCount; ++index) {
        String ssid;
        uint8_t encryption = 0;
        int32_t rssi = -127;
        int32_t channel = 0;
        uint8_t* bssid = nullptr;

        if (!WiFi.getNetworkInfo(
                static_cast<uint8_t>(index),
                ssid,
                encryption,
                rssi,
                bssid,
                channel)) {
            continue;
        }

        if (bssid == nullptr) {
            continue;
        }

        char address[18];

        std::snprintf(
            address,
            sizeof(address),
            "%02X:%02X:%02X:%02X:%02X:%02X",
            bssid[0], bssid[1], bssid[2],
            bssid[3], bssid[4], bssid[5]
        );

        Device observation;

        observation.radio = RadioType::Wifi;
        observation.address = address;
        observation.name = ssid.length() > 0
            ? std::string(ssid.c_str())
            : "Hidden network";

        observation.rssi = static_cast<int16_t>(rssi);
        observation.hasRssi = true;
        observation.firstSeenMs = millis();
        observation.lastSeenMs = observation.firstSeenMs;

        observation.wifi.ssid = ssid.c_str();
        observation.wifi.channel = static_cast<int16_t>(channel);
        observation.wifi.encryption = encryption;
        observation.wifi.hidden = ssid.length() == 0;

        if (ssid.length() > 0) {
            for (int16_t other = 0; other < networkCount; ++other) {
                if (other != index && WiFi.SSID(other) == ssid) {
                    observation.wifi.duplicateSsid = true;
                    break;
                }
            }
        }

        if (observations_.tryPush(std::move(observation))) {
            ++queued;
        } else {
            ++rejected;
        }
    }

    Serial.printf(
        "[WIFI] Observations queued: %u; rejected: %u\n",
        static_cast<unsigned>(queued),
        static_cast<unsigned>(rejected)
    );
}

} // namespace wirelessrecon::hardware::radio
