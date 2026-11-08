#pragma once
#include "Device.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace wirelessrecon::core {

// Fixed-capacity inventory avoids unbounded heap growth on memory-constrained ESP32s.
// A scan adapter calls beginScan(), upsert() for each result, then completeScan().
class DeviceInventory {
public:
    static constexpr size_t kCapacity = 150; // 50 Wi-Fi + 50 BLE + 50 Classic

    bool beginScan(RadioType radio);
    bool upsert(const Device& observation);
    void completeScan(RadioType radio);

    size_t size() const { return count_; }
    size_t count(RadioType radio) const;
    const Device* at(size_t index) const;
    const Device* find(RadioType radio, const char* address) const;
    size_t copyRadio(RadioType radio, Device* output, size_t outputCapacity) const;
    uint32_t generation() const { return generation_; }
    bool scanInProgress(RadioType radio) const;

private:
    std::array<Device, kCapacity> devices_{};
    size_t count_ = 0;
    uint32_t generation_ = 0;
    bool scanning_[3] = {false, false, false};
    static size_t radioIndex(RadioType radio);
};

} // namespace wirelessrecon::core
