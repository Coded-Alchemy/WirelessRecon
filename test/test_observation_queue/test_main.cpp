//
// Created by dedlee on 11/10/26.
//

#include <unity.h>

#include "core/ObservationQueue.h"

#include <string>
#include <utility>

using namespace wirelessrecon::core;

namespace {

Device makeObservation(const std::string& address,
                       const std::string& name,
                       int16_t rssi) {
    Device device;

    device.radio = RadioType::Ble;
    device.address = address;
    device.name = name;
    device.rssi = rssi;
    device.hasRssi = true;
    device.lastSeenMs = 100;

    return device;
}

void test_new_queue_is_empty() {
    ObservationQueue queue;

    TEST_ASSERT_EQUAL_UINT32(0, queue.size());
    TEST_ASSERT_EQUAL_UINT32(0, queue.droppedCount());

    Device observation;
    TEST_ASSERT_FALSE(queue.tryPop(observation));
}

void test_push_and_pop_preserves_observation() {
    ObservationQueue queue;

    Device submitted = makeObservation("AA:BB:CC:DD", "BLE Sensor", -55);

    TEST_ASSERT_TRUE(queue.tryPush(std::move(submitted)));
    TEST_ASSERT_EQUAL_UINT32(1, queue.size());

    Device received;
    TEST_ASSERT_TRUE(queue.tryPop(received));

    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD", received.address.c_str());
    TEST_ASSERT_EQUAL_STRING("BLE Sensor", received.name.c_str());
    TEST_ASSERT_EQUAL_INT(-55, received.rssi);
    TEST_ASSERT_TRUE(received.hasRssi);

    TEST_ASSERT_EQUAL_UINT32(0, queue.size());
}

void test_queue_preserves_fifo_order() {
    ObservationQueue queue;

    Device first = makeObservation("AA:01", "First", -40);
    Device second = makeObservation("AA:02", "Second", -50);
    Device third = makeObservation("AA:03", "Third", -60);

    TEST_ASSERT_TRUE(queue.tryPush(std::move(first)));
    TEST_ASSERT_TRUE(queue.tryPush(std::move(second)));
    TEST_ASSERT_TRUE(queue.tryPush(std::move(third)));

    Device received;

    TEST_ASSERT_TRUE(queue.tryPop(received));
    TEST_ASSERT_EQUAL_STRING("AA:01", received.address.c_str());

    TEST_ASSERT_TRUE(queue.tryPop(received));
    TEST_ASSERT_EQUAL_STRING("AA:02", received.address.c_str());

    TEST_ASSERT_TRUE(queue.tryPop(received));
    TEST_ASSERT_EQUAL_STRING("AA:03", received.address.c_str());

    TEST_ASSERT_FALSE(queue.tryPop(received));
}

void test_full_queue_rejects_new_observation() {
    ObservationQueue queue;

    for (std::size_t i = 0; i < ObservationQueue::kCapacity; ++i) {
        Device observation = makeObservation(
            "DEVICE_" + std::to_string(i),
            "Test Device",
            -60
        );

        TEST_ASSERT_TRUE(queue.tryPush(std::move(observation)));
    }

    TEST_ASSERT_EQUAL_UINT32(ObservationQueue::kCapacity, queue.size());

    Device rejected = makeObservation("OVERFLOW", "Rejected", -80);

    TEST_ASSERT_FALSE(queue.tryPush(std::move(rejected)));
    TEST_ASSERT_EQUAL_UINT32(1, queue.droppedCount());
    TEST_ASSERT_EQUAL_UINT32(ObservationQueue::kCapacity, queue.size());
}

void test_queue_accepts_new_observation_after_pop() {
    ObservationQueue queue;

    for (std::size_t i = 0; i < ObservationQueue::kCapacity; ++i) {
        Device observation = makeObservation(
            "DEVICE_" + std::to_string(i),
            "Test Device",
            -60
        );

        TEST_ASSERT_TRUE(queue.tryPush(std::move(observation)));
    }

    Device received;
    TEST_ASSERT_TRUE(queue.tryPop(received));

    Device next = makeObservation("NEXT_DEVICE", "Next Device", -45);

    TEST_ASSERT_TRUE(queue.tryPush(std::move(next)));
    TEST_ASSERT_EQUAL_UINT32(ObservationQueue::kCapacity, queue.size());
    TEST_ASSERT_EQUAL_UINT32(0, queue.droppedCount());
}

void test_clear_discards_pending_observations() {
    ObservationQueue queue;

    Device first = makeObservation("AA:01", "First", -40);
    Device second = makeObservation("AA:02", "Second", -50);

    TEST_ASSERT_TRUE(queue.tryPush(std::move(first)));
    TEST_ASSERT_TRUE(queue.tryPush(std::move(second)));

    queue.clear();

    TEST_ASSERT_EQUAL_UINT32(0, queue.size());

    Device received;
    TEST_ASSERT_FALSE(queue.tryPop(received));
}

void test_clear_preserves_drop_counter() {
    ObservationQueue queue;

    for (std::size_t i = 0; i < ObservationQueue::kCapacity; ++i) {
        Device observation = makeObservation(
            "DEVICE_" + std::to_string(i),
            "Test Device",
            -60
        );

        TEST_ASSERT_TRUE(queue.tryPush(std::move(observation)));
    }

    Device rejected = makeObservation("OVERFLOW", "Rejected", -80);
    TEST_ASSERT_FALSE(queue.tryPush(std::move(rejected)));

    queue.clear();

    TEST_ASSERT_EQUAL_UINT32(0, queue.size());
    TEST_ASSERT_EQUAL_UINT32(1, queue.droppedCount());
}

} // namespace

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_new_queue_is_empty);
    RUN_TEST(test_push_and_pop_preserves_observation);
    RUN_TEST(test_queue_preserves_fifo_order);
    RUN_TEST(test_full_queue_rejects_new_observation);
    RUN_TEST(test_queue_accepts_new_observation_after_pop);
    RUN_TEST(test_clear_discards_pending_observations);
    RUN_TEST(test_clear_preserves_drop_counter);

    return UNITY_END();
}