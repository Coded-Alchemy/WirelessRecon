#include "core/DeviceInventory.h"

#include <limits>
#include <string>

namespace wirelessrecon::core {

/**
 * @brief Converts a radio enum to its array index.
 *
 * The caller must validate the returned index against kRadioCount.
 */
std::size_t DeviceInventory::radioIndex(RadioType radio) {
    return static_cast<std::size_t>(radio);
}

/** @brief Checks that an address pointer refers to a nonempty string. */
bool DeviceInventory::validAddress(const char* address) {
    return address != nullptr && address[0] != '\0';
}

bool DeviceInventory::beginScan(RadioType radio) {
    const std::size_t index = radioIndex(radio);

    if (index >= kRadioCount || scanning_[index]) {
        return false;
    }

    scanning_[index] = true;

    // Reset only this radio's scan markers. Visibility is not changed until
    // a successful completeScan().
    for (std::size_t i = 0; i < count_; ++i) {
        if (devices_[i].radio == radio) {
            devices_[i].seenInCurrentScan = false;
        }
    }

    return true;
}

bool DeviceInventory::upsert(const Device& observation) {
    const std::size_t radio = radioIndex(observation.radio);

    if (!validAddress(observation.address.c_str()) ||
        radio >= kRadioCount ||
        !scanning_[radio]) {
        return false;
    }

    std::size_t index = count_;

    // A radio address is the stable identity. SSID is not unique.
    for (std::size_t i = 0; i < count_; ++i) {
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

        // Lifecycle fields are maintained by the inventory.
        added.firstSeenMs = observation.firstSeenMs != 0
                                ? observation.firstSeenMs
                                : observation.lastSeenMs;
        added.sightings = 1;
        added.seenInCurrentScan = true;

        ++count_;
        return true;
    }

    Device& current = devices_[index];

    const std::uint32_t firstSeen = current.firstSeenMs;
    const std::uint32_t previousSightings = current.sightings;

    // Preserve a useful Bluetooth name when a later advertisement omits it.
    const bool preserveName =
        observation.name.empty() && !current.name.empty();

    const std::string previousName = current.name;

    current = observation;
    current.firstSeenMs = firstSeen;
    current.sightings =
        previousSightings < std::numeric_limits<std::uint32_t>::max()
            ? previousSightings + 1
            : previousSightings;

    if (preserveName) {
        current.name = previousName;
    }

    current.seenInCurrentScan = true;
    return true;
}

void DeviceInventory::completeScan(RadioType radio) {
    const std::size_t index = radioIndex(radio);

    if (index >= kRadioCount || !scanning_[index]) {
        return;
    }

    for (std::size_t i = 0; i < count_; ++i) {
        if (devices_[i].radio == radio) {
            devices_[i].currentlyVisible =
                devices_[i].seenInCurrentScan;
        }
    }

    scanning_[index] = false;
    ++generation_;
}

void DeviceInventory::cancelScan(RadioType radio) {
    const std::size_t index = radioIndex(radio);

    if (index >= kRadioCount || !scanning_[index]) {
        return;
    }

    // Clear temporary markers while preserving historical visibility.
    for (std::size_t i = 0; i < count_; ++i) {
        if (devices_[i].radio == radio) {
            devices_[i].seenInCurrentScan = false;
        }
    }

    scanning_[index] = false;
}

std::size_t DeviceInventory::count(RadioType radio) const {
    const std::size_t index = radioIndex(radio);

    if (index >= kRadioCount) {
        return 0;
    }

    std::size_t result = 0;

    for (std::size_t i = 0; i < count_; ++i) {
        if (devices_[i].radio == radio) {
            ++result;
        }
    }

    return result;
}

const Device* DeviceInventory::at(std::size_t index) const {
    return index < count_ ? &devices_[index] : nullptr;
}

const Device* DeviceInventory::find(
    RadioType radio,
    const char* address
) const {
    const std::size_t index = radioIndex(radio);

    if (index >= kRadioCount || !validAddress(address)) {
        return nullptr;
    }

    for (std::size_t i = 0; i < count_; ++i) {
        const Device& device = devices_[i];

        if (device.radio == radio && device.address == address) {
            return &device;
        }
    }

    return nullptr;
}

std::size_t DeviceInventory::copyRadio(
    RadioType radio,
    Device* output,
    std::size_t outputCapacity
) const {
    return copyRadioPage(radio, 0, output, outputCapacity);
}

std::size_t DeviceInventory::copyRadioPage(
    RadioType radio,
    std::size_t offset,
    Device* output,
    std::size_t outputCapacity
) const {
    const std::size_t index = radioIndex(radio);

    if (index >= kRadioCount ||
        output == nullptr ||
        outputCapacity == 0) {
        return 0;
    }

    std::size_t matchingRecordsSkipped = 0;
    std::size_t copied = 0;

    for (std::size_t i = 0;
         i < count_ && copied < outputCapacity;
         ++i) {

        if (devices_[i].radio != radio) {
            continue;
        }

        if (matchingRecordsSkipped < offset) {
            ++matchingRecordsSkipped;
            continue;
        }

        output[copied++] = devices_[i];
    }

    return copied;
}

bool DeviceInventory::scanInProgress(RadioType radio) const {
    const std::size_t index = radioIndex(radio);

    return index < kRadioCount && scanning_[index];
}

} // namespace wirelessrecon::core