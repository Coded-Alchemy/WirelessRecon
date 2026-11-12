
#include <unity.h>

#include "core/DeviceInventory.h"

#include <string>

using namespace wirelessrecon::core;

namespace {

DeviceInventory inventory;

/**
 * Creates a minimal observation for inventory tests.
 * Timestamps and identity are supplied explicitly for deterministic tests.
 */
Device makeDevice(
    RadioType radio,
    const char* address,
    const char* name,
    int16_t rssi,
    uint32_t timestamp) {

    Device device;
    device.radio = radio;
    device.address = address;
    device.name = name;
    device.rssi = rssi;
    device.hasRssi = true;
    device.firstSeenMs = timestamp;
    device.lastSeenMs = timestamp;

    return device;
}

void test_inventory_starts_empty() {
    TEST_ASSERT_EQUAL_UINT32(0, inventory.size());
    TEST_ASSERT_EQUAL_UINT32(0, inventory.count(RadioType::Wifi));
    TEST_ASSERT_EQUAL_UINT32(0, inventory.generation());
}

void test_scan_must_begin_before_observation() {
    const Device device =
        makeDevice(RadioType::Wifi, "AA:BB:CC:DD:EE:01", "Test AP", -45, 100);

    TEST_ASSERT_FALSE(inventory.upsert(device));
    TEST_ASSERT_EQUAL_UINT32(0, inventory.size());
}

void test_begin_scan_rejects_duplicate_scan() {
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Wifi));
    TEST_ASSERT_FALSE(inventory.beginScan(RadioType::Wifi));
    TEST_ASSERT_TRUE(inventory.scanInProgress(RadioType::Wifi));
}

void test_invalid_radio_is_rejected() {
    const RadioType invalidRadio = static_cast<RadioType>(99);

    TEST_ASSERT_FALSE(inventory.beginScan(invalidRadio));
    TEST_ASSERT_EQUAL_UINT32(0, inventory.count(invalidRadio));
    TEST_ASSERT_FALSE(inventory.scanInProgress(invalidRadio));

    const Device device =
        makeDevice(invalidRadio, "AA:BB:CC:DD:EE:02", "Invalid", -50, 100);

    TEST_ASSERT_FALSE(inventory.upsert(device));
}

void test_empty_address_is_rejected() {
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Wifi));

    const Device emptyAddress =
        makeDevice(RadioType::Wifi, "", "Invalid", -50, 100);

    Device nullAddress;
    nullAddress.radio = RadioType::Wifi;
    nullAddress.name = "Invalid";
    nullAddress.rssi = -50;

    TEST_ASSERT_FALSE(inventory.upsert(emptyAddress));
    TEST_ASSERT_FALSE(inventory.upsert(nullAddress));
    TEST_ASSERT_EQUAL_UINT32(0, inventory.size());
}

void test_insert_creates_one_device() {
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Wifi));

    const Device device =
        makeDevice(RadioType::Wifi, "AA:BB:CC:DD:EE:03", "Test AP", -45, 100);

    TEST_ASSERT_TRUE(inventory.upsert(device));
    TEST_ASSERT_EQUAL_UINT32(1, inventory.size());
    TEST_ASSERT_EQUAL_UINT32(1, inventory.count(RadioType::Wifi));

    const Device* stored =
        inventory.find(RadioType::Wifi, "AA:BB:CC:DD:EE:03");

    TEST_ASSERT_NOT_NULL(stored);
    TEST_ASSERT_EQUAL_STRING("Test AP", stored->name.c_str());
    TEST_ASSERT_EQUAL_INT16(-45, stored->rssi);
    TEST_ASSERT_EQUAL_UINT32(1, stored->sightings);
    TEST_ASSERT_TRUE(stored->seenInCurrentScan);

    // An observation alone does not establish successful scan completion.
    TEST_ASSERT_FALSE(stored->currentlyVisible);
}

