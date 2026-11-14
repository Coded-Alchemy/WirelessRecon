
#include <unity.h>

#include "Arduino.h"
#include "BLEDevice.h"
#include "core/ObservationQueue.h"
#include "hardware/radio/BleScanner.h"

#include <string>
#include <utility>

using namespace wirelessrecon::core;
using wirelessrecon::hardware::radio::BleScanner;

void setUp() {
    fake_platform::resetBle();
    fake_platform::resetClock();
}

namespace {

void test_ble_scanner_reports_radio_and_initializes_bluedroid() {
    ObservationQueue queue;
    BleScanner scanner(queue);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(RadioType::Ble),
                          static_cast<int>(scanner.radio()));
    TEST_ASSERT_TRUE(scanner.start());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Running),
                          static_cast<int>(scanner.state()));
    TEST_ASSERT_EQUAL_INT(1, fake_platform::bleInitCalls);
    TEST_ASSERT_EQUAL_UINT32(250, fake_platform::delayTotalMs);

    const auto diagnostics = scanner.diagnostics();
    TEST_ASSERT_TRUE(diagnostics.initializationAttempted);
    TEST_ASSERT_TRUE(diagnostics.controllerReady);
    TEST_ASSERT_TRUE(diagnostics.hostReady);
    TEST_ASSERT_TRUE(diagnostics.scanStarted);
    TEST_ASSERT_EQUAL_INT(2, diagnostics.controllerStatus);
    TEST_ASSERT_EQUAL_INT(2, diagnostics.hostStatus);

    TEST_ASSERT_TRUE(fake_platform::bleScan.activeScan);
    TEST_ASSERT_EQUAL_UINT32(10, fake_platform::bleScan.durationSeconds);
    TEST_ASSERT_EQUAL_INT(100, fake_platform::bleScan.interval);
    TEST_ASSERT_EQUAL_INT(80, fake_platform::bleScan.window);
    TEST_ASSERT_FALSE(fake_platform::bleScan.wantDuplicates);
    TEST_ASSERT_TRUE(fake_platform::bleScan.shouldParse);

    // Release the static active-scanner slot before scanner destruction.
    fake_platform::bleScan.complete();
}

void test_ble_scan_converts_advertisements_into_observations() {
    ObservationQueue queue;
    BleScanner scanner(queue);
    TEST_ASSERT_TRUE(scanner.start());

    fake_platform::bleScan.emit(
        BLEAdvertisedDevice("AA:BB:CC:DD:EE:01", "Sensor One", -48));
    fake_platform::bleScan.emit(
        BLEAdvertisedDevice("AA:BB:CC:DD:EE:02", "", -127, false));

    TEST_ASSERT_EQUAL_UINT32(2, queue.size());
    auto diagnostics = scanner.diagnostics();
    TEST_ASSERT_EQUAL_UINT32(2, diagnostics.devicesDiscovered);
    TEST_ASSERT_EQUAL_UINT32(2, diagnostics.observationsQueued);
    TEST_ASSERT_EQUAL_UINT32(0, diagnostics.observationsRejected);

    Device device;
    TEST_ASSERT_TRUE(queue.tryPop(device));
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:01", device.address.c_str());
    TEST_ASSERT_EQUAL_STRING("Sensor One", device.name.c_str());
    TEST_ASSERT_EQUAL_INT(-48, device.rssi);
    TEST_ASSERT_TRUE(device.hasRssi);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RadioType::Ble),
                          static_cast<int>(device.radio));
    TEST_ASSERT_EQUAL_UINT32(123706, device.firstSeenMs);
    TEST_ASSERT_EQUAL_UINT32(device.firstSeenMs, device.lastSeenMs);

    TEST_ASSERT_TRUE(queue.tryPop(device));
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:02", device.address.c_str());
    TEST_ASSERT_EQUAL_STRING("", device.name.c_str());
    TEST_ASSERT_FALSE(device.hasRssi);

    fake_platform::bleScan.complete();
}

void test_ble_scan_completion_transitions_to_completed_and_resettable() {
    ObservationQueue queue;
    BleScanner scanner(queue);
    TEST_ASSERT_TRUE(scanner.start());

    fake_platform::bleScan.emit(
        BLEAdvertisedDevice("AA:00:00:00:00:01", "Tag", -60));
    fake_platform::bleScan.complete();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Completed),
                          static_cast<int>(scanner.state()));
    TEST_ASSERT_TRUE(scanner.diagnostics().scanStarted);

    scanner.reset();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Idle),
                          static_cast<int>(scanner.state()));
    TEST_ASSERT_EQUAL_UINT32(2, fake_platform::bleScan.clearResultsCalls);
}

