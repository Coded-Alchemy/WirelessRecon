
/**
 * @file LVGLManager.cpp
 * @brief LVGL 9 display/input adapter and application UI lifecycle.
 */

#include "ui/lvgl/LVGLManager.h"

#include <Arduino.h>
#include <cstdlib>

#include "hardware/display/IDisplay.h"
#include "hardware/touch/ITouch.h"
#include "ui/display/DisplayCapabilities.h"
#include "ui/layout/ResponsiveLayout.h"
#include "ui/theme/UITheme.h"

namespace wirelessrecon::ui::lvgl {

using hardware::display::Color;

LVGLManager::LVGLManager(
    hardware::display::DisplayManager& displayManager,
    hardware::touch::TouchManager& touchManager
)
    : displayManager_(displayManager),
      touchManager_(touchManager) {
}

void LVGLManager::setApplicationContext(
    core::WirelessCoordinator& coordinator,
    core::DeviceInventory& inventory
) {
    coordinator_ = &coordinator;
    inventory_ = &inventory;

    dashboardScreen_.bind(coordinator, inventory);
    wifiScreen_.bind(coordinator, inventory);

    dashboardScreen_.setNavigationCallback(
        &LVGLManager::navigationCallback,
        this
    );

    wifiScreen_.setNavigationCallback(
        &LVGLManager::navigationCallback,
        this
    );

    wifiScreen_.setBackCallback(
        &LVGLManager::wifiBackCallback,
        this
    );
}

bool LVGLManager::begin() {
    if (initialized_) return true;

    if (coordinator_ == nullptr || inventory_ == nullptr) {
        Serial.println("[LVGL] Application context has not been configured.");
        return false;
    }

    if (!displayManager_.isInitialized()) {
        Serial.println("[LVGL] Display manager is not initialized.");
        return false;
    }

    if (!touchManager_.isInitialized()) {
        Serial.println("[LVGL] Touch manager is not initialized.");
        return false;
    }

    auto& screen = displayManager_.display();
    const uint16_t width = screen.width();
    const uint16_t height = screen.height();

    if (width == 0 || height == 0) {
        Serial.println("[LVGL] Invalid display dimensions.");
        return false;
    }

    Serial.println();
    Serial.println("[LVGL] Initializing LVGL 9...");

    lv_init();
    lv_tick_set_cb(&LVGLManager::tickCallback);

    const uint16_t bufferLines =
        height < kDrawBufferLines ? height : kDrawBufferLines;

    const size_t pixelCount =
        static_cast<size_t>(width) * bufferLines;

    const size_t requestedBytes =
        pixelCount * sizeof(uint16_t);

    if (requestedBytes == 0 || requestedBytes > UINT32_MAX) {
        Serial.println("[LVGL] Draw-buffer size is invalid.");
        return false;
    }

    drawBufferBytes_ = static_cast<uint32_t>(requestedBytes);
    drawBuffer_ = static_cast<uint16_t*>(
        std::malloc(drawBufferBytes_)
    );

    if (drawBuffer_ == nullptr) {
        Serial.printf(
            "[LVGL] Unable to allocate %lu-byte draw buffer.\n",
            static_cast<unsigned long>(drawBufferBytes_)
        );
        drawBufferBytes_ = 0;
        return false;
    }

    display_ = lv_display_create(width, height);

    if (display_ == nullptr) {
        Serial.println("[LVGL] Display creation failed.");
        std::free(drawBuffer_);
        drawBuffer_ = nullptr;
        drawBufferBytes_ = 0;
        return false;
    }

    lv_display_set_user_data(display_, this);
    lv_display_set_color_format(display_, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display_, &LVGLManager::flushCallback);

    lv_display_set_buffers(
        display_,
        drawBuffer_,
        nullptr,
        drawBufferBytes_,
        LV_DISPLAY_RENDER_MODE_PARTIAL
    );

    inputDevice_ = lv_indev_create();

    if (inputDevice_ == nullptr) {
        Serial.println("[LVGL] Input device creation failed.");
        return false;
    }

    lv_indev_set_type(inputDevice_, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(inputDevice_, display_);
    lv_indev_set_user_data(inputDevice_, this);
    lv_indev_set_read_cb(
        inputDevice_,
        &LVGLManager::touchReadCallback
    );

    if (!createDashboardScreen()) {
        Serial.println("[LVGL] Dashboard creation failed.");
        return false;
    }

    initialized_ = true;
    lastScreenRefreshMs_ = millis();

    Serial.printf(
        "[LVGL] Initialized: %u x %u, RGB565, partial buffer (%u lines).\n",
        width,
        height,
        bufferLines
    );

    return true;
}

void LVGLManager::handle() {
    if (!initialized_) return;

    lv_timer_handler();

    const uint32_t now = millis();

    if (static_cast<uint32_t>(
            now - lastScreenRefreshMs_
        ) >= kScreenRefreshIntervalMs) {
        refreshActiveScreen();
        lastScreenRefreshMs_ = now;
    }

    delay(kHandlerDelayMs);
}

bool LVGLManager::isInitialized() const {
    return initialized_;
}

uint32_t LVGLManager::tickCallback() {
    return millis();
}

void LVGLManager::flushCallback(
    ::lv_display_t* display,
    const ::lv_area_t* area,
    uint8_t* pixelMap
) {
    auto* manager = static_cast<LVGLManager*>(
        lv_display_get_user_data(display)
    );

    if (manager != nullptr) {
        manager->flush(area, pixelMap);
    }

    lv_display_flush_ready(display);
}

void LVGLManager::touchReadCallback(
    ::lv_indev_t* inputDevice,
    ::lv_indev_data_t* data
) {
    auto* manager = static_cast<LVGLManager*>(
        lv_indev_get_user_data(inputDevice)
    );

    if (manager != nullptr) {
        manager->readTouch(data);
        return;
    }

    data->state = LV_INDEV_STATE_RELEASED;
    data->point.x = 0;
    data->point.y = 0;
}

void LVGLManager::flush(
    const ::lv_area_t* area,
    const uint8_t* pixelMap
) {
    if (area == nullptr || pixelMap == nullptr) return;

    const int32_t width = area->x2 - area->x1 + 1;
    const int32_t height = area->y2 - area->y1 + 1;

    if (width <= 0 || height <= 0 ||
        area->x1 < 0 || area->y1 < 0) {
        return;
    }

    auto& screen = displayManager_.display();

    if (area->x2 >= screen.width() ||
        area->y2 >= screen.height()) {
        return;
    }

    screen.pushPixels(
        area->x1,
        area->y1,
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        reinterpret_cast<const Color*>(pixelMap)
    );
}

void LVGLManager::readTouch(::lv_indev_data_t* data) {
    if (data == nullptr) return;

    hardware::touch::TouchPoint point;

    if (touchManager_.read(point) && point.touched) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = point.x;
        data->point.y = point.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

bool LVGLManager::prepareActiveScreen() {
    lv_obj_t* screen = lv_screen_active();

    if (screen == nullptr) {
        Serial.println("[LVGL] No active screen is available.");
        return false;
    }

    lv_obj_clean(screen);
    theme::applyBackground(screen);

    wirelessrecon::ui::display::DisplayCapabilities capabilities;
    capabilities.width = displayManager_.display().width();
    capabilities.height = displayManager_.display().height();
    capabilities.hasTouch = touchManager_.isInitialized();
    capabilities.supportsSwipe = capabilities.hasTouch;

    capabilities.orientation =
        capabilities.width >= capabilities.height
            ? wirelessrecon::ui::display::DisplayOrientation::Landscape
            : wirelessrecon::ui::display::DisplayOrientation::Portrait;

    const auto profile =
        wirelessrecon::ui::layout::ResponsiveLayout::select(
            capabilities
        );

    lv_obj_set_style_pad_all(
        screen, profile.outerMargin, LV_PART_MAIN
    );

    return true;
}

bool LVGLManager::createDashboardScreen() {
    if (!prepareActiveScreen()) return false;

    if (!dashboardScreen_.create(lv_screen_active())) {
        Serial.println("[LVGL] Dashboard widget creation failed.");
        return false;
    }

    Serial.println("[LVGL] Dashboard created.");
    return true;
}

bool LVGLManager::createWifiScreen() {
    if (!prepareActiveScreen()) return false;

    if (!wifiScreen_.create(lv_screen_active())) {
        Serial.println("[LVGL] Wi-Fi screen creation failed.");
        return false;
    }

    Serial.println("[LVGL] Wi-Fi screen created.");
    return true;
}

void LVGLManager::showDashboard() {
    if (!initialized_) return;
    if (!createDashboardScreen()) return;

    activeScreen_ = ActiveScreen::Dashboard;
    lastScreenRefreshMs_ = millis();
}

void LVGLManager::showWifi() {
    if (!initialized_) return;
    if (!createWifiScreen()) return;

    activeScreen_ = ActiveScreen::Wifi;
    lastScreenRefreshMs_ = millis();
}

void LVGLManager::navigateTo(
    theme::NavigationDestination destination
) {
    if (!initialized_) return;

    switch (destination) {
        case theme::NavigationDestination::Dashboard:
            showDashboard();
            break;

        case theme::NavigationDestination::Wifi:
            showWifi();
            break;

        case theme::NavigationDestination::Bluetooth:
        case theme::NavigationDestination::Settings:
            // These destinations are deliberately disabled in the UI
            // until their screens are implemented.
            break;
    }
}

void LVGLManager::refreshActiveScreen() {
    if (activeScreen_ == ActiveScreen::Wifi) {
        wifiScreen_.refresh();
    } else {
        dashboardScreen_.refresh();
    }
}

void LVGLManager::navigationCallback(
    void* context,
    theme::NavigationDestination destination
) {
    auto* manager = static_cast<LVGLManager*>(context);

    if (manager != nullptr) {
        manager->navigateTo(destination);
    }
}

void LVGLManager::wifiBackCallback(void* context) {
    auto* manager = static_cast<LVGLManager*>(context);

    if (manager != nullptr) {
        manager->showDashboard();
    }
}

} // namespace wirelessrecon::ui::lvgl