void test_duplicate_identity_updates_existing_device() {
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Wifi));

    const Device first =
        makeDevice(RadioType::Wifi, "AA:BB:CC:DD:EE:04", "Test AP", -45, 100);

    TEST_ASSERT_TRUE(inventory.upsert(first));

    const Device second =
        makeDevice(RadioType::Wifi, "AA:BB:CC:DD:EE:04", "Test AP", -62, 200);

    TEST_ASSERT_TRUE(inventory.upsert(second));
    TEST_ASSERT_EQUAL_UINT32(1, inventory.size());

    const Device* stored =
        inventory.find(RadioType::Wifi, "AA:BB:CC:DD:EE:04");

    TEST_ASSERT_NOT_NULL(stored);
    TEST_ASSERT_EQUAL_INT16(-62, stored->rssi);
    TEST_ASSERT_EQUAL_UINT32(100, stored->firstSeenMs);
    TEST_ASSERT_EQUAL_UINT32(200, stored->lastSeenMs);
    TEST_ASSERT_EQUAL_UINT32(2, stored->sightings);
}

void test_radio_type_is_part_of_identity() {
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Wifi));
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Ble));

    const char* sharedAddress = "AA:BB:CC:DD:EE:05";

    const Device wifi =
        makeDevice(RadioType::Wifi, sharedAddress, "Wi-Fi device", -40, 100);

    const Device ble =
        makeDevice(RadioType::Ble, sharedAddress, "BLE device", -55, 100);

    TEST_ASSERT_TRUE(inventory.upsert(wifi));
    TEST_ASSERT_TRUE(inventory.upsert(ble));

    TEST_ASSERT_EQUAL_UINT32(2, inventory.size());
    TEST_ASSERT_EQUAL_UINT32(1, inventory.count(RadioType::Wifi));
    TEST_ASSERT_EQUAL_UINT32(1, inventory.count(RadioType::Ble));

    TEST_ASSERT_NOT_NULL(inventory.find(RadioType::Wifi, sharedAddress));
    TEST_ASSERT_NOT_NULL(inventory.find(RadioType::Ble, sharedAddress));
}

void test_empty_name_does_not_erase_known_name() {
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Ble));

    const Device named =
        makeDevice(RadioType::Ble, "AA:BB:CC:DD:EE:06", "Known device", -50, 100);

    const Device unnamed =
        makeDevice(RadioType::Ble, "AA:BB:CC:DD:EE:06", "", -58, 200);

    TEST_ASSERT_TRUE(inventory.upsert(named));
    TEST_ASSERT_TRUE(inventory.upsert(unnamed));

    const Device* stored =
        inventory.find(RadioType::Ble, "AA:BB:CC:DD:EE:06");

    TEST_ASSERT_NOT_NULL(stored);
    TEST_ASSERT_EQUAL_STRING("Known device", stored->name.c_str());
    TEST_ASSERT_EQUAL_INT16(-58, stored->rssi);
    TEST_ASSERT_EQUAL_UINT32(2, stored->sightings);
}

void test_complete_scan_marks_observed_devices_visible() {
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Wifi));

    const Device device =
        makeDevice(RadioType::Wifi, "AA:BB:CC:DD:EE:07", "Test AP", -45, 100);

    TEST_ASSERT_TRUE(inventory.upsert(device));
    inventory.completeScan(RadioType::Wifi);

    const Device* stored =
        inventory.find(RadioType::Wifi, "AA:BB:CC:DD:EE:07");

    TEST_ASSERT_NOT_NULL(stored);
    TEST_ASSERT_TRUE(stored->currentlyVisible);
    TEST_ASSERT_TRUE(stored->seenInCurrentScan);
    TEST_ASSERT_FALSE(inventory.scanInProgress(RadioType::Wifi));
    TEST_ASSERT_EQUAL_UINT32(1, inventory.generation());
}

