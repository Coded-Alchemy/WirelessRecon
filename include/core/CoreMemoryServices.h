//
// Created by dedlee on 11/15/26.
//


#pragma once

#include "core/DeviceInventory.h"
#include "core/ObservationQueue.h"
#include "core/WirelessCoordinator.h"

namespace wirelessrecon {
    namespace core {

        /**
         * Owns the core observation queue and manages the lifetime of
         * memory-dependent application services.
         *
         * ObservationQueue remains a member of this object and therefore
         * uses normal internal RAM allocation.
         *
         * DeviceInventory is allocated in PSRAM and placement-constructed.
         *
         * WirelessCoordinator is allocated separately in normal heap memory.
         *
         * This class owns all three services. Consumers receive references
         * or non-owning pointers and must not delete them.
         */
        class CoreMemoryServices {
        public:
            explicit CoreMemoryServices(
                const RadioCapabilities& capabilities
            );

            ~CoreMemoryServices();

            CoreMemoryServices(const CoreMemoryServices&) = delete;
            CoreMemoryServices& operator=(const CoreMemoryServices&) = delete;

            /**
             * Initializes the inventory and coordinator.
             *
             * Safe to call once. Returns true when all services are ready.
             */
            bool begin();

            /**
             * Releases the coordinator and inventory.
             * The observation queue remains alive until this object is destroyed.
             */
            void shutdown();

            bool isReady() const;

            ObservationQueue& observationQueue();

            DeviceInventory* deviceInventory();
            const DeviceInventory* deviceInventory() const;

            WirelessCoordinator* wirelessCoordinator();
            const WirelessCoordinator* wirelessCoordinator() const;

        private:
            ObservationQueue observationQueue_;

            DeviceInventory* deviceInventory_ = nullptr;
            WirelessCoordinator* wirelessCoordinator_ = nullptr;

            RadioCapabilities capabilities_;

            bool initialized_ = false;
        };

    } // namespace core
} // namespace wirelessrecon
