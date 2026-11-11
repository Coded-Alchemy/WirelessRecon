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
            return false;
        }

        diagnostics_ = WifiScanDiagnostics{};

        // Configure station mode without connecting to an access point.
        diagnostics_.stationModeReady = WiFi.mode(WIFI_STA);

        if (!diagnostics_.stationModeReady) {
            state_ = ScannerState::Failed;
            return false;
        }

        WiFi.disconnect(false, false);
        delay(100);

        // Release results left by a previous scan.
        WiFi.scanDelete();

        // Start an asynchronous scan and include hidden networks.
        diagnostics_.scanStartResult = WiFi.scanNetworks(true, true);

        if (diagnostics_.scanStartResult == WIFI_SCAN_RUNNING ||
            diagnostics_.scanStartResult >= 0) {
            state_ = ScannerState::Running;
            return true;
        }

        state_ = ScannerState::Failed;
        return false;
    }

    void WifiScanner::stop() {
        if (state_ == ScannerState::Running) {
            // The Arduino scan API does not provide a portable
            // cancellation operation. Discard results when it finishes.
            state_ = ScannerState::Stopping;
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

    const WifiScanDiagnostics& WifiScanner::diagnostics() const {
        return diagnostics_;
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

        diagnostics_.scanCompletionResult = result;

        // A cancellation discards the scan rather than submitting
        // partial results to the inventory.
        if (state_ == ScannerState::Stopping) {
            WiFi.scanDelete();
            state_ = ScannerState::Idle;
            return;
        }

        if (result == WIFI_SCAN_FAILED) {
            WiFi.scanDelete();
            state_ = ScannerState::Failed;
            return;
        }

        collectResults(result);

        // Release the Arduino Wi-Fi scan result buffer.
        WiFi.scanDelete();

        // Completed is reported only after all observations have been
        // submitted to the queue.
        state_ = ScannerState::Completed;
    }

    void WifiScanner::collectResults(int16_t networkCount) {
        diagnostics_.networkCount = networkCount;

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

            // Mark networks sharing the same visible SSID.
            if (ssid.length() > 0) {
                for (int16_t other = 0; other < networkCount; ++other) {
                    if (other != index && WiFi.SSID(other) == ssid) {
                        observation.wifi.duplicateSsid = true;
                        break;
                    }
                }
            }

            // The queue owns accepted observations. Inventory updates
            // remain the responsibility of WirelessCoordinator.
            if (observations_.tryPush(std::move(observation))) {
                ++diagnostics_.observationsQueued;
            } else {
                ++diagnostics_.observationsRejected;
            }
        }
    }

} // namespace wirelessrecon::hardware::radio