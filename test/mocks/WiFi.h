
#pragma once

#include "Arduino.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

constexpr int WIFI_STA = 1;
constexpr int16_t WIFI_SCAN_RUNNING = -1;
constexpr int16_t WIFI_SCAN_FAILED = -2;

namespace fake_platform {

struct WifiNetwork {
    std::string ssid;
    std::array<uint8_t, 6> bssid{};
    uint8_t encryption = 3;
    int32_t rssi = -55;
    int32_t channel = 6;
    bool infoAvailable = true;
    bool nullBssid = false;
};

inline WifiNetwork makeWifiNetwork(
    const std::string& ssid,
    std::array<uint8_t, 6> bssid,
    int32_t rssi = -55,
    int32_t channel = 6,
    uint8_t encryption = 3
) {
    WifiNetwork result;
    result.ssid = ssid;
    result.bssid = bssid;
    result.rssi = rssi;
    result.channel = channel;
    result.encryption = encryption;
    return result;
}

} // namespace fake_platform

class FakeWiFiClass {
public:
    std::vector<fake_platform::WifiNetwork> networks;

    bool modeResult = true;
    int16_t startResult = WIFI_SCAN_RUNNING;
    int16_t completionResult = WIFI_SCAN_RUNNING;
    bool modeCalled = false;
    int modeArgument = -1;
    int disconnectCalls = 0;
    int scanDeleteCalls = 0;
    int scanNetworksCalls = 0;
    bool asyncArgument = false;
    bool showHiddenArgument = false;
    bool passiveArgument = true;
    uint32_t maxMsPerChannelArgument = 0;

    void reset() {
        networks.clear();
        modeResult = true;
        startResult = WIFI_SCAN_RUNNING;
        completionResult = WIFI_SCAN_RUNNING;
        modeCalled = false;
        modeArgument = -1;
        disconnectCalls = 0;
        scanDeleteCalls = 0;
        scanNetworksCalls = 0;
        asyncArgument = false;
        showHiddenArgument = false;
        passiveArgument = true;
        maxMsPerChannelArgument = 0;
    }

    bool mode(int value) {
        modeCalled = true;
        modeArgument = value;
        return modeResult;
    }

    bool disconnect(bool, bool) {
        ++disconnectCalls;
        return true;
    }

    int16_t scanNetworks(
        bool async,
        bool showHidden,
        bool passive,
        uint32_t maxMsPerChannel
    ) {
        ++scanNetworksCalls;
        asyncArgument = async;
        showHiddenArgument = showHidden;
        passiveArgument = passive;
        maxMsPerChannelArgument = maxMsPerChannel;
        return startResult;
    }

    int16_t scanComplete() const {
        return completionResult;
    }

    void scanDelete() {
        ++scanDeleteCalls;
    }

    bool getNetworkInfo(
        uint8_t index,
        String& ssid,
        uint8_t& encryption,
        int32_t& rssi,
        uint8_t*& bssid,
        int32_t& channel
    ) {
        if (index >= networks.size()) {
            return false;
        }

        fake_platform::WifiNetwork& network = networks[index];

        if (!network.infoAvailable) {
            return false;
        }

        ssid = String(network.ssid);
        encryption = network.encryption;
        rssi = network.rssi;
        channel = network.channel;
        bssid = network.nullBssid ? nullptr : network.bssid.data();

        return true;
    }

    String SSID(int32_t index) const {
        if (index < 0 ||
            static_cast<std::size_t>(index) >= networks.size()) {
            return String();
        }

        return String(
            networks[static_cast<std::size_t>(index)].ssid
        );
    }
};

inline FakeWiFiClass WiFi;