void test_successful_empty_scan_marks_previous_devices_missing() {
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Wifi));

    const Device device =
        makeDevice(RadioType::Wifi, "AA:BB:CC:DD:EE:08", "Test AP", -45, 100);

    TEST_ASSERT_TRUE(inventory.upsert(device));
    inventory.completeScan(RadioType::Wifi);

    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Wifi));

    // No observations were received in the second successful scan.
    inventory.completeScan(RadioType::Wifi);

    const Device* stored =
        inventory.find(RadioType::Wifi, "AA:BB:CC:DD:EE:08");

    TEST_ASSERT_NOT_NULL(stored);
    TEST_ASSERT_FALSE(stored->currentlyVisible);
    TEST_ASSERT_EQUAL_UINT32(2, inventory.generation());
}

void test_cancelled_scan_preserves_previous_visibility() {
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Wifi));

    const Device device =
        makeDevice(RadioType::Wifi, "AA:BB:CC:DD:EE:09", "Test AP", -45, 100);

    TEST_ASSERT_TRUE(inventory.upsert(device));
    inventory.completeScan(RadioType::Wifi);

    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Wifi));

    inventory.cancelScan(RadioType::Wifi);

    const Device* stored =
        inventory.find(RadioType::Wifi, "AA:BB:CC:DD:EE:09");

    TEST_ASSERT_NOT_NULL(stored);
    TEST_ASSERT_TRUE(stored->currentlyVisible);
    TEST_ASSERT_FALSE(inventory.scanInProgress(RadioType::Wifi));
    TEST_ASSERT_EQUAL_UINT32(1, inventory.generation());
}

void test_partial_observation_does_not_make_new_device_visible() {
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Ble));

    const Device device =
        makeDevice(RadioType::Ble, "AA:BB:CC:DD:EE:10", "Partial", -60, 100);

    TEST_ASSERT_TRUE(inventory.upsert(device));
    inventory.cancelScan(RadioType::Ble);

    const Device* stored =
        inventory.find(RadioType::Ble, "AA:BB:CC:DD:EE:10");

    TEST_ASSERT_NOT_NULL(stored);
    TEST_ASSERT_FALSE(stored->currentlyVisible);
    TEST_ASSERT_EQUAL_UINT32(0, inventory.generation());
}

void test_completion_without_active_scan_does_nothing() {
    inventory.completeScan(RadioType::Wifi);

    TEST_ASSERT_EQUAL_UINT32(0, inventory.generation());
    TEST_ASSERT_FALSE(inventory.scanInProgress(RadioType::Wifi));
}

void test_cancel_without_active_scan_does_nothing() {
    inventory.cancelScan(RadioType::Wifi);

    TEST_ASSERT_EQUAL_UINT32(0, inventory.generation());
    TEST_ASSERT_FALSE(inventory.scanInProgress(RadioType::Wifi));
}

void test_scan_completion_is_independent_per_radio() {
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Wifi));
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Ble));

    const Device wifi =
        makeDevice(RadioType::Wifi, "AA:BB:CC:DD:EE:11", "Wi-Fi", -45, 100);

    const Device ble =
        makeDevice(RadioType::Ble, "AA:BB:CC:DD:EE:12", "BLE", -55, 100);

    TEST_ASSERT_TRUE(inventory.upsert(wifi));
    TEST_ASSERT_TRUE(inventory.upsert(ble));

    inventory.completeScan(RadioType::Wifi);

    TEST_ASSERT_FALSE(inventory.scanInProgress(RadioType::Wifi));
    TEST_ASSERT_TRUE(inventory.scanInProgress(RadioType::Ble));
    TEST_ASSERT_EQUAL_UINT32(1, inventory.generation());

    inventory.completeScan(RadioType::Ble);

    TEST_ASSERT_FALSE(inventory.scanInProgress(RadioType::Ble));
    TEST_ASSERT_EQUAL_UINT32(2, inventory.generation());
}

