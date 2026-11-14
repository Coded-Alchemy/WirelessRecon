#include <unity.h>

#include "Arduino.h"
#include "WiFi.h"
#include "core/ObservationQueue.h"
#include "hardware/radio/WifiScanner.h"

#include <array>
#include <string>
#include <utility>

using namespace wirelessrecon::core;
using wirelessrecon::hardware::radio::WifiScanner;

void setUp() {
    WiFi.reset();
    fake_platform::resetClock();
}

namespace {

void test_wifi_scanner_reports_radio_and_starts_async_station_scan() {
    ObservationQueue queue;
    WifiScanner scanner(queue);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(RadioType::Wifi),
                          static_cast<int>(scanner.radio()));
    TEST_ASSERT_TRUE(scanner.start());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Running),
                          static_cast<int>(scanner.state()));
    TEST_ASSERT_TRUE(WiFi.modeCalled);
    TEST_ASSERT_EQUAL_INT(WIFI_STA, WiFi.modeArgument);
    TEST_ASSERT_EQUAL_INT(1, WiFi.disconnectCalls);
    TEST_ASSERT_TRUE(WiFi.asyncArgument);
    TEST_ASSERT_TRUE(WiFi.showHiddenArgument);
    TEST_ASSERT_FALSE(WiFi.passiveArgument);
    TEST_ASSERT_EQUAL_UINT32(600, WiFi.maxMsPerChannelArgument);
    TEST_ASSERT_EQUAL_UINT32(100, fake_platform::delayTotalMs);
    TEST_ASSERT_TRUE(scanner.diagnostics().stationModeReady);
}

void test_wifi_start_fails_when_station_mode_fails() {
    ObservationQueue queue;
    WifiScanner scanner(queue);
    WiFi.modeResult = false;

    TEST_ASSERT_FALSE(scanner.start());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Failed),
                          static_cast<int>(scanner.state()));
    TEST_ASSERT_FALSE(scanner.diagnostics().stationModeReady);
    TEST_ASSERT_EQUAL_INT(0, WiFi.scanNetworksCalls);
}

void test_wifi_start_fails_when_scan_api_rejects_request() {
    ObservationQueue queue;
    WifiScanner scanner(queue);
    WiFi.startResult = WIFI_SCAN_FAILED;

    TEST_ASSERT_FALSE(scanner.start());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Failed),
                          static_cast<int>(scanner.state()));
    TEST_ASSERT_EQUAL_INT(WIFI_SCAN_FAILED,
                          scanner.diagnostics().scanStartResult);
}

void test_wifi_scan_collects_and_maps_network_metadata() {
    ObservationQueue queue;
    WifiScanner scanner(queue);

    auto first = fake_platform::makeWifiNetwork(
        "LabNet", {{0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x01}}, -42, 11, 4);
    auto second = fake_platform::makeWifiNetwork(
        "LabNet", {{0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x02}}, -67, 1, 3);
    auto hidden = fake_platform::makeWifiNetwork(
        "", {{0x10, 0x20, 0x30, 0x40, 0x50, 0x60}}, -80, 6, 0);
    WiFi.networks = {first, second, hidden};

    TEST_ASSERT_TRUE(scanner.start());
    WiFi.completionResult = 3;
    scanner.poll();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Completed),
                          static_cast<int>(scanner.state()));
    TEST_ASSERT_EQUAL_UINT32(3, scanner.diagnostics().networkCount);
    TEST_ASSERT_EQUAL_UINT32(3, scanner.diagnostics().observationsQueued);
    TEST_ASSERT_EQUAL_UINT32(0, scanner.diagnostics().observationsRejected);
    TEST_ASSERT_EQUAL_UINT32(3, queue.size());

    Device device;
    TEST_ASSERT_TRUE(queue.tryPop(device));
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:00:00:01", device.address.c_str());
    TEST_ASSERT_EQUAL_STRING("LabNet", device.name.c_str());
    TEST_ASSERT_EQUAL_INT(-42, device.rssi);
    TEST_ASSERT_TRUE(device.hasRssi);
    TEST_ASSERT_EQUAL_STRING("LabNet", device.wifi.ssid.c_str());
    TEST_ASSERT_EQUAL_INT(11, device.wifi.channel);
    TEST_ASSERT_EQUAL_INT(4, device.wifi.encryption);
    TEST_ASSERT_TRUE(device.wifi.duplicateSsid);
    TEST_ASSERT_FALSE(device.wifi.hidden);
    TEST_ASSERT_EQUAL_UINT32(123556, device.firstSeenMs);
    TEST_ASSERT_EQUAL_UINT32(device.firstSeenMs, device.lastSeenMs);

    TEST_ASSERT_TRUE(queue.tryPop(device));
    TEST_ASSERT_TRUE(device.wifi.duplicateSsid);

    TEST_ASSERT_TRUE(queue.tryPop(device));
    TEST_ASSERT_EQUAL_STRING("Hidden network", device.name.c_str());
    TEST_ASSERT_TRUE(device.wifi.hidden);
    TEST_ASSERT_EQUAL_STRING("", device.wifi.ssid.c_str());
}

