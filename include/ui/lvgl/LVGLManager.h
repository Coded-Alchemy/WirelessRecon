/**
 * @file LVGLManager.h
 * @brief Public interface for the LVGL display, input, and screen manager.
 *
 * @details
 * LVGLManager is the presentation-layer adapter between LVGL and the
 * hardware-neutral DisplayManager/TouchManager interfaces.
 *
 * Responsibilities:
 * - Initialize LVGL and its partial RGB565 draw buffer.
 * - Register display flush and touch input callbacks.
 * - Own the DashboardScreen and WifiScreen controller objects.
 * - Create the active screen and route navigation requests.
 * - Service LVGL and refresh the active screen from handle().
 *
 * Non-responsibilities:
 * - It does not own or configure the physical display/touch drivers.
 * - It does not own WirelessCoordinator or DeviceInventory.
 * - It does not start scans or process radio observations.
 *
 * Threading invariant:
 * handle() is the sole regular LVGL service point. Screen creation,
 * navigation callbacks, refreshes, event handling, and widget mutations must
 * execute on the same application/UI task. Radio callbacks must not call
 * LVGLManager or screen methods.
 *
 * Lifetime invariant:
 * DisplayManager and TouchManager are references and must outlive this
 * manager. Application services passed to setApplicationContext() are
 * non-owning pointers and must also outlive it.
 */

#pragma once

#include <cstdint>

#include <lvgl.h>

#include "core/DeviceInventory.h"
#include "core/WirelessCoordinator.h"
#include "hardware/display/DisplayManager.h"
#include "hardware/touch/TouchManager.h"
#include "ui/screens/DashboardScreen.h"
#include "ui/screens/WifiScreen.h"
#include "ui/theme/UITheme.h"

namespace wirelessrecon::ui::lvgl {

/**
 * @class LVGLManager
 * @brief Owns LVGL integration and coordinates the active application screen.
 */
class LVGLManager {
public:
    /**
     * @brief Constructs the manager around hardware adapters.
     *
     * @param displayManager Hardware-neutral display facade; must outlive manager.
     * @param touchManager Hardware-neutral touch facade; must outlive manager.
     */
    LVGLManager(
        hardware::display::DisplayManager& displayManager,
        hardware::touch::TouchManager& touchManager
    );

    LVGLManager(const LVGLManager&) = delete;
    LVGLManager& operator=(const LVGLManager&) = delete;

    /**
     * @brief Supplies non-owning application services and wires navigation.
     *
     * This method does not begin either service and does not alter radio
     * scanner registration. The composition root retains ownership.
     */
    void setApplicationContext(
        core::WirelessCoordinator& coordinator,
        core::DeviceInventory& inventory
    );

    /**
     * @brief Initializes LVGL, display/input adapters, and the dashboard.
     * @return true if the UI is ready for handle() calls.
     *
     * The display and touch managers must already be initialized. A failure
     * after lv_init() or display creation is treated as a boot-time failure;
     * this class does not attempt to tear down and reinitialize global LVGL.
     */
    bool begin();

    /**
     * @brief Services LVGL and periodically refreshes the active screen.
     *
     * This method must be called repeatedly from the designated UI task.
     * Do not call it from scanner callbacks, interrupt handlers, or another
     * task concurrently with LVGL operations.
     */
    void handle();

    /** @brief Reports whether begin() completed successfully. */
    bool isInitialized() const;

private:
    /** @brief Number of display rows allocated to the partial draw buffer. */
    static constexpr uint16_t kDrawBufferLines = 40;

    /** @brief Cooperative delay after each LVGL service iteration. */
    static constexpr uint32_t kHandlerDelayMs = 5;

    /** @brief Active-screen data refresh cadence. */
    static constexpr uint32_t kScreenRefreshIntervalMs = 750;

    /** @brief Identifies the screens currently implemented by the manager. */
    enum class ActiveScreen : uint8_t {
        Dashboard,
        Wifi
    };

    // Hardware adapters are referenced, never owned.
    hardware::display::DisplayManager& displayManager_;
    hardware::touch::TouchManager& touchManager_;

    // Application services are non-owning and composition-root owned.
    core::WirelessCoordinator* coordinator_ = nullptr;
    core::DeviceInventory* inventory_ = nullptr;

    // LVGL-owned handles. The draw buffer is allocated by this manager.
    ::lv_display_t* display_ = nullptr;
    ::lv_indev_t* inputDevice_ = nullptr;
    uint16_t* drawBuffer_ = nullptr;
    uint32_t drawBufferBytes_ = 0;

    // Screen controller objects are manager-owned. Their LVGL widgets are
    // children of the currently active LVGL screen.
    screens::DashboardScreen dashboardScreen_;
    screens::WifiScreen wifiScreen_;

    ActiveScreen activeScreen_ = ActiveScreen::Dashboard;
    uint32_t lastScreenRefreshMs_ = 0;
    bool initialized_ = false;

    /** @brief LVGL tick source backed by Arduino millis(). */
    static uint32_t tickCallback();

    /**
     * @brief LVGL display flush trampoline.
     *
     * Always signals flush completion, including rejected/invalid flushes.
     */
    static void flushCallback(
        ::lv_display_t* display,
        const ::lv_area_t* area,
        uint8_t* pixelMap
    );

    /** @brief LVGL pointer-input trampoline backed by TouchManager. */
    static void touchReadCallback(
        ::lv_indev_t* inputDevice,
        ::lv_indev_data_t* data
    );

    /**
     * @brief Shared navigation callback used by DashboardScreen and WifiScreen.
     *
     * Dashboard and Wi-Fi destinations are handled. Bluetooth and Settings
     * are currently disabled in the screen controls and are ignored here.
     */
    static void screenNavigationCallback(
        void* context,
        theme::NavigationDestination destination
    );

    /** @brief Wi-Fi Back callback that returns to the dashboard. */
    static void wifiBackCallback(void* context);

    /** @brief Copies a validated LVGL RGB565 area to the display adapter. */
    void flush(
        const ::lv_area_t* area,
        const uint8_t* pixelMap
    );

    /** @brief Reads one touch sample and translates it to LVGL pointer data. */
    void readTouch(::lv_indev_data_t* data);

    /** @brief Clears/configures the active screen and creates dashboard widgets. */
    bool createDashboardScreen();

    /** @brief Clears/configures the active screen and creates Wi-Fi widgets. */
    bool createWifiScreen();

    /** @brief Recreates the dashboard and marks it as the active screen. */
    void showDashboard();

    /** @brief Recreates the Wi-Fi inventory screen and marks it active. */
    void showWifi();

    /** @brief Refreshes only the screen currently selected by activeScreen_. */
    void refreshActiveScreen();

    /**
     * @brief Applies display-derived layout setup to lv_screen_active().
     * @return false if LVGL has no active screen.
     */
    bool prepareActiveScreen();
};

} // namespace wirelessrecon::ui::lvgl