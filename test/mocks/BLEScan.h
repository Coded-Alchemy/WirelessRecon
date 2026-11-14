#pragma once

#include "BLEAdvertisedDevice.h"

#include <cstdint>

class BLEScanResults {};

class BLEScan {
public:
    using CompletionCallback = void (*)(BLEScanResults);

    bool startResult = true;
    bool stopCalled = false;
    bool activeScan = false;
    uint32_t durationSeconds = 0;
    uint16_t interval = 0;
    uint16_t window = 0;
    int clearResultsCalls = 0;
    bool wantDuplicates = true;
    bool shouldParse = false;
    BLEAdvertisedDeviceCallbacks* advertisedCallbacks = nullptr;
    CompletionCallback completionCallback = nullptr;

    void clearResults() { ++clearResultsCalls; }

    void setAdvertisedDeviceCallbacks(BLEAdvertisedDeviceCallbacks* callbacks,
                                      bool duplicates = false,
                                      bool parse = true) {
        advertisedCallbacks = callbacks;
        wantDuplicates = duplicates;
        shouldParse = parse;
    }

    void setActiveScan(bool enabled) { activeScan = enabled; }
    void setInterval(uint16_t value) { interval = value; }
    void setWindow(uint16_t value) { window = value; }

    bool start(uint32_t duration, CompletionCallback callback,
               bool /*isContinue*/ = false) {
        durationSeconds = duration;
        completionCallback = callback;
        stopCalled = false;
        return startResult;
    }

    void stop() { stopCalled = true; }

    void emit(const BLEAdvertisedDevice& device) {
        if (advertisedCallbacks != nullptr) {
            advertisedCallbacks->onResult(device);
        }
    }

    void complete() {
        if (completionCallback != nullptr) {
            CompletionCallback callback = completionCallback;
            completionCallback = nullptr;
            callback(BLEScanResults{});
        }
    }
};
