//
// Created by dedlee on 11/10/26.
//
#include "core/ObservationQueue.h"

#include <limits>
#include <utility>

namespace wirelessrecon::core {

    bool ObservationQueue::tryPush(Device&& observation) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (size_ >= kCapacity) {
            // Saturate instead of allowing the counter to wrap around.
            if (droppedCount_ < std::numeric_limits<uint32_t>::max()) {
                ++droppedCount_;
            }

            return false;
        }

        // Move the observation into the next available queue slot.
        entries_[tail_] = std::move(observation);

        tail_ = (tail_ + 1) % kCapacity;
        ++size_;

        return true;
    }

    bool ObservationQueue::tryPop(Device& observation) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (size_ == 0) {
            return false;
        }

        // Transfer ownership to the application task.
        observation = std::move(entries_[head_]);

        // Release any remaining resources in the consumed slot.
        entries_[head_] = Device{};

        head_ = (head_ + 1) % kCapacity;
        --size_;

        return true;
    }

    std::size_t ObservationQueue::size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_;
    }

    uint32_t ObservationQueue::droppedCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return droppedCount_;
    }

    void ObservationQueue::clear() {
        std::lock_guard<std::mutex> lock(mutex_);

        // Release pending observations without changing drop statistics.
        for (std::size_t i = 0; i < size_; ++i) {
            const std::size_t index = (head_ + i) % kCapacity;
            entries_[index] = Device{};
        }

        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }

} // namespace wirelessrecon::core