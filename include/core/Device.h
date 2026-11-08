#pragma once
#include <cstdint>
#include <string>

namespace wirelessrecon::core {

enum class RadioType : uint8_t { Wifi = 0, Ble = 1, BluetoothClassic = 2 };

struct WifiMetadata {
    std::string ssid;
    int16_t channel = 0;
    uint8_t encryption = 0; // Adapter stores platform-native enum value.
    bool hidden = false;
    bool duplicateSsid = false;
};

// Common identity and lifecycle fields shared by every radio technology.
// address is the stable radio identifier (BSSID or Bluetooth address).
struct Device {
    RadioType radio = RadioType::Wifi;
    std::string address;
    std::string name;
    int16_t rssi = -127;
    bool hasRssi = false;
    uint32_t firstSeenMs = 0;
    uint32_t lastSeenMs = 0;
    uint32_t sightings = 0;
    bool seenInCurrentScan = false;
    bool currentlyVisible = false;
    WifiMetadata wifi{};
};

inline bool sameIdentity(const Device& a, const Device& b) {
    return a.radio == b.radio && a.address == b.address;
}

} // namespace wirelessrecon::core
