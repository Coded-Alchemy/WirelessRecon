
# WirelessRecon PlatformIO Configuration

## 1. Purpose

WirelessRecon uses PlatformIO to build firmware for multiple ESP32 boards and to run native host-side unit tests.

As the project grows, keeping every build environment in one large `platformio.ini` makes configuration harder to maintain. The project therefore uses PlatformIO's `extra_configs` feature to divide configuration by responsibility.

There is still only one PlatformIO project. The root `platformio.ini` imports the additional configuration files and PlatformIO combines them into one effective configuration.

## 2. Configuration directory structure

```text
WirelessRecon_Modular/
├── platformio.ini
├── config/
│   ├── platformio.boards.ini
│   ├── platformio.tests.ini
│   └── platformio.diagnostics.ini
├── docs/
│   └── PLATFORMIO_CONFIGURATION.md
├── include/
├── src/
└── test/
```

## 3. Main platformio.ini

**Purpose:** Main project entry point and shared configuration.

The root file is responsible for:

- Selecting the default build environment.
- Defining the source and include directories.
- Defining shared platform and framework settings.
- Defining common library dependencies.
- Defining common compiler flags.
- Importing all configuration files.

The main file explicitly expects these files:

| Imported file | Responsibility |
|---|---|
| `config/platformio.boards.ini` | Board firmware environments |
| `config/platformio.tests.ini` | Native unit tests |
| `config/platformio.diagnostics.ini` | Hardware diagnostic environments |

### What belongs here

- `[platformio]`
- `default_envs`
- `src_dir` and `include_dir`
- `extra_configs`
- Shared `[env]` settings
- Common libraries and compiler flags

### What does not belong here

- Board-specific GPIO definitions
- Individual board build environments
- Test suite filters
- Hardware diagnostic-only dependencies
- Board-specific source filters

## 4. Board configuration

**File:** `config/platformio.boards.ini`

**Purpose:** Define firmware environments for supported ESP32 hardware.

Current environments:

| Environment | Purpose |
|---|---|
| `thingpulse_wrover` | Legacy Phase 7.1 baseline |
| `esp32_s3` | ESP32-S3 firmware |
| `esp32_c3` | ESP32-C3 firmware |
| `esp32_32e_28lcd` | ELEGOO ESP32-32E 2.8-inch display |
| `thingpulse_wrover_modular` | Modular WROVER firmware |

### What belongs here

- Board identifiers
- CPU and flash configuration
- PSRAM configuration
- Partition selection
- Board-specific preprocessor definitions
- Firmware source filters
- Board-specific build settings

### What does not belong here

- Native unit test configuration
- Hardware diagnostic-only environments
- Shared libraries already defined in the root file

When adding another board, create a new `[env:environment_name]` section here unless it requires a separate diagnostic environment.

## 5. Native test configuration

**File:** `config/platformio.tests.ini`

**Purpose:** Define host-side tests that run on the development computer without ESP32 hardware.

Current environment:

`native_tests`

Current test suites:

- `test_device_inventory`
- `test_observation_queue`
- `test_wireless_coordinator`
- `test_device_model`
- `test_inventory_edge_cases`
- `test_wifi_scanner`
- `test_ble_scanner`
- `test_responsive_layout`

### What belongs here

- Native platform selection
- Unity test framework configuration
- Test filters
- Native compiler flags
- Mock include paths
- Production source files needed by tests

### What does not belong here

- ESP32 board definitions
- Physical display or touch dependencies
- Hardware upload settings
- Hardware diagnostic firmware environments

New portable unit test suites should be added to `test_filter`.

If a test requires an additional production source file, add it to `build_src_filter` as well.

## 6. Hardware diagnostic configuration

**File:** `config/platformio.diagnostics.ini`

**Purpose:** Define environments used to validate physical hardware and build diagnostic firmware.

Current environments:

| Environment | Purpose |
|---|---|
| `esp32_32e_28lcd_test` | ELEGOO LCD and touch hardware test |
| `thingpulse_wrover_modular_diag` | Modular WROVER with hardware diagnostics enabled |

### What belongs here

- Hardware diagnostic environments
- Diagnostic-only libraries
- Diagnostic-only compiler definitions
- Hardware-test source filters
- Diagnostic environment inheritance

