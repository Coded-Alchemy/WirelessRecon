# Architecture and migration contract

## Dependency direction

`UI -> Application services -> Core models/services <- Radio adapters`

The core must not include Arduino, ESP-IDF, LovyanGFX, LVGL, or board pin headers. Hardware and
radio adapters translate platform-native events into `wirelessrecon::core::Device` observations.

## Identity

A device is identified by `(RadioType, address)`. Addresses are not globally unique across radio
families, so the radio type is part of the key. Wi-Fi-only fields remain in `WifiMetadata`; common
fields cover identity, signal, sightings, and lifecycle.

## Scan lifecycle

1. `beginScan(radio)` marks existing devices of that radio unseen for the new scan.
2. Each observation is passed to `upsert(device)`.
3. `completeScan(radio)` sets `currentlyVisible` from `seenInCurrentScan` and increments generation.

The inventory does not start radios, schedule scans, sort presentation lists, or own UI state.
Those responsibilities belong to later services/adapters.

## Concurrency

`DeviceInventory` is deliberately not internally synchronized. The application layer must serialize
updates (for example, with a FreeRTOS mutex or a single-owner event queue). Do not call it directly
from multiple radio callback tasks without that protection.

## Memory

Capacity is fixed at 150 entries to match the current maximum of 50 records per radio. The model
uses `std::string`; before expanding capacity or adding long histories, measure heap fragmentation
on non-PSRAM boards. RSSI history remains a separate bounded service in a later phase.


## ESP32-32E N4 display-board profile

The board photo shows an ESP32-32E N4 module and PCB silkscreen identifying a 2.8-inch
240x320 resistive-touch LCD. The profile therefore declares the original ESP32 radio set
(Wi-Fi, BLE, and Bluetooth Classic), no PSRAM, and the display geometry/touch type. It does
not declare an LCD driver, touch-controller driver, or pin map: those require the board's
schematic, controller markings, or a controlled hardware identification test. Do not reuse
the ThingPulse ILI9488/FT6236 wiring for this board without verification.
