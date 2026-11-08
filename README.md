# WirelessRecon — Modular Refactor (Phase 1)

This repository is the first migration step from the Phase 7.1 Wireless Recon Console sketch.
The ThingPulse WROVER baseline is preserved in `src/legacy/WirelessRecon_Phase7_1_Baseline.cpp`.
Its Wi-Fi, passive BLE, Bluetooth Classic, privacy, inventory, RSSI tracking, RF dashboard,
touch handling, and existing LovyanGFX UI have not been rewritten in this phase.

## Selected direction

- Build: PlatformIO + Arduino framework
- UI target: LVGL 9.2.x for the later UI migration
- Boards: ThingPulse WROVER, ESP32-32E N4 2.8-inch display board, ESP32-S3, ESP32-C3
- Initial compatibility target: ThingPulse WROVER / ESP32 core line selected by PlatformIO platform pin

## Added board profile: ESP32-32E N4 2.8-inch board

The uploaded board photo identifies the module marking as **ESP32-32E N4** and the PCB as a
**2.8-inch, 240x320 LCD with resistive touch**. The PlatformIO environment is
`esp32_32e_28lcd` and uses the generic `esp32dev` target for the original ESP32 family.
The profile enables Wi-Fi, BLE, and Bluetooth Classic; it does not assume PSRAM. The module's
`N4` marking is treated as 4 MB flash.

The photo does not identify the LCD controller, resistive-touch controller, or their complete
GPIO wiring. Those are intentionally left unconfigured rather than guessed. This environment
currently builds the board-identification scaffold and portable core, not the full application UI.

## Phase 1 delivered

- Introduced a radio-neutral `Device` model (`include/core/Device.h`).
- Introduced fixed-capacity `DeviceInventory` with stable identity `(radio, address)`, upsert,
  per-radio scan lifecycle, visibility tracking, lookup, and copy-out APIs.
- Added native unit test source for inventory lifecycle behavior.
- Added PlatformIO environments and board capability profiles.
- Kept the original application source intact as the reference implementation.

## Important migration boundary

Phase 1 establishes and tests the portable model, but the legacy screen/scanner code still uses its
original arrays. It has **not yet been switched over** to `DeviceInventory`. That compatibility
bridge is the next step and must be done carefully so existing sorting, historical Wi-Fi metadata,
privacy aliases, and BLE/Classic tracking behavior remain unchanged.

The S3 and C3 environments currently compile a board-identification scaffold plus the portable core;
they are not full wireless-recon ports yet. The existing legacy implementation uses Bluetooth Classic
APIs and ThingPulse-specific display/touch wiring, so it cannot simply be enabled unchanged on S3/C3.
The board capability profile correctly marks Classic Bluetooth unavailable on those targets.

LVGL 9 is declared as a dependency for the upcoming UI phase. The existing baseline continues to use
LovyanGFX in this phase; no LVGL 8-to-9 UI conversion is claimed here.

## Build

Install PlatformIO Core / VS Code PlatformIO extension, then from this directory:

```bash
pio run -e thingpulse_wrover
pio run -e esp32_32e_28lcd
pio run -e esp32_s3
pio run -e esp32_c3
```

The ThingPulse environment uses `huge_app.csv` and PSRAM. Confirm the exact board variant and flash
settings against the physical Color Kit before uploading. The S3/C3 builds are scaffold builds only.

## Native inventory test

The test is a plain C++ test and can be run without an ESP32 toolchain:

```bash
g++ -std=c++17 -I include src/core/DeviceInventory.cpp test/native/test_inventory.cpp -o /tmp/wr_inventory_test
/tmp/wr_inventory_test
```

## Next phase

Phase 1.5: add a compatibility adapter that feeds legacy Wi-Fi/BLE/Classic observations into the
new inventory, compare counts and lifecycle behavior, then migrate UI reads only after parity tests.