void test_ble_stop_waits_for_completion_callback_before_idle() {
    ObservationQueue queue;
    BleScanner scanner(queue);
    TEST_ASSERT_TRUE(scanner.start());

    scanner.stop();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Stopping),
                          static_cast<int>(scanner.state()));
    TEST_ASSERT_TRUE(fake_platform::bleScan.stopCalled);

    fake_platform::bleScan.complete();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Idle),
                          static_cast<int>(scanner.state()));
    TEST_ASSERT_TRUE(scanner.diagnostics().scanStarted);
}

void test_ble_rejects_start_while_scanner_is_busy() {
    ObservationQueue queue;
    BleScanner scanner(queue);
    TEST_ASSERT_TRUE(scanner.start());
    TEST_ASSERT_FALSE(scanner.start());
    TEST_ASSERT_EQUAL_INT(1, fake_platform::bleInitCalls);

    fake_platform::bleScan.complete();
}

void test_ble_start_fails_when_scan_object_is_unavailable() {
    ObservationQueue queue;
    BleScanner scanner(queue);
    fake_platform::bleScanAvailable = false;

    TEST_ASSERT_FALSE(scanner.start());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Failed),
                          static_cast<int>(scanner.state()));
    TEST_ASSERT_FALSE(scanner.diagnostics().scanStarted);
}

void test_ble_start_failure_releases_active_scan_slot() {
    ObservationQueue firstQueue;
    BleScanner first(firstQueue);
    fake_platform::bleScan.startResult = false;

    TEST_ASSERT_FALSE(first.start());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Failed),
                          static_cast<int>(first.state()));

    fake_platform::bleScan.startResult = true;
    ObservationQueue secondQueue;
    BleScanner second(secondQueue);
    TEST_ASSERT_TRUE(second.start());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Running),
                          static_cast<int>(second.state()));

    fake_platform::bleScan.complete();
}

void test_ble_queue_overflow_is_counted_without_overwriting_queue() {
    ObservationQueue queue;
    for (std::size_t i = 0; i < ObservationQueue::kCapacity; ++i) {
        Device filler;
        filler.address = "FILL_" + std::to_string(i);
        TEST_ASSERT_TRUE(queue.tryPush(std::move(filler)));
    }

    BleScanner scanner(queue);
    TEST_ASSERT_TRUE(scanner.start());
    fake_platform::bleScan.emit(
        BLEAdvertisedDevice("AA:BB:CC:DD:EE:FF", "Overflow", -70));

    const auto diagnostics = scanner.diagnostics();
    TEST_ASSERT_EQUAL_UINT32(1, diagnostics.devicesDiscovered);
    TEST_ASSERT_EQUAL_UINT32(0, diagnostics.observationsQueued);
    TEST_ASSERT_EQUAL_UINT32(1, diagnostics.observationsRejected);
    TEST_ASSERT_EQUAL_UINT32(1, queue.droppedCount());
    TEST_ASSERT_EQUAL_UINT32(ObservationQueue::kCapacity, queue.size());

    fake_platform::bleScan.complete();
}

void test_ble_reset_does_not_reset_running_scanner() {
    ObservationQueue queue;
    BleScanner scanner(queue);
    TEST_ASSERT_TRUE(scanner.start());

    scanner.reset();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Running),
                          static_cast<int>(scanner.state()));

    fake_platform::bleScan.complete();
    scanner.reset();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScannerState::Idle),
                          static_cast<int>(scanner.state()));
}

} // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_ble_scanner_reports_radio_and_initializes_bluedroid);
    RUN_TEST(test_ble_scan_converts_advertisements_into_observations);
    RUN_TEST(test_ble_scan_completion_transitions_to_completed_and_resettable);
    RUN_TEST(test_ble_stop_waits_for_completion_callback_before_idle);
    RUN_TEST(test_ble_rejects_start_while_scanner_is_busy);
    RUN_TEST(test_ble_start_fails_when_scan_object_is_unavailable);
    RUN_TEST(test_ble_start_failure_releases_active_scan_slot);
    RUN_TEST(test_ble_queue_overflow_is_counted_without_overwriting_queue);
    RUN_TEST(test_ble_reset_does_not_reset_running_scanner);
    return UNITY_END();
}
