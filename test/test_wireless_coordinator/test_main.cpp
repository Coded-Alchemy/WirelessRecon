//
// Created by dedlee on 11/11/26.
//

#include <unity.h>

#include "core/WirelessCoordinator.h"

using namespace wirelessrecon::core;

namespace {

class FakeRadioScanner final : public IRadioScanner {
public:
    explicit FakeRadioScanner(RadioType radio)
        : radio_(radio) {
    }

    RadioType radio() const override {
        return radio_;
    }

    bool start() override {
        ++startCount_;

        if (failStart_) {
            state_ = ScannerState::Failed;
            return false;
        }

        if (state_ != ScannerState::Idle) {
            return false;
        }

        state_ = ScannerState::Running;
        return true;
    }

    void stop() override {
        ++stopCount_;

        if (state_ == ScannerState::Running) {
            state_ = ScannerState::Idle;
        }
    }

    ScannerState state() const override {
        return state_;
    }

    void reset() override {
        if (state_ == ScannerState::Completed ||
            state_ == ScannerState::Failed) {
            state_ = ScannerState::Idle;
        }
    }

    void complete() {
        state_ = ScannerState::Completed;
    }

    void fail() {
        state_ = ScannerState::Failed;
    }

    void setFailStart(bool value) {
        failStart_ = value;
    }

    int startCount() const {
        return startCount_;
    }

    int stopCount() const {
        return stopCount_;
    }

private:
    RadioType radio_;
    ScannerState state_ = ScannerState::Idle;
    bool failStart_ = false;
    int startCount_ = 0;
    int stopCount_ = 0;
};

struct TestContext {
    DeviceInventory inventory;
    ObservationQueue observations;
    RadioCapabilities capabilities;

    explicit TestContext(
        RadioCapabilities configuredCapabilities =
            {true, true, true, true})
        : capabilities(configuredCapabilities),
          coordinator(inventory, observations, capabilities) {
    }

