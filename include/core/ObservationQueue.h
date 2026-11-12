//
// Created by dedlee on 11/10/26.
//

#pragma once

#include "Device.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace wirelessrecon::core {

/**
 * Thread-safe, bounded FIFO for transferring radio observations
 * from asynchronous callbacks to the application task.
 *
 * Ownership:
 * - Radio adapters create and submit Device observations.
 * - The queue temporarily owns submitted observations.
 * - The application task removes observations and submits them
 *   to DeviceInventory.
 *
 * The queue does not maintain device identity or visibility.
 *
 * When full, the queue rejects the newest observation and increments
 * its dropped-observation counter. Existing queued observations are
 * never overwritten.
 */
class ObservationQueue final {
public:
    static constexpr std::size_t kCapacity = 64;

    ObservationQueue() = default;

    ObservationQueue(const ObservationQueue&) = delete;
    ObservationQueue& operator=(const ObservationQueue&) = delete;

    /**
     * Adds an observation by transferring ownership into the queue.
     *
     * @return true if accepted; false if the queue is full.
     */
    bool tryPush(Device&& observation);

    /**
     * Removes the oldest observation and transfers it to the caller.
     *
     * @param[out] observation Receives the removed observation.
     * @return true if an observation was available.
     */
    bool tryPop(Device& observation);

    /** Returns the number of observations currently queued. */
    std::size_t size() const;

    /** Returns the cumulative number of observations rejected as full. */
    uint32_t droppedCount() const;

    /**
     * Discards all pending observations.
     *
     * The cumulative dropped-observation count is preserved.
     */
    void clear();

private:
    std::array<Device, kCapacity> entries_{};

    std::size_t head_ = 0;
    std::size_t tail_ = 0;
    std::size_t size_ = 0;

    uint32_t droppedCount_ = 0;

    mutable std::mutex mutex_;
};

} // namespace wirelessrecon::core