#pragma once

#include "Device.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace wirelessrecon::core {

/**
 * @class DeviceInventory
 * @brief Owns the authoritative historical inventory of discovered devices.
 *
 * @details
 * Radio adapters submit observations through upsert(). The application task
 * serializes inventory access and controls scan lifecycle. UI components read
 * records through the copy APIs and never own the authoritative inventory.
 *
 * Scan lifecycle:
 *   beginScan() -> upsert() zero or more times -> completeScan()
 *
 * Failed or interrupted scan:
 *   beginScan() -> upsert() zero or more times -> cancelScan()
 *
 * A cancelled scan preserves the last successfully established visibility
 * state and does not increment generation().
 *
 * sightings counts accepted observations, not scan sessions.
 *
 * Threading:
 * This class is not internally synchronized. All calls must be serialized
 * by the application task. Radio callbacks must transfer observations to
 * that task rather than calling this class directly.
 */
class DeviceInventory {
public:
    static constexpr std::size_t kCapacity = 150;
    static constexpr std::size_t kRadioCount = 3;

    DeviceInventory() = default;

    /**
     * @brief Starts a scan for one radio.
     * @return false for an invalid radio or an already-active scan.
     */
    bool beginScan(RadioType radio);

    /**
     * @brief Adds or updates one observation during an active scan.
     *
     * Identity is the combination of radio type and address.
     *
     * @return false for an invalid observation, inactive scan, or full
     * inventory.
     */
    bool upsert(const Device& observation);

    /**
     * @brief Commits a successful scan and updates visibility.
     *
     * Devices observed during the scan become visible. Previously visible
     * devices not observed during the scan become not currently visible.
     * The inventory generation increments exactly once.
     */
    void completeScan(RadioType radio);

    /**
     * @brief Cancels a scan without changing established visibility.
     *
     * Partial observations remain in historical storage, but are not treated
     * as proof of a successful scan.
     */
    void cancelScan(RadioType radio);

    /** @brief Returns the total number of records across all radios. */
    std::size_t size() const { return count_; }

    /** @brief Returns the number of records belonging to one radio. */
    std::size_t count(RadioType radio) const;

    /** @brief Returns a read-only pointer to a record by global index. */
    const Device* at(std::size_t index) const;

    /** @brief Finds a record by radio and stable address. */
    const Device* find(RadioType radio, const char* address) const;

    /**
     * @brief Copies up to outputCapacity records belonging to one radio.
     *
     * Records are copied in their inventory order. This method is retained
     * for callers that need a bounded prefix of a radio's inventory.
     *
     * @param radio Radio type to copy.
     * @param output Caller-owned destination buffer.
     * @param outputCapacity Number of Device objects available in output.
     * @return Number of records copied.
     */
    std::size_t copyRadio(
        RadioType radio,
        Device* output,
        std::size_t outputCapacity
    ) const;

    /**
     * @brief Copies a page of records belonging to one radio.
     *
     * @details
     * The method skips the first offset matching records, then copies at most
     * outputCapacity records into caller-owned storage. It does not allocate
     * memory and does not create a second inventory snapshot.
     *
     * This is intended for UI views that display a small bounded page from a
     * larger inventory.
     *
     * @param radio Radio type to copy.
     * @param offset Number of matching records to skip.
     * @param output Caller-owned destination buffer.
     * @param outputCapacity Number of Device objects available in output.
     * @return Number of records copied into output.
     */
    std::size_t copyRadioPage(
        RadioType radio,
        std::size_t offset,
        Device* output,
        std::size_t outputCapacity
    ) const;

    /**
     * @brief Returns the inventory generation.
     *
     * The generation increments once for each successfully completed radio
     * scan. Cancelled scans do not increment it.
     */
    std::uint32_t generation() const { return generation_; }

    /** @brief Returns whether a scan is active for the supplied radio. */
    bool scanInProgress(RadioType radio) const;

private:
    std::array<Device, kCapacity> devices_{};
    std::size_t count_ = 0;
    std::uint32_t generation_ = 0;
    std::array<bool, kRadioCount> scanning_{};

    static std::size_t radioIndex(RadioType radio);
    static bool validAddress(const char* address);
};

} // namespace wirelessrecon::core