void test_find_and_at_handle_missing_entries() {
    TEST_ASSERT_NULL(inventory.at(0));
    TEST_ASSERT_NULL(inventory.find(RadioType::Wifi, "missing"));
    TEST_ASSERT_NULL(inventory.find(RadioType::Wifi, nullptr));
}

void test_copy_radio_respects_capacity_and_radio() {
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Wifi));
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Ble));

    TEST_ASSERT_TRUE(inventory.upsert(
        makeDevice(RadioType::Wifi, "AA:BB:CC:DD:EE:13", "Wi-Fi 1", -40, 100)));

    TEST_ASSERT_TRUE(inventory.upsert(
        makeDevice(RadioType::Wifi, "AA:BB:CC:DD:EE:14", "Wi-Fi 2", -50, 100)));

    TEST_ASSERT_TRUE(inventory.upsert(
        makeDevice(RadioType::Ble, "AA:BB:CC:DD:EE:15", "BLE 1", -60, 100)));

    Device output[1];

    TEST_ASSERT_EQUAL_UINT32(
        1, inventory.copyRadio(RadioType::Wifi, output, 1));

    TEST_ASSERT_EQUAL_STRING("Wi-Fi 1", output[0].name.c_str());

    TEST_ASSERT_EQUAL_UINT32(
        0, inventory.copyRadio(RadioType::Wifi, nullptr, 1));

    TEST_ASSERT_EQUAL_UINT32(
        0, inventory.copyRadio(RadioType::Wifi, output, 0));

    TEST_ASSERT_EQUAL_UINT32(
        0, inventory.copyRadio(static_cast<RadioType>(99), output, 1));
}

void test_capacity_limit_is_enforced() {
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Wifi));

    for (size_t i = 0; i < DeviceInventory::kCapacity; ++i) {
        const std::string address = "wifi-" + std::to_string(i);

        const Device device =
            makeDevice(RadioType::Wifi, address.c_str(), "AP", -50, 100);

        TEST_ASSERT_TRUE(inventory.upsert(device));
    }

    TEST_ASSERT_EQUAL_UINT32(DeviceInventory::kCapacity, inventory.size());

    const Device overflow =
        makeDevice(RadioType::Wifi, "wifi-overflow", "Overflow", -50, 200);

    TEST_ASSERT_FALSE(inventory.upsert(overflow));
    TEST_ASSERT_EQUAL_UINT32(DeviceInventory::kCapacity, inventory.size());
}

} // namespace

// Unity requires externally linked fixture functions.
void setUp() {
    inventory = DeviceInventory{};
}

void tearDown() {
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_inventory_starts_empty);
    RUN_TEST(test_scan_must_begin_before_observation);
    RUN_TEST(test_begin_scan_rejects_duplicate_scan);
    RUN_TEST(test_invalid_radio_is_rejected);
    RUN_TEST(test_empty_address_is_rejected);
    RUN_TEST(test_insert_creates_one_device);
    RUN_TEST(test_duplicate_identity_updates_existing_device);
    RUN_TEST(test_radio_type_is_part_of_identity);
    RUN_TEST(test_empty_name_does_not_erase_known_name);
    RUN_TEST(test_complete_scan_marks_observed_devices_visible);
    RUN_TEST(test_successful_empty_scan_marks_previous_devices_missing);
    RUN_TEST(test_cancelled_scan_preserves_previous_visibility);
    RUN_TEST(test_partial_observation_does_not_make_new_device_visible);
    RUN_TEST(test_completion_without_active_scan_does_nothing);
    RUN_TEST(test_cancel_without_active_scan_does_nothing);
    RUN_TEST(test_scan_completion_is_independent_per_radio);
    RUN_TEST(test_find_and_at_handle_missing_entries);
    RUN_TEST(test_copy_radio_respects_capacity_and_radio);
    RUN_TEST(test_capacity_limit_is_enforced);

    return UNITY_END();
}