    WirelessCoordinator coordinator;
};

Device makeObservation(
    RadioType radio,
    const char* address,
    const char* name = "Test Device") {

    Device device;
    device.radio = radio;
    device.address = address;
    device.name = name;
    device.rssi = -55;
    device.hasRssi = true;
    device.lastSeenMs = 1000;
    return device;
}

// Registration

void test_registers_supported_scanner() {
    TestContext context;
    FakeRadioScanner wifi(RadioType::Wifi);

    TEST_ASSERT_TRUE(context.coordinator.registerScanner(wifi));
}

void test_duplicate_scanner_registration_is_rejected() {
    TestContext context;
    FakeRadioScanner first(RadioType::Wifi);
    FakeRadioScanner second(RadioType::Wifi);

    TEST_ASSERT_TRUE(context.coordinator.registerScanner(first));
    TEST_ASSERT_FALSE(context.coordinator.registerScanner(second));
}

void test_unsupported_radio_is_rejected() {
    TestContext context({true, true, false, true});
    FakeRadioScanner classic(RadioType::BluetoothClassic);

    TEST_ASSERT_FALSE(context.coordinator.registerScanner(classic));

    TEST_ASSERT_EQUAL(
        static_cast<int>(ScanRequestResult::UnsupportedRadio),
        static_cast<int>(
            context.coordinator.requestScan(
                RadioType::BluetoothClassic
            )
        )
    );
}

void test_request_without_registered_scanner_is_rejected() {
    TestContext context;

    TEST_ASSERT_EQUAL(
        static_cast<int>(ScanRequestResult::ScannerUnavailable),
        static_cast<int>(
            context.coordinator.requestScan(RadioType::Wifi)
        )
    );
}

// Scan lifecycle

void test_request_scan_starts_scanner_and_inventory() {
    TestContext context;
    FakeRadioScanner wifi(RadioType::Wifi);

    TEST_ASSERT_TRUE(context.coordinator.registerScanner(wifi));

    TEST_ASSERT_EQUAL(
        static_cast<int>(ScanRequestResult::Started),
        static_cast<int>(
            context.coordinator.requestScan(RadioType::Wifi)
        )
    );

    TEST_ASSERT_EQUAL(
        static_cast<int>(ScannerState::Running),
        static_cast<int>(wifi.state())
    );

    TEST_ASSERT_TRUE(
        context.coordinator.scanInProgress(RadioType::Wifi)
    );

    TEST_ASSERT_EQUAL(1, wifi.startCount());
}

void test_process_applies_queued_observations_to_inventory() {
    TestContext context;
    FakeRadioScanner wifi(RadioType::Wifi);

    TEST_ASSERT_TRUE(context.coordinator.registerScanner(wifi));
    TEST_ASSERT_EQUAL(
        static_cast<int>(ScanRequestResult::Started),
        static_cast<int>(
            context.coordinator.requestScan(RadioType::Wifi)
        )
    );

    TEST_ASSERT_TRUE(
        context.observations.tryPush(
            makeObservation(RadioType::Wifi, "AA:BB:CC:DD:EE:01")
        )
    );

    TEST_ASSERT_EQUAL(1, context.coordinator.process());
    TEST_ASSERT_EQUAL(1, context.inventory.size());

    const Device* stored = context.inventory.find(
        RadioType::Wifi,
        "AA:BB:CC:DD:EE:01"
    );

    TEST_ASSERT_NOT_NULL(stored);
    TEST_ASSERT_EQUAL_STRING("Test Device", stored->name.c_str());
}

void test_completed_scan_commits_inventory_and_resets_scanner() {
    TestContext context;
    FakeRadioScanner wifi(RadioType::Wifi);

    TEST_ASSERT_TRUE(context.coordinator.registerScanner(wifi));
    TEST_ASSERT_EQUAL(
        static_cast<int>(ScanRequestResult::Started),
        static_cast<int>(
            context.coordinator.requestScan(RadioType::Wifi)
        )
    );

    TEST_ASSERT_TRUE(
        context.observations.tryPush(
            makeObservation(RadioType::Wifi, "AA:BB:CC:DD:EE:02")
        )
    );

    wifi.complete();

    // process() drains observations before committing the scan.
    TEST_ASSERT_EQUAL(1, context.coordinator.process());

    TEST_ASSERT_FALSE(
        context.coordinator.scanInProgress(RadioType::Wifi)
    );

    const Device* stored = context.inventory.find(
        RadioType::Wifi,
        "AA:BB:CC:DD:EE:02"
    );

    TEST_ASSERT_NOT_NULL(stored);
    TEST_ASSERT_TRUE(stored->currentlyVisible);

    TEST_ASSERT_EQUAL(
        static_cast<int>(ScannerState::Idle),
        static_cast<int>(wifi.state())
    );
}

void test_failed_scan_cancels_inventory() {
    TestContext context;
    FakeRadioScanner wifi(RadioType::Wifi);

    TEST_ASSERT_TRUE(context.coordinator.registerScanner(wifi));
    TEST_ASSERT_EQUAL(
        static_cast<int>(ScanRequestResult::Started),
        static_cast<int>(
            context.coordinator.requestScan(RadioType::Wifi)
        )
    );

    wifi.fail();
    context.coordinator.process();

    TEST_ASSERT_FALSE(
        context.coordinator.scanInProgress(RadioType::Wifi)
    );

    TEST_ASSERT_EQUAL(
        static_cast<int>(ScannerState::Idle),
        static_cast<int>(wifi.state())
    );
}

void test_failed_hardware_start_rolls_back_inventory() {
    TestContext context;
    FakeRadioScanner wifi(RadioType::Wifi);

    wifi.setFailStart(true);

    TEST_ASSERT_TRUE(context.coordinator.registerScanner(wifi));

    TEST_ASSERT_EQUAL(
        static_cast<int>(ScanRequestResult::StartFailed),
        static_cast<int>(
            context.coordinator.requestScan(RadioType::Wifi)
        )
    );

    TEST_ASSERT_FALSE(
        context.coordinator.scanInProgress(RadioType::Wifi)
    );
}

void test_cancel_scan_stops_scanner_and_cancels_inventory() {
    TestContext context;
    FakeRadioScanner wifi(RadioType::Wifi);

    TEST_ASSERT_TRUE(context.coordinator.registerScanner(wifi));
    TEST_ASSERT_EQUAL(
        static_cast<int>(ScanRequestResult::Started),
        static_cast<int>(
            context.coordinator.requestScan(RadioType::Wifi)
        )
    );

    TEST_ASSERT_TRUE(
        context.coordinator.cancelScan(RadioType::Wifi)
    );

    TEST_ASSERT_EQUAL(1, wifi.stopCount());
    TEST_ASSERT_FALSE(
        context.coordinator.scanInProgress(RadioType::Wifi)
    );
}

void test_cancelled_scan_rejects_late_observations() {
    TestContext context;
    FakeRadioScanner wifi(RadioType::Wifi);

    TEST_ASSERT_TRUE(context.coordinator.registerScanner(wifi));
    TEST_ASSERT_EQUAL(
        static_cast<int>(ScanRequestResult::Started),
        static_cast<int>(
            context.coordinator.requestScan(RadioType::Wifi)
        )
    );

    TEST_ASSERT_TRUE(
        context.coordinator.cancelScan(RadioType::Wifi)
    );

    TEST_ASSERT_TRUE(
        context.observations.tryPush(
            makeObservation(RadioType::Wifi, "AA:BB:CC:DD:EE:03")
        )
    );

    TEST_ASSERT_EQUAL(0, context.coordinator.process());
    TEST_ASSERT_EQUAL(0, context.inventory.size());
}

// Radio concurrency

void test_shared_bluetooth_controller_rejects_conflicting_scan() {
    TestContext context;
    FakeRadioScanner ble(RadioType::Ble);
    FakeRadioScanner classic(RadioType::BluetoothClassic);

    TEST_ASSERT_TRUE(context.coordinator.registerScanner(ble));
    TEST_ASSERT_TRUE(context.coordinator.registerScanner(classic));

    TEST_ASSERT_EQUAL(
        static_cast<int>(ScanRequestResult::Started),
        static_cast<int>(
            context.coordinator.requestScan(RadioType::Ble)
        )
    );

    TEST_ASSERT_EQUAL(
        static_cast<int>(ScanRequestResult::RadioConflict),
        static_cast<int>(
            context.coordinator.requestScan(
                RadioType::BluetoothClassic
            )
        )
    );
}

void test_wifi_can_scan_while_bluetooth_is_active() {
    TestContext context;
    FakeRadioScanner wifi(RadioType::Wifi);
    FakeRadioScanner ble(RadioType::Ble);

    TEST_ASSERT_TRUE(context.coordinator.registerScanner(wifi));
    TEST_ASSERT_TRUE(context.coordinator.registerScanner(ble));

    TEST_ASSERT_EQUAL(
        static_cast<int>(ScanRequestResult::Started),
        static_cast<int>(
            context.coordinator.requestScan(RadioType::Ble)
        )
    );

    TEST_ASSERT_EQUAL(
        static_cast<int>(ScanRequestResult::Started),
        static_cast<int>(
            context.coordinator.requestScan(RadioType::Wifi)
        )
    );
}

void test_ble_and_classic_can_scan_when_controller_is_not_shared() {
    TestContext context({true, true, true, false});
    FakeRadioScanner ble(RadioType::Ble);
    FakeRadioScanner classic(RadioType::BluetoothClassic);

    TEST_ASSERT_TRUE(context.coordinator.registerScanner(ble));
    TEST_ASSERT_TRUE(context.coordinator.registerScanner(classic));

    TEST_ASSERT_EQUAL(
        static_cast<int>(ScanRequestResult::Started),
        static_cast<int>(
            context.coordinator.requestScan(RadioType::Ble)
        )
    );

    TEST_ASSERT_EQUAL(
        static_cast<int>(ScanRequestResult::Started),
        static_cast<int>(
            context.coordinator.requestScan(
                RadioType::BluetoothClassic
            )
        )
    );
}

void test_completed_scanner_can_start_another_scan() {
    TestContext context;
    FakeRadioScanner wifi(RadioType::Wifi);

    TEST_ASSERT_TRUE(context.coordinator.registerScanner(wifi));

    TEST_ASSERT_EQUAL(
        static_cast<int>(ScanRequestResult::Started),
        static_cast<int>(
            context.coordinator.requestScan(RadioType::Wifi)
        )
    );

    wifi.complete();
    context.coordinator.process();

    TEST_ASSERT_EQUAL(
        static_cast<int>(ScanRequestResult::Started),
        static_cast<int>(
            context.coordinator.requestScan(RadioType::Wifi)
        )
    );

    TEST_ASSERT_EQUAL(2, wifi.startCount());
}

} // namespace

void setUp() {
}

void tearDown() {
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_registers_supported_scanner);
    RUN_TEST(test_duplicate_scanner_registration_is_rejected);
    RUN_TEST(test_unsupported_radio_is_rejected);
    RUN_TEST(test_request_without_registered_scanner_is_rejected);

    RUN_TEST(test_request_scan_starts_scanner_and_inventory);
    RUN_TEST(test_process_applies_queued_observations_to_inventory);
    RUN_TEST(test_completed_scan_commits_inventory_and_resets_scanner);
    RUN_TEST(test_failed_scan_cancels_inventory);
    RUN_TEST(test_failed_hardware_start_rolls_back_inventory);
    RUN_TEST(test_cancel_scan_stops_scanner_and_cancels_inventory);
    RUN_TEST(test_cancelled_scan_rejects_late_observations);

    RUN_TEST(test_shared_bluetooth_controller_rejects_conflicting_scan);
    RUN_TEST(test_wifi_can_scan_while_bluetooth_is_active);
    RUN_TEST(test_ble_and_classic_can_scan_when_controller_is_not_shared);
    RUN_TEST(test_completed_scanner_can_start_another_scan);

    return UNITY_END();
}
