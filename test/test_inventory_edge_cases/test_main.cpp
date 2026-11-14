//
// Created by dedlee on 11/13/26.
//

#include <unity.h>

#include "core/DeviceInventory.h"

#include <cstdint>
#include <string>

using namespace wirelessrecon::core;

namespace {

DeviceInventory inventory;

Device makeDevice(
    RadioType radio,
    const std::string& address,
    const char* name,
    int16_t rssi,
    uint32_t firstSeen,
    uint32_t lastSeen) {

    Device device;
    device.radio = radio;
    device.address = address;
    device.name = name;
    device.rssi = rssi;
    device.hasRssi = true;
    device.firstSeenMs = firstSeen;
    device.lastSeenMs = lastSeen;

    return device;
}

void test_first_seen_falls_back_to_last_seen() {
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Wifi));

    const Device observation = makeDevice(
        RadioType::Wifi,
        "AA:BB:CC:DD:EE:01",
        "Fallback AP",
        -48,
        0,
        1234);

    TEST_ASSERT_TRUE(inventory.upsert(observation));

    const Device* stored =
        inventory.find(RadioType::Wifi, "AA:BB:CC:DD:EE:01");

    TEST_ASSERT_NOT_NULL(stored);
    TEST_ASSERT_EQUAL_UINT32(1234, stored->firstSeenMs);
    TEST_ASSERT_EQUAL_UINT32(1234, stored->lastSeenMs);
}

void test_wifi_metadata_is_replaced_on_update() {
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Wifi));

    Device first = makeDevice(
        RadioType::Wifi,
        "AA:BB:CC:DD:EE:02",
        "Access Point",
        -45,
        100,
        100);

    first.wifi.ssid = "OriginalNetwork";
    first.wifi.channel = 1;
    first.wifi.encryption = 3;
    first.wifi.hidden = false;
    first.wifi.duplicateSsid = false;

    TEST_ASSERT_TRUE(inventory.upsert(first));

    Device second = makeDevice(
        RadioType::Wifi,
        "AA:BB:CC:DD:EE:02",
        "Access Point",
        -61,
        200,
        300);

    second.wifi.ssid = "UpdatedNetwork";
    second.wifi.channel = 11;
    second.wifi.encryption = 4;
    second.wifi.hidden = true;
    second.wifi.duplicateSsid = true;

    TEST_ASSERT_TRUE(inventory.upsert(second));

    const Device* stored =
        inventory.find(RadioType::Wifi, "AA:BB:CC:DD:EE:02");

    TEST_ASSERT_NOT_NULL(stored);

    TEST_ASSERT_EQUAL_STRING(
        "UpdatedNetwork",
        stored->wifi.ssid.c_str());

    TEST_ASSERT_EQUAL_INT16(11, stored->wifi.channel);
    TEST_ASSERT_EQUAL_UINT8(4, stored->wifi.encryption);
    TEST_ASSERT_TRUE(stored->wifi.hidden);
    TEST_ASSERT_TRUE(stored->wifi.duplicateSsid);

    TEST_ASSERT_EQUAL_INT16(-61, stored->rssi);
    TEST_ASSERT_EQUAL_UINT32(100, stored->firstSeenMs);
    TEST_ASSERT_EQUAL_UINT32(300, stored->lastSeenMs);
    TEST_ASSERT_EQUAL_UINT32(2, stored->sightings);
}

void test_existing_device_can_be_updated_at_capacity() {
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Wifi));

    for (size_t i = 0; i < DeviceInventory::kCapacity; ++i) {
        const std::string address = "capacity-" + std::to_string(i);

        const Device observation = makeDevice(
            RadioType::Wifi,
            address,
            "Access Point",
            -50,
            100,
            100);

        TEST_ASSERT_TRUE(inventory.upsert(observation));
    }

    TEST_ASSERT_EQUAL_UINT32(
        DeviceInventory::kCapacity,
        inventory.size());

    const Device update = makeDevice(
        RadioType::Wifi,
        "capacity-0",
        "Updated Access Point",
        -72,
        200,
        300);

    TEST_ASSERT_TRUE(inventory.upsert(update));

    TEST_ASSERT_EQUAL_UINT32(
        DeviceInventory::kCapacity,
        inventory.size());

    const Device* stored =
        inventory.find(RadioType::Wifi, "capacity-0");

    TEST_ASSERT_NOT_NULL(stored);
    TEST_ASSERT_EQUAL_STRING(
        "Updated Access Point",
        stored->name.c_str());

    TEST_ASSERT_EQUAL_INT16(-72, stored->rssi);
    TEST_ASSERT_EQUAL_UINT32(100, stored->firstSeenMs);
    TEST_ASSERT_EQUAL_UINT32(300, stored->lastSeenMs);
    TEST_ASSERT_EQUAL_UINT32(2, stored->sightings);
}

