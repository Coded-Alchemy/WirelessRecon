
#include "core/DeviceInventory.h"

#include <limits>

namespace wirelessrecon::core {

size_t DeviceInventory::radioIndex(RadioType radio) {
    return static_cast<size_t>(radio);
}

bool DeviceInventory::validAddress(const char* address) {
    return address != nullptr && address[0] != '\0';
}

bool DeviceInventory::beginScan(RadioType radio) {
    const size_t index = radioIndex(radio);

    if (index >= kRadioCount || scanning_[index]) {
        return false;
    }

    scanning_[index] = true;

    // Reset only this radio's scan markers. Do not mark devices missing yet:
    // the scan has not completed and may still fail.
    for (size_t i = 0; i < count_; ++i) {
        if (devices_[i].radio == radio) {
            devices_[i].seenInCurrentScan = false;
        }
    }

    return true;
}

bool DeviceInventory::upsert(const Device& observation) {
    const size_t radio = radioIndex(observation.radio);

    // Reject malformed observations and callbacks arriving outside a scan.
    if (!validAddress(observation.address.c_str()) ||
        radio >= kRadioCount ||
        !scanning_[radio]) {
        return false;
    }

    size_t index = count_;

    // Radio + address form the stable device identity.
    for (size_t i = 0; i < count_; ++i) {
        if (sameIdentity(devices_[i], observation)) {
            index = i;
            break;
        }
    }

    if (index == count_) {
        if (count_ >= kCapacity) {
            return false;
        }

        Device& added = devices_[count_];
        added = observation;

        // Lifecycle counters are owned by the inventory, not the adapter.
        added.firstSeenMs = observation.firstSeenMs != 0
                                ? observation.firstSeenMs
                                : observation.lastSeenMs;
        added.sightings = 1;
        added.seenInCurrentScan = true;

        ++count_;
        return true;
    }

    Device& current = devices_[index];

    const uint32_t firstSeen = current.firstSeenMs;
    const uint32_t previousSightings = current.sightings;

    // Keep the last useful Bluetooth name if a later advertisement omits it.
    // Wi-Fi SSID is stored separately in WifiMetadata and is always replaced.
    const bool preserveName =
        observation.name.empty() && !current.name.empty();

    const std::string previousName = current.name;

    current = observation;
    current.firstSeenMs = firstSeen;
    current.sightings =
        previousSightings < std::numeric_limits<uint32_t>::max()
            ? previousSightings + 1
            : previousSightings;

    if (preserveName) {
        current.name = previousName;
    }

    current.seenInCurrentScan = true;

    return true;
}

void DeviceInventory::completeScan(RadioType radio) {
    const size_t index = radioIndex(radio);

    // Ignore unmatched completion callbacks. They must not change generation.
    if (index >= kRadioCount || !scanning_[index]) {
        return;
    }

    for (size_t i = 0; i < count_; ++i) {
        if (devices_[i].radio == radio) {
            devices_[i].currentlyVisible =
                devices_[i].seenInCurrentScan;
        }
    }

    scanning_[index] = false;
    ++generation_;
}

void DeviceInventory::cancelScan(RadioType radio) {
    const size_t index = radioIndex(radio);

    if (index >= kRadioCount || !scanning_[index]) {
        return;
    }

    // Discard this scan's temporary markers. Keep historical records and
    // their last successfully established visibility state unchanged.
    for (size_t i = 0; i < count_; ++i) {
        if (devices_[i].radio == radio) {
            devices_[i].seenInCurrentScan = false;
        }
    }

    scanning_[index] = false;
}

size_t DeviceInventory::count(RadioType radio) const {
    const size_t index = radioIndex(radio);

    if (index >= kRadioCount) {
        return 0;
    }

    size_t result = 0;

    for (size_t i = 0; i < count_; ++i) {
        if (devices_[i].radio == radio) {
            ++result;
        }
    }

    return result;
}

const Device* DeviceInventory::at(size_t index) const {
    return index < count_ ? &devices_[index] : nullptr;
}

const Device* DeviceInventory::find(
    RadioType radio,
    const char* address) const {

    const size_t index = radioIndex(radio);

    if (index >= kRadioCount || !validAddress(address)) {
        return nullptr;
    }

    for (size_t i = 0; i < count_; ++i) {
        const Device& device = devices_[i];

        if (device.radio == radio && device.address == address) {
            return &device;
        }
    }

    return nullptr;
}

size_t DeviceInventory::copyRadio(
    RadioType radio,
    Device* output,
    size_t outputCapacity) const {

    const size_t index = radioIndex(radio);

    if (index >= kRadioCount || output == nullptr || outputCapacity == 0) {
        return 0;
    }

    size_t copied = 0;

    for (size_t i = 0; i < count_ && copied < outputCapacity; ++i) {
        if (devices_[i].radio == radio) {
            output[copied++] = devices_[i];
        }
    }

    return copied;
}

bool DeviceInventory::scanInProgress(RadioType radio) const {
    const size_t index = radioIndex(radio);

    return index < kRadioCount && scanning_[index];
}

} // namespace wirelessrecon::core