void test_wifi_skips_unavailable_network_info_and_null_bssid() {
    ObservationQueue queue;
    WifiScanner scanner(queue);

    auto unavailable = fake_platform::makeWifiNetwork(
        "Skip1", {{1, 2, 3, 4, 5, 6}});
    unavailable.infoAvailable = false;

    auto noBssid = fake_platform::makeWifiNetwork(
        "Skip2", {{1, 2, 3, 4, 5, 7}});
    noBssid.nullBssid = true;

    auto valid = fake_platform::makeWifiNetwork(
        "Keep", {{1, 2, 3, 4, 5, 8}});
    WiFi.networks = {unavailable, noBssid, valid};

    TEST_ASSERT_TRUE(scanner.start());
    WiFi.completionResult = 3;
    scanner.poll();

    TEST_ASSERT_EQUAL_UINT32(1, queue.size());
    TEST_ASSERT_EQUAL_UINT32(1, scanner.diagnostics().observationsQueued);
    Device device;
    TEST_ASSERT_TRUE(queue.tryPop(device));
    TEST_ASSERT_EQUAL_STRING("Keep", device.name.c_str());
}

void test_wifi_scan_waits_while_running_then_completes() {
    ObservationQueue queue;
    WifiScanner scanner(queue);
    WiFi.networks.push_back(fake_platform::makeWifiNetwork(
        "One", {{1, 1, 1, 1, 1, 1}}));

    TEST_ASSERT_TRUE(scanner.start());
    scanner.poll();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Running),
                          static_cast<int>(scanner.state()));
    TEST_ASSERT_EQUAL_UINT32(0, queue.size());

    WiFi.completionResult = 1;
    scanner.poll();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Completed),
                          static_cast<int>(scanner.state()));
    TEST_ASSERT_EQUAL_UINT32(1, queue.size());
}

void test_wifi_scan_failure_after_start_is_terminal() {
    ObservationQueue queue;
    WifiScanner scanner(queue);
    TEST_ASSERT_TRUE(scanner.start());

    WiFi.completionResult = WIFI_SCAN_FAILED;
    scanner.poll();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Failed),
                          static_cast<int>(scanner.state()));
    TEST_ASSERT_EQUAL_INT(WIFI_SCAN_FAILED,
                          scanner.diagnostics().scanCompletionResult);
    TEST_ASSERT_EQUAL_UINT32(0, queue.size());
}

void test_wifi_stop_discards_results_and_returns_to_idle_after_poll() {
    ObservationQueue queue;
    WifiScanner scanner(queue);
    WiFi.networks.push_back(fake_platform::makeWifiNetwork(
        "Cancelled", {{1, 2, 3, 4, 5, 6}}));

    TEST_ASSERT_TRUE(scanner.start());
    scanner.stop();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Stopping),
                          static_cast<int>(scanner.state()));

    WiFi.completionResult = 1;
    scanner.poll();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Idle),
                          static_cast<int>(scanner.state()));
    TEST_ASSERT_EQUAL_UINT32(0, queue.size());
    TEST_ASSERT_EQUAL_UINT32(0, scanner.diagnostics().observationsQueued);
}

void test_wifi_reset_only_resets_terminal_states() {
    ObservationQueue queue;
    WifiScanner scanner(queue);

    scanner.reset();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Idle),
                          static_cast<int>(scanner.state()));

    TEST_ASSERT_TRUE(scanner.start());
    scanner.reset();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Running),
                          static_cast<int>(scanner.state()));

    WiFi.completionResult = 0;
    scanner.poll();
    scanner.reset();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Idle),
                          static_cast<int>(scanner.state()));
}

void test_wifi_rejects_start_while_not_idle() {
    ObservationQueue queue;
    WifiScanner scanner(queue);
    TEST_ASSERT_TRUE(scanner.start());
    TEST_ASSERT_FALSE(scanner.start());
    TEST_ASSERT_EQUAL_INT(1, WiFi.scanNetworksCalls);
}

void test_wifi_queue_overflow_is_reported() {
    ObservationQueue queue;
    for (std::size_t i = 0; i < ObservationQueue::kCapacity; ++i) {
        Device filler;
        filler.address = "FILL_" + std::to_string(i);
        TEST_ASSERT_TRUE(queue.tryPush(std::move(filler)));
    }

    WifiScanner scanner(queue);
    WiFi.networks.push_back(fake_platform::makeWifiNetwork(
        "Overflow", {{1, 2, 3, 4, 5, 6}}));
    TEST_ASSERT_TRUE(scanner.start());
    WiFi.completionResult = 1;
    scanner.poll();

    TEST_ASSERT_EQUAL_UINT32(ObservationQueue::kCapacity, queue.size());
    TEST_ASSERT_EQUAL_UINT32(0, scanner.diagnostics().observationsQueued);
    TEST_ASSERT_EQUAL_UINT32(1, scanner.diagnostics().observationsRejected);
    TEST_ASSERT_EQUAL_UINT32(1, queue.droppedCount());
}

} // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_wifi_scanner_reports_radio_and_starts_async_station_scan);
    RUN_TEST(test_wifi_start_fails_when_station_mode_fails);
    RUN_TEST(test_wifi_start_fails_when_scan_api_rejects_request);
    RUN_TEST(test_wifi_scan_collects_and_maps_network_metadata);
    RUN_TEST(test_wifi_skips_unavailable_network_info_and_null_bssid);
    RUN_TEST(test_wifi_scan_waits_while_running_then_completes);
    RUN_TEST(test_wifi_scan_failure_after_start_is_terminal);
    RUN_TEST(test_wifi_stop_discards_results_and_returns_to_idle_after_poll);
    RUN_TEST(test_wifi_reset_only_resets_terminal_states);
    RUN_TEST(test_wifi_rejects_start_while_not_idle);
    RUN_TEST(test_wifi_queue_overflow_is_reported);
    return UNITY_END();
}
