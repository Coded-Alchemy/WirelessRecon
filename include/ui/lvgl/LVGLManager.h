/**
 * @file LVGLManager.h
 * @brief Owns LVGL display/input integration and UI-task servicing.
 *
 * LVGLManager is the boundary between the portable LVGL UI and the
 * repository's hardware display/touch abstractions.
 *
 * Responsibilities:
 * - Initialize LVGL 9 and its tick source.
 * - Allocate and configure the partial RGB565 draw buffer.
 * - Connect LVGL flush and pointer-input callbacks to hardware managers.
 * - Create the initial dashboard after application services are bound.
 * - Run LVGL timers and periodic dashboard refreshes from the UI task.
 *
 * Ownership:
 * - DisplayManager and TouchManager are references; they outlive this class.
 * - LVGLManager owns the draw buffer allocated with malloc().
 * - LVGL owns its display, input-device, and widget objects after creation.
 * - DashboardScreen stores non-owning pointers to LVGL widgets.
 * - WirelessCoordinator and DeviceInventory are non-owning service pointers.
 *
 * Threading:
 * - begin() and handle() must be called from the application task.
 * - handle() is the only place in the normal application loop that services
 *   LVGL. Do not call LVGL APIs from radio callbacks or interrupt handlers.
 */

#pragma once

#include <cstdint>

#include <lvgl.h>

#include "core/DeviceInventory.h"
#include "core/WirelessCoordinator.h"

#include "hardware/display/DisplayManager.h"
#include "hardware/touch/TouchManager.h"

#include "ui/screens/DashboardScreen.h"

namespace wirelessrecon::ui::lvgl {

class LVGLManager final {
public:
    LVGLManager(
        hardware::display::DisplayManager& displayManager,
        hardware::touch::TouchManager& touchManager
    );

    LVGLManager(const LVGLManager&) = delete;
    LVGLManager& operator=(const LVGLManager&) = delete;

    /**
     * Supplies the application services used by dashboard screens.
     *
     * Call this after DeviceInventory and WirelessCoordinator have been
     * constructed and before begin(). The manager does not take ownership.
     */
    void setApplicationContext(
        core::WirelessCoordinator& coordinator,
        core::DeviceInventory& inventory
    );

    /** Initializes LVGL and creates the first dashboard. */
    bool begin();

    /**
     * Services LVGL timers, input, rendering, and periodic screen refresh.
     *
     * Call repeatedly from the main application loop. This method includes
     * a short delay to avoid continuously busy-looping the CPU.
     */
    void handle();

    /** Reports whether begin() completed successfully. */
    bool isInitialized() const;

private:
    /**
     * Number of display rows held in the partial draw buffer.
     *
     * A partial buffer reduces RAM use compared with a full-screen buffer.
     * At 320 pixels wide and RGB565, 40 rows require 25,600 bytes.
     */
    static constexpr uint16_t kDrawBufferLines = 40;

    /** Minimum delay between LVGL handler iterations. */
    static constexpr uint32_t kHandlerDelayMs = 5;

    /** Dashboard data is refreshed independently of LVGL's own timers. */
    static constexpr uint32_t kDashboardRefreshIntervalMs = 750;

    hardware::display::DisplayManager& displayManager_;
    hardware::touch::TouchManager& touchManager_;

    // Non-owning application services. The board composition root owns them.
    core::WirelessCoordinator* coordinator_ = nullptr;
    core::DeviceInventory* inventory_ = nullptr;

    // LVGL owns these registered objects after their creation.
    ::lv_display_t* display_ = nullptr;
    ::lv_indev_t* inputDevice_ = nullptr;

    // Allocated by begin(); released only if allocation/setup is rolled back.
    uint16_t* drawBuffer_ = nullptr;
    uint32_t drawBufferBytes_ = 0;

    // DashboardScreen owns no LVGL objects; the active LVGL screen does.
    screens::DashboardScreen dashboardScreen_;

    uint32_t lastDashboardRefreshMs_ = 0;
    bool initialized_ = false;

    /** LVGL tick callback, backed by Arduino millis(). */
    static uint32_t tickCallback();

    /** LVGL display flush callback. */
    static void flushCallback(
        ::lv_display_t* display,
        const ::lv_area_t* area,
        uint8_t* pixelMap
    );

    /** LVGL pointer-input callback. */
    static void touchReadCallback(
        ::lv_indev_t* inputDevice,
        ::lv_indev_data_t* data
    );

    /** Creates the dashboard on the active LVGL screen. */
    bool createDashboardScreen();

    /** Copies one LVGL partial-render area to the physical display. */
    void flush(
        const ::lv_area_t* area,
        const uint8_t* pixelMap
    );

    /** Reads and translates the current touch-controller state. */
    void readTouch(::lv_indev_data_t* data);
};

} // namespace wirelessrecon::ui::lvgl