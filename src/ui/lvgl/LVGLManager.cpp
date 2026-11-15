/**
 * @file LVGLManager.cpp
 * @brief LVGL 9 display/input adapter and application UI lifecycle.
 *
 * Rendering path:
 *
 *   LVGL widget tree
 *       -> LVGL partial renderer
 *       -> flushCallback()
 *       -> LVGLManager::flush()
 *       -> DisplayManager
 *       -> board-specific IDisplay implementation
 *
 * Input path:
 *
 *   TouchManager
 *       -> board-specific ITouch implementation
 *       -> LVGLManager::readTouch()
 *       -> touchReadCallback()
 *       -> LVGL pointer input processing
 *
 * The manager deliberately knows about DisplayManager and TouchManager,
 * rather than a particular panel or touch-controller driver. Board-specific
 * pin assignments and bus configuration remain below those abstractions.
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
    // These pointers are non-owning. The board composition root must keep
    // both services alive until after LVGLManager stops using the dashboard.
    coordinator_ = &coordinator;
    inventory_ = &inventory;

    dashboardScreen_.bind(coordinator, inventory);
}

bool LVGLManager::begin() {
    if (initialized_) {
        return true;
    }

    if (coordinator_ == nullptr || inventory_ == nullptr) {
        Serial.println(
            "[LVGL] Application context has not been configured."
        );
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

    // A partial buffer limits the static rendering footprint. It is large
    // enough to batch multiple display rows while remaining much smaller
    // than a full 240x320 or 320x480 RGB565 framebuffer.
    const uint16_t bufferLines =
        height < kDrawBufferLines ? height : kDrawBufferLines;

    const size_t pixelCount =
        static_cast<size_t>(width) * bufferLines;

    const size_t requestedBytes =
        pixelCount * sizeof(uint16_t);

    if (requestedBytes == 0 ||
        requestedBytes > UINT32_MAX) {
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
    lv_display_set_color_format(
        display_,
        LV_COLOR_FORMAT_RGB565
    );
    lv_display_set_flush_cb(
        display_,
        &LVGLManager::flushCallback
    );
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

        /*
         * LVGL has already accepted the display and draw buffer. We avoid
         * freeing the draw buffer here because the display may still refer
         * to it. The failure is terminal for this boot; the caller should
         * leave LVGL inactive rather than attempt a second initialization.
         */
        return false;
    }

    lv_indev_set_type(
        inputDevice_,
        LV_INDEV_TYPE_POINTER
    );
    lv_indev_set_display(
        inputDevice_,
        display_
    );
    lv_indev_set_user_data(
        inputDevice_,
        this
    );
    lv_indev_set_read_cb(
        inputDevice_,
        &LVGLManager::touchReadCallback
    );

    if (!createDashboardScreen()) {
        Serial.println("[LVGL] Dashboard creation failed.");
        return false;
    }

    initialized_ = true;
    lastDashboardRefreshMs_ = millis();

    Serial.printf(
        "[LVGL] Initialized: %u x %u, RGB565, partial buffer (%u lines).\n",
        width,
        height,
        bufferLines
    );

    return true;
}

void LVGLManager::handle() {
    if (!initialized_) {
        return;
    }

    /*
     * LVGL input, event callbacks, timers, layout, and drawing are all
     * serviced from this task. Keep this as the sole regular LVGL service
     * point in the application loop.
     */
    lv_timer_handler();

    const uint32_t now = millis();

    // Unsigned subtraction is safe across millis() rollover.
    if (static_cast<uint32_t>(
            now - lastDashboardRefreshMs_
        ) >= kDashboardRefreshIntervalMs) {
        dashboardScreen_.refresh();
        lastDashboardRefreshMs_ = now;
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

    /*
     * LVGL must be notified even if the adapter rejects an invalid area.
     * Otherwise the renderer can remain blocked waiting for this buffer.
     */
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
    if (area == nullptr || pixelMap == nullptr) {
        return;
    }

    const int32_t width =
        area->x2 - area->x1 + 1;

    const int32_t height =
        area->y2 - area->y1 + 1;

    if (width <= 0 || height <= 0 ||
        area->x1 < 0 || area->y1 < 0) {
        return;
    }

    auto& screen = displayManager_.display();

    if (area->x2 >= screen.width() ||
        area->y2 >= screen.height()) {
        return;
    }

    /*
     * LVGL is configured for RGB565. The display abstraction accepts the
     * same 16-bit pixel representation. No color conversion or temporary
     * framebuffer is required here.
     */
    screen.pushPixels(
        area->x1,
        area->y1,
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        reinterpret_cast<const Color*>(pixelMap)
    );
}

void LVGLManager::readTouch(::lv_indev_data_t* data) {
    if (data == nullptr) {
        return;
    }

    hardware::touch::TouchPoint point;

    if (touchManager_.read(point) && point.touched) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = point.x;
        data->point.y = point.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

bool LVGLManager::createDashboardScreen() {
    lv_obj_t* screen = lv_screen_active();

    if (screen == nullptr) {
        Serial.println("[LVGL] No active screen is available.");
        return false;
    }

    // Remove any default or previous test-screen children.
    lv_obj_clean(screen);

    wirelessrecon::ui::theme::applyBackground(screen);

    /*
     * Build a portable description of the actual display. This keeps
     * responsive selection independent from board-specific macros.
     */
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

    /*
     * The first dashboard is intentionally single-column. The selected
     * profile still supplies the safe outer margin, and later dashboard
     * components can consume the remaining profile properties.
     */
    lv_obj_set_style_pad_all(
        screen,
        profile.outerMargin,
        LV_PART_MAIN
    );

    if (!dashboardScreen_.create(screen)) {
        Serial.println("[LVGL] Dashboard widget creation failed.");
        return false;
    }

    Serial.printf(
        "[LVGL] Dashboard created. Layout columns: %u, margin: %u.\n",
        profile.columns,
        profile.outerMargin
    );

    return true;
}

} // namespace wirelessrecon::ui::lvgl