
#pragma once

#include "Device.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace wirelessrecon::core {

/**
 * Maintains the historical inventory of discovered wireless devices.
 *
 * Ownership:
 * - This class owns device identity, observation counts, and visibility state.
 * - Radio adapters submit observations through upsert().
 * - The application/coordinator controls scan lifecycle.
 * - UI code reads inventory data.
 *
 * Scan lifecycle:
 *   beginScan() -> upsert() zero or more times -> completeScan()
 *
 * If a scan fails or times out:
 *   beginScan() -> upsert() zero or more times -> cancelScan()
 *
 * A cancelled scan never changes currentlyVisible or generation().
 *
 * sightings counts accepted observations. It does not count scan sessions:
 * for example, each accepted BLE advertisement increments the count.
 *
 * This class is not internally synchronized. Calls must be serialized by
 * the application task. Asynchronous radio callbacks must not call it
 * directly; observations should be transferred to the application task.
 */
class DeviceInventory {
public:
    // Capacity is divided equally across Wi-Fi, BLE, and Bluetooth Classic.
    static constexpr size_t kCapacity = 150;
    static constexpr size_t kRadioCount = 3;

    DeviceInventory() = default;

    /**
     * Starts a scan for one radio.
     *
     * Returns false for an invalid radio or if that radio is already scanning.
     * Existing visibility is preserved until a successful completeScan().
     */
    bool beginScan(RadioType radio);

    /**
     * Adds or updates one observation.
     *
     * The observation is accepted only while its radio has an active scan.
     * Identity is the combination of radio type and address.
     *
     * Returns false for an invalid address, invalid radio, inactive scan,
     * or when the fixed inventory capacity has been reached.
     */
    bool upsert(const Device& observation);

    /**
     * Completes a successfully finished scan.
     *
     * Devices observed during this scan become visible. Previously visible
     * devices not observed during this scan become not currently visible.
     * The inventory generation increments exactly once.
     *
     * Does nothing if no scan is active for the supplied radio.
     */
    void completeScan(RadioType radio);

    /**
     * Aborts a failed, stopped, or timed-out scan.
     *
     * Previously known visibility and generation are left unchanged.
     * Partial observations remain in the historical inventory but are not
     * considered proof that the scan completed successfully.
     */
    void cancelScan(RadioType radio);

    size_t size() const { return count_; }
    size_t count(RadioType radio) const;

    const Device* at(size_t index) const;

    const Device* find(RadioType radio, const char* address) const;

    /**
     * Copies matching devices into caller-owned storage.
     *
     * Returns the number copied. The result is limited by outputCapacity.
     */
    size_t copyRadio(
        RadioType radio,
        Device* output,
        size_t outputCapacity) const;

    /**
     * Counts successful scan completions across all radios.
     * This is not a per-device observation counter.
     */
    uint32_t generation() const { return generation_; }

    bool scanInProgress(RadioType radio) const;

private:
    std::array<Device, kCapacity> devices_{};
    size_t count_ = 0;
    uint32_t generation_ = 0;
    std::array<bool, kRadioCount> scanning_{};

    static size_t radioIndex(RadioType radio);
    static bool validAddress(const char* address);
};

} // namespace wirelessrecon::core
