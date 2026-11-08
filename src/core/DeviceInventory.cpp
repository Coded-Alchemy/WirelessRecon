#include "core/DeviceInventory.h"
#include <cstring>

namespace wirelessrecon::core {

size_t DeviceInventory::radioIndex(RadioType radio) {
    return static_cast<size_t>(radio);
}

bool DeviceInventory::beginScan(RadioType radio) {
    const size_t index = radioIndex(radio);
    if (index >= 3 || scanning_[index]) return false;
    scanning_[index] = true;
    for (size_t i = 0; i < count_; ++i) {
        if (devices_[i].radio == radio) devices_[i].seenInCurrentScan = false;
    }
    return true;
}

bool DeviceInventory::upsert(const Device& observation) {
    if (observation.address.empty()) return false;
    size_t index = count_;
    for (size_t i = 0; i < count_; ++i) {
        if (sameIdentity(devices_[i], observation)) { index = i; break; }
    }

    if (index == count_) {
        if (count_ >= kCapacity) return false;
        devices_[index] = observation;
        devices_[index].firstSeenMs = observation.firstSeenMs ? observation.firstSeenMs : observation.lastSeenMs;
        devices_[index].sightings = observation.sightings ? observation.sightings : 1;
        ++count_;
    } else {
        Device& current = devices_[index];
        const uint32_t firstSeen = current.firstSeenMs;
        const uint32_t sightings = current.sightings + 1;
        const WifiMetadata previousWifi = current.wifi;
        current = observation;
        current.firstSeenMs = firstSeen;
        current.sightings = observation.sightings ? observation.sightings : sightings;
        if (current.radio == RadioType::Wifi && current.wifi.ssid.empty()) current.wifi = previousWifi;
    }

    devices_[index].seenInCurrentScan = true;
    devices_[index].currentlyVisible = true;
    return true;
}

void DeviceInventory::completeScan(RadioType radio) {
    const size_t index = radioIndex(radio);
    if (index >= 3) return;
    for (size_t i = 0; i < count_; ++i) {
        if (devices_[i].radio == radio) devices_[i].currentlyVisible = devices_[i].seenInCurrentScan;
    }
    scanning_[index] = false;
    ++generation_;
}

size_t DeviceInventory::count(RadioType radio) const {
    size_t result = 0;
    for (size_t i = 0; i < count_; ++i) if (devices_[i].radio == radio) ++result;
    return result;
}

const Device* DeviceInventory::at(size_t index) const {
    return index < count_ ? &devices_[index] : nullptr;
}

const Device* DeviceInventory::find(RadioType radio, const char* address) const {
    if (!address) return nullptr;
    for (size_t i = 0; i < count_; ++i) {
        if (devices_[i].radio == radio && devices_[i].address == address) return &devices_[i];
    }
    return nullptr;
}

size_t DeviceInventory::copyRadio(RadioType radio, Device* output, size_t outputCapacity) const {
    if (!output || outputCapacity == 0) return 0;
    size_t copied = 0;
    for (size_t i = 0; i < count_ && copied < outputCapacity; ++i) {
        if (devices_[i].radio == radio) output[copied++] = devices_[i];
    }
    return copied;
}

bool DeviceInventory::scanInProgress(RadioType radio) const {
    const size_t index = radioIndex(radio);
    return index < 3 && scanning_[index];
}

} // namespace wirelessrecon::core
