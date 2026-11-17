
/**
 * @file LVGLManager.h
 * @brief Public interface for the LVGL display, input, and screen manager.
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

class LVGLManager {
public:
    LVGLManager(
        hardware::display::DisplayManager& displayManager,
        hardware::touch::TouchManager& touchManager
    );

    LVGLManager(const LVGLManager&) = delete;
    LVGLManager& operator=(const LVGLManager&) = delete;

    void setApplicationContext(
        core::WirelessCoordinator& coordinator,
        core::DeviceInventory& inventory
    );

    bool begin();
    void handle();
    bool isInitialized() const;

private:
    static constexpr uint16_t kDrawBufferLines = 40;
    static constexpr uint32_t kHandlerDelayMs = 5;
    static constexpr uint32_t kScreenRefreshIntervalMs = 750;

    enum class ActiveScreen : uint8_t {
        Dashboard,
        Wifi
    };

    hardware::display::DisplayManager& displayManager_;
    hardware::touch::TouchManager& touchManager_;

    core::WirelessCoordinator* coordinator_ = nullptr;
    core::DeviceInventory* inventory_ = nullptr;

    ::lv_display_t* display_ = nullptr;
    ::lv_indev_t* inputDevice_ = nullptr;
    uint16_t* drawBuffer_ = nullptr;
    uint32_t drawBufferBytes_ = 0;

    screens::DashboardScreen dashboardScreen_;
    screens::WifiScreen wifiScreen_;

    ActiveScreen activeScreen_ = ActiveScreen::Dashboard;
    uint32_t lastScreenRefreshMs_ = 0;
    bool initialized_ = false;

    static uint32_t tickCallback();

    static void flushCallback(
        ::lv_display_t* display,
        const ::lv_area_t* area,
        uint8_t* pixelMap
    );

    static void touchReadCallback(
        ::lv_indev_t* inputDevice,
        ::lv_indev_data_t* data
    );

    static void navigationCallback(
        void* context,
        theme::NavigationDestination destination
    );

    static void wifiBackCallback(void* context);

    void flush(const ::lv_area_t* area, const uint8_t* pixelMap);
    void readTouch(::lv_indev_data_t* data);

    bool createDashboardScreen();
    bool createWifiScreen();

    void showDashboard();
    void showWifi();
    void navigateTo(theme::NavigationDestination destination);

    void refreshActiveScreen();
    bool prepareActiveScreen();
};

} // namespace wirelessrecon::ui::lvgl