### What does not belong here

- General-purpose board firmware environments
- Native unit test configuration
- Shared dependencies that belong in the root configuration

Diagnostic environments can extend board environments defined in `platformio.boards.ini`.

For example, `thingpulse_wrover_modular_diag` extends `thingpulse_wrover_modular` and adds the `WR_ENABLE_HARDWARE_DIAGNOSTICS` definition.

## 7. Future UI configuration

A separate UI configuration file is not currently necessary because the project does not yet have an independent UI-specific build environment.

When separate UI build environments become useful, introduce:

`config/platformio.ui.ini`

Potential responsibilities:

- LVGL-specific build environments
- Alternative UI rendering backend builds
- UI-focused compile configurations
- Display-specific UI validation environments

The file must be added to `extra_configs` in the root `platformio.ini` when it is created.

Do not duplicate the shared board settings or library dependencies in a UI configuration file.

## 8. Configuration inheritance

The root `[env]` section defines common settings inherited by firmware environments.

These include:

- ESP32 platform version
- Arduino framework
- Serial monitor speed
- Library dependency mode
- LovyanGFX
- LVGL
- Common include paths
- LVGL configuration include definition

Individual environments can override or extend these settings.

For example:

```ini
[env:thingpulse_wrover_modular_diag]
extends = env:thingpulse_wrover_modular

build_flags =
    ${env:thingpulse_wrover_modular.build_flags}
    -D WR_ENABLE_HARDWARE_DIAGNOSTICS
```

The imported files are merged into one effective PlatformIO configuration. Environment inheritance works across these imported files.

If the same configuration option is declared more than once for the same section, the later imported declaration can override the earlier one. Avoid accidental duplicate definitions.

## 9. Build and test commands

Run commands from the project root.

### Build firmware

```bash
pio run -e thingpulse_wrover
pio run -e thingpulse_wrover_modular
pio run -e esp32_32e_28lcd
pio run -e esp32_s3
pio run -e esp32_c3
```

### Build diagnostic firmware

```bash
pio run -e esp32_32e_28lcd_test
pio run -e thingpulse_wrover_modular_diag
```

### Run native tests

```bash
pio test -e native_tests
```

### Inspect the merged configuration

```bash
pio project config
```

This is useful for verifying that PlatformIO has loaded all imported files and resolved environment inheritance.

### Validate configuration syntax

```bash
pio project config --lint
```

## 10. Adding a new board

1. Add a new environment to `config/platformio.boards.ini`.
2. Define the board identifier and hardware-specific settings.
3. Add the appropriate board preprocessor definition.
4. Define the source filter required for that board.
5. Build the new environment.
6. Document the board profile and hardware requirements.

Do not modify the root shared settings unless the new board requires a genuinely project-wide change.

## 11. Adding a new native test

1. Create a test directory under `test/`.
2. Add a Unity test source file.
3. Add the test directory name to `test_filter` in `config/platformio.tests.ini`.
4. Add any required production source files to `build_src_filter`.
5. Run the native test environment.
6. Record the test coverage in the relevant project documentation.

## 12. Adding a hardware diagnostic

1. Add a diagnostic environment to `config/platformio.diagnostics.ini`.
2. Select or extend the appropriate board environment.
3. Add diagnostic-only dependencies.
4. Add diagnostic-only compiler definitions.
5. Limit the source filter to the required diagnostic sources.
6. Build and validate on physical hardware.

## 13. Configuration maintenance rules

- Keep one root `platformio.ini`.
- Import configuration files explicitly.
- Do not create independent PlatformIO projects inside `config/`.
- Keep each environment in the file matching its responsibility.
- Avoid duplicating common dependencies and compiler flags.
- Keep environment names stable because developers and scripts may depend on them.
- Validate the merged configuration after changing imports or inheritance.
- Keep this document synchronized with the actual configuration files.
- Preserve validated build environments when reorganizing configuration.

## 14. Design principle

The configuration layout should scale with WirelessRecon's hardware and software architecture.

Board support, UI rendering, diagnostics, and tests should be independently maintainable while sharing one consistent project configuration.

This organization is intended to support future ESP32 boards, display sizes, UI rendering backends, and portable native tests without turning the root configuration into a large collection of unrelated settings.
