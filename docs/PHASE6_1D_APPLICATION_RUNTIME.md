# Phase 6.1D — Application Runtime and Composition Root

## Status

Implementation prepared. Build and hardware regression validation pending.

## Objective

Extract application construction, startup sequencing, and runtime servicing
from `src/targets/board_scaffold.cpp`.

The Arduino entry point should delegate to a single application runtime.
The runtime becomes the composition root for the board-selected firmware.

This phase does not redesign the radio, core memory, display, touch, or LVGL
subsystems.

## New files

- `include/application/ApplicationRuntime.h`
- `src/application/ApplicationRuntime.cpp`

## Updated files

- `src/targets/board_scaffold.cpp`
- `config/platformio.boards.ini`

## Responsibility boundaries

| Component | Responsibility |
|---|---|
| `ApplicationRuntime` | Constructs and coordinates application components; controls startup and runtime sequencing |
| `CoreMemoryServices` | Owns `ObservationQueue`, `DeviceInventory`, and `WirelessCoordinator` lifetimes |
| `RadioController` | Registers radio scanners, starts the normal Wi-Fi scan, and processes radio observations |
| `WifiScanner` | Performs Wi-Fi scanning and submits observations |
| `BleScanner` | Performs BLE scanning and submits observations |
| `DisplayManager` | Provides the board-independent display interface |
| `TouchManager` | Provides the board-independent touch interface |
| `LVGLManager` | Owns LVGL display/input integration, dashboard creation, and UI-task servicing |
| Diagnostic classes | Exercise and report hardware behavior without taking ownership of the subsystems |
| `board_scaffold.cpp` | Delegates Arduino `setup()` and `loop()` to `ApplicationRuntime` |

## Ownership

`ApplicationRuntime` owns:

- Wi-Fi scanner
- BLE scanner when compiled for a supported board
- Display configuration and display adapter
- Display manager
- Touch configuration and touch adapter
- Touch manager
- LVGL manager
- Radio controller in modular builds
- Optional diagnostic objects

`CoreMemoryServices` continues to own:

- Observation queue
- Device inventory
- Wireless coordinator

The inventory and coordinator pointers held by `ApplicationRuntime` are
non-owning aliases.

The radio controller, LVGL manager, and diagnostics continue to use
non-owning references to their dependencies.

## Startup sequence

```mermaid
flowchart TD
    A["Arduino setup()"] --> B["ApplicationRuntime.begin()"]
    B --> C["Initialize CoreMemoryServices"]
    C --> D{"Core services ready?"}
    D -- No --> E["Return from startup"]
    D -- Yes --> F["Initialize display"]
    F --> G["Initialize touch"]
    G --> H{"Display and touch ready?"}
    H -- No --> I["Leave application subsystems inactive"]
    H -- Yes --> J{"Diagnostic build?"}
    J -- No --> K["Bind application context to LVGL"]
    K --> L["Initialize LVGL dashboard"]
    L --> M["RadioController.begin()"]
    M --> N["Register Wi-Fi and supported BLE"]
    N --> O["Start Wi-Fi scan"]
    J -- Yes --> P["Run display diagnostics"]
    P --> Q["Initialize Wi-Fi diagnostics"]
    Q --> R["Arduino loop()"]