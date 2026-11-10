# Phase 4 — LVGL 9 integration

This patch adds an LVGL 9 application layer to the modular ThingPulse target.

## Files changed

- `platformio.ini`
  - Adds `LV_CONF_INCLUDE_SIMPLE`.
  - Adds `src/ui/lvgl/LVGLManager.cpp` to `thingpulse_wrover_modular`.
- `include/hardware/display/IDisplay.h`
  - Adds an RGB565 rectangle-transfer method required by LVGL.
- `include/hardware/display/drivers/LovyanGFXDisplay.h`
- `src/hardware/display/drivers/LovyanGFXDisplay.cpp`
  - Implements the rectangle transfer using LovyanGFX `pushImage`.
- `include/ui/lvgl/LVGLManager.h`
- `src/ui/lvgl/LVGLManager.cpp`
  - Initializes LVGL 9, partial rendering, RGB565 display, FT6236 pointer input, and a simple interactive test screen.
- `src/targets/board_scaffold.cpp`
  - Starts the LVGL test interface after the existing hardware initialization.

## Build and upload

From the project root:

```bash
pio run -e thingpulse_wrover_modular
pio run -e thingpulse_wrover_modular -t upload --upload-port /dev/ttyACM0
pio device monitor -p /dev/ttyACM0 -b 115200
```

Change `/dev/ttyACM0` to the serial port reported by `pio device list`.

## Expected behavior

1. Display initialization succeeds.
2. FT6236 touch initialization succeeds.
3. LVGL initializes at the detected display resolution.
4. The screen displays WirelessRecon, LVGL 9 interface test, and a Test Touch button.
5. Pressing the button changes the status label to `Touch input is working`.

## Important notes

- The legacy `thingpulse_wrover` source filter and Phase 7.1 source are not changed.
- The display buffer is a 40-line partial RGB565 buffer. At 320 pixels wide this is 25,600 bytes.
- The touch adapter uses the existing `TouchManager`; it does not introduce a second I2C implementation.
- The LVGL tick source is Arduino `millis()`. The main loop calls `lv_timer_handler()` through `LVGLManager::handle()`.
- `include/lv_conf.h` is the existing LVGL 9.6.0 configuration supplied with the project.


## Phase 4 display-transfer correction

The LovyanGFX RGB565 transfer now explicitly disables byte swapping for LVGL's
native ESP32 RGB565 buffer and waits for any outstanding SPI DMA transfer to
finish before returning from `pushPixels()`. This ensures the LVGL partial draw
buffer is not reused while the display is still consuming it. Touch handling
and I2C configuration are unchanged.