void test_invalid_radio_is_rejected_by_public_operations() {
    const RadioType invalidRadio = static_cast<RadioType>(99);

    Device output[1];

    TEST_ASSERT_FALSE(inventory.beginScan(invalidRadio));
    TEST_ASSERT_FALSE(inventory.scanInProgress(invalidRadio));

    TEST_ASSERT_EQUAL_UINT32(0, inventory.count(invalidRadio));

    TEST_ASSERT_NULL(inventory.find(
        invalidRadio,
        "AA:BB:CC:DD:EE:03"));

    TEST_ASSERT_EQUAL_UINT32(
        0,
        inventory.copyRadio(invalidRadio, output, 1));

    inventory.completeScan(invalidRadio);
    inventory.cancelScan(invalidRadio);

    TEST_ASSERT_EQUAL_UINT32(0, inventory.generation());
    TEST_ASSERT_EQUAL_UINT32(0, inventory.size());

    Device invalidObservation = makeDevice(
        invalidRadio,
        "AA:BB:CC:DD:EE:04",
        "Invalid",
        -50,
        100,
        100);

    TEST_ASSERT_FALSE(inventory.upsert(invalidObservation));
}

void test_scan_lifecycle_is_independent_between_radios() {
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Wifi));
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Ble));

    const Device wifi = makeDevice(
        RadioType::Wifi,
        "AA:BB:CC:DD:EE:05",
        "Wi-Fi device",
        -40,
        100,
        100);

    const Device ble = makeDevice(
        RadioType::Ble,
        "AA:BB:CC:DD:EE:06",
        "BLE device",
        -60,
        100,
        100);

    TEST_ASSERT_TRUE(inventory.upsert(wifi));
    TEST_ASSERT_TRUE(inventory.upsert(ble));

    inventory.completeScan(RadioType::Wifi);

    TEST_ASSERT_FALSE(inventory.scanInProgress(RadioType::Wifi));
    TEST_ASSERT_TRUE(inventory.scanInProgress(RadioType::Ble));

    TEST_ASSERT_EQUAL_UINT32(1, inventory.generation());

    const Device* storedWifi =
        inventory.find(RadioType::Wifi, "AA:BB:CC:DD:EE:05");

    const Device* storedBle =
        inventory.find(RadioType::Ble, "AA:BB:CC:DD:EE:06");

    TEST_ASSERT_NOT_NULL(storedWifi);
    TEST_ASSERT_NOT_NULL(storedBle);

    TEST_ASSERT_TRUE(storedWifi->currentlyVisible);
    TEST_ASSERT_FALSE(storedBle->currentlyVisible);

    inventory.cancelScan(RadioType::Ble);

    TEST_ASSERT_FALSE(inventory.scanInProgress(RadioType::Ble));
    TEST_ASSERT_EQUAL_UINT32(1, inventory.generation());
}

void test_copy_radio_copies_all_matching_devices_in_inventory_order() {
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Wifi));
    TEST_ASSERT_TRUE(inventory.beginScan(RadioType::Ble));

    TEST_ASSERT_TRUE(inventory.upsert(makeDevice(
        RadioType::Wifi,
        "wifi-1",
        "Wi-Fi One",
        -40,
        100,
        100)));

    TEST_ASSERT_TRUE(inventory.upsert(makeDevice(
        RadioType::Ble,
        "ble-1",
        "BLE One",
        -50,
        100,
        100)));

    TEST_ASSERT_TRUE(inventory.upsert(makeDevice(
        RadioType::Wifi,
        "wifi-2",
        "Wi-Fi Two",
        -60,
        100,
        100)));

    Device output[3];

    const size_t copied =
        inventory.copyRadio(RadioType::Wifi, output, 3);

    TEST_ASSERT_EQUAL_UINT32(2, copied);
    TEST_ASSERT_EQUAL_STRING("Wi-Fi One", output[0].name.c_str());
    TEST_ASSERT_EQUAL_STRING("Wi-Fi Two", output[1].name.c_str());
}

} // namespace

void setUp() {
    inventory = DeviceInventory{};
}

void tearDown() {}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_first_seen_falls_back_to_last_seen);
    RUN_TEST(test_wifi_metadata_is_replaced_on_update);
    RUN_TEST(test_existing_device_can_be_updated_at_capacity);
    RUN_TEST(test_invalid_radio_is_rejected_by_public_operations);
    RUN_TEST(test_scan_lifecycle_is_independent_between_radios);
    RUN_TEST(test_copy_radio_copies_all_matching_devices_in_inventory_order);

    return UNITY_END();
}
