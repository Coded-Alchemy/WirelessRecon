//
// Created by dedlee on 11/15/26.
//

#include "core/CoreMemoryServices.h"

#include <Arduino.h>
#include <esp32-hal-psram.h>

#include <cstdlib>
#include <new>

namespace wirelessrecon {
namespace core {

CoreMemoryServices::CoreMemoryServices(
    const RadioCapabilities& capabilities
)
    : capabilities_(capabilities) {
}

CoreMemoryServices::~CoreMemoryServices() {
    shutdown();
}

bool CoreMemoryServices::begin() {
    if (initialized_) {
        return true;
    }

    if (deviceInventory_ != nullptr ||
        wirelessCoordinator_ != nullptr) {
        Serial.println(
            "[MEMORY] Core services are partially initialized."
        );
        shutdown();
    }

    Serial.println("[MEMORY] Checking external PSRAM...");

    if (!psramFound()) {
        Serial.println("[MEMORY] PSRAM was not detected.");
        return false;
    }

    Serial.printf(
        "[MEMORY] PSRAM available: %u bytes\n",
        static_cast<unsigned>(ESP.getPsramSize())
    );

    void* inventoryMemory = ps_malloc(sizeof(DeviceInventory));

    if (inventoryMemory == nullptr) {
        Serial.println(
            "[MEMORY] Failed to allocate DeviceInventory in PSRAM."
        );
        return false;
    }

    // Construct DeviceInventory inside the PSRAM allocation.
    deviceInventory_ = new (inventoryMemory) DeviceInventory();

    wirelessCoordinator_ =
        new (std::nothrow) WirelessCoordinator(
            *deviceInventory_,
            observationQueue_,
            capabilities_
        );

    if (wirelessCoordinator_ == nullptr) {
        Serial.println(
            "[MEMORY] Failed to allocate WirelessCoordinator."
        );

        deviceInventory_->~DeviceInventory();
        free(deviceInventory_);
        deviceInventory_ = nullptr;

        return false;
    }

    initialized_ = true;

    Serial.printf(
        "[MEMORY] DeviceInventory allocated in PSRAM: %u bytes\n",
        static_cast<unsigned>(sizeof(DeviceInventory))
    );

    Serial.println("[MEMORY] Core storage initialized.");

    return true;
}

void CoreMemoryServices::shutdown() {
    // The coordinator references the inventory and queue, so destroy
    // it before releasing the inventory.
    if (wirelessCoordinator_ != nullptr) {
        delete wirelessCoordinator_;
        wirelessCoordinator_ = nullptr;
    }

    if (deviceInventory_ != nullptr) {
        deviceInventory_->~DeviceInventory();
        free(deviceInventory_);
        deviceInventory_ = nullptr;
    }

    initialized_ = false;
}

bool CoreMemoryServices::isReady() const {
    return initialized_ &&
           deviceInventory_ != nullptr &&
           wirelessCoordinator_ != nullptr;
}

ObservationQueue& CoreMemoryServices::observationQueue() {
    return observationQueue_;
}

DeviceInventory* CoreMemoryServices::deviceInventory() {
    return deviceInventory_;
}

const DeviceInventory* CoreMemoryServices::deviceInventory() const {
    return deviceInventory_;
}

WirelessCoordinator* CoreMemoryServices::wirelessCoordinator() {
    return wirelessCoordinator_;
}

const WirelessCoordinator*
CoreMemoryServices::wirelessCoordinator() const {
    return wirelessCoordinator_;
}

} // namespace core
} // namespace wirelessrecon
