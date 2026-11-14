#pragma once

#include "BLEScan.h"
#include "esp_bt.h"
#include "esp_bt_main.h"

#include <string>

namespace fake_platform {
inline BLEScan bleScan;
inline bool bleScanAvailable = true;
inline int bleInitCalls = 0;
inline bool bleInitSucceeds = true;

inline void resetBle() {
    bleScan = BLEScan{};
    bleScanAvailable = true;
    bleInitCalls = 0;
    bleInitSucceeds = true;
    fakeControllerStatus() = ESP_BT_CONTROLLER_STATUS_IDLE;
    fakeBluedroidStatus() = ESP_BLUEDROID_STATUS_UNINITIALIZED;
}
}

class BLEDevice {
public:
    static void init(std::string /*deviceName*/) {
        ++fake_platform::bleInitCalls;
        if (fake_platform::bleInitSucceeds) {
            fakeControllerStatus() = ESP_BT_CONTROLLER_STATUS_ENABLED;
            fakeBluedroidStatus() = ESP_BLUEDROID_STATUS_ENABLED;
        }
    }

    static BLEScan* getScan() {
        return fake_platform::bleScanAvailable
            ? &fake_platform::bleScan
            : nullptr;
    }
};
