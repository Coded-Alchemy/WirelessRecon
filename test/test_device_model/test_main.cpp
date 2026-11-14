//
// Created by dedlee on 11/13/26.
//


#include <unity.h>

#include "core/Device.h"

#include <cstdint>

using namespace wirelessrecon::core;

namespace {

void test_device_default_values() {
    const Device device;

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RadioType::Wifi),
        static_cast<uint8_t>(device.radio));

    TEST_ASSERT_TRUE(device.address.empty());
    TEST_ASSERT_TRUE(device.name.empty());

    TEST_ASSERT_EQUAL_INT16(-127, device.rssi);
    TEST_ASSERT_FALSE(device.hasRssi);

    TEST_ASSERT_EQUAL_UINT32(0, device.firstSeenMs);
    TEST_ASSERT_EQUAL_UINT32(0, device.lastSeenMs);
    TEST_ASSERT_EQUAL_UINT32(0, device.sightings);

    TEST_ASSERT_FALSE(device.seenInCurrentScan);
    TEST_ASSERT_FALSE(device.currentlyVisible);
}

void test_wifi_metadata_default_values() {
    const WifiMetadata metadata;

    TEST_ASSERT_TRUE(metadata.ssid.empty());
    TEST_ASSERT_EQUAL_INT16(0, metadata.channel);
    TEST_ASSERT_EQUAL_UINT8(0, metadata.encryption);
    TEST_ASSERT_FALSE(metadata.hidden);
    TEST_ASSERT_FALSE(metadata.duplicateSsid);
}

void test_radio_type_values_are_stable() {
    TEST_ASSERT_EQUAL_UINT8(0, static_cast<uint8_t>(RadioType::Wifi));
    TEST_ASSERT_EQUAL_UINT8(1, static_cast<uint8_t>(RadioType::Ble));
    TEST_ASSERT_EQUAL_UINT8(
        2,
        static_cast<uint8_t>(RadioType::BluetoothClassic));
}

void test_same_identity_matches_same_radio_and_address() {
    Device first;
    first.radio = RadioType::Wifi;
    first.address = "AA:BB:CC:DD:EE:01";
    first.name = "First name";
    first.rssi = -40;

    Device second;
    second.radio = RadioType::Wifi;
    second.address = "AA:BB:CC:DD:EE:01";
    second.name = "Different name";
    second.rssi = -70;

    TEST_ASSERT_TRUE(sameIdentity(first, second));
    TEST_ASSERT_TRUE(sameIdentity(second, first));
}

void test_same_address_on_different_radios_is_not_same_identity() {
    Device wifi;
    wifi.radio = RadioType::Wifi;
    wifi.address = "AA:BB:CC:DD:EE:02";

    Device ble;
    ble.radio = RadioType::Ble;
    ble.address = "AA:BB:CC:DD:EE:02";

    Device classic;
    classic.radio = RadioType::BluetoothClassic;
    classic.address = "AA:BB:CC:DD:EE:02";

    TEST_ASSERT_FALSE(sameIdentity(wifi, ble));
    TEST_ASSERT_FALSE(sameIdentity(wifi, classic));
    TEST_ASSERT_FALSE(sameIdentity(ble, classic));
}

void test_different_addresses_on_same_radio_are_not_same_identity() {
    Device first;
    first.radio = RadioType::Ble;
    first.address = "AA:BB:CC:DD:EE:03";

    Device second;
    second.radio = RadioType::Ble;
    second.address = "AA:BB:CC:DD:EE:04";

    TEST_ASSERT_FALSE(sameIdentity(first, second));
}

} // namespace

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_device_default_values);
    RUN_TEST(test_wifi_metadata_default_values);
    RUN_TEST(test_radio_type_values_are_stable);
    RUN_TEST(test_same_identity_matches_same_radio_and_address);
    RUN_TEST(test_same_address_on_different_radios_is_not_same_identity);
    RUN_TEST(test_different_addresses_on_same_radio_are_not_same_identity);

    return UNITY_END();
}
