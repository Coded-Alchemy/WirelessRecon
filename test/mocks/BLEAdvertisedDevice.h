#pragma once

#include <cstdint>
#include <string>
#include <utility>

class BLEAddress {
public:
    BLEAddress() = default;
    explicit BLEAddress(std::string value) : value_(std::move(value)) {}
    std::string toString() const { return value_; }
private:
    std::string value_;
};

class BLEAdvertisedDevice {
public:
    BLEAdvertisedDevice() = default;
    BLEAdvertisedDevice(std::string address, std::string name = "",
                        int rssi = -127, bool hasRssi = true)
        : address_(std::move(address)),
          name_(std::move(name)),
          rssi_(rssi),
          hasRssi_(hasRssi) {}

    BLEAddress getAddress() const { return BLEAddress(address_); }
    std::string getName() const { return name_; }
    int getRSSI() const { return rssi_; }
    bool haveName() const { return !name_.empty(); }
    bool haveRSSI() const { return hasRssi_; }

private:
    std::string address_;
    std::string name_;
    int rssi_ = -127;
    bool hasRssi_ = false;
};

class BLEAdvertisedDeviceCallbacks {
public:
    virtual ~BLEAdvertisedDeviceCallbacks() = default;
    virtual void onResult(BLEAdvertisedDevice advertisedDevice) = 0;
};
