#include "ui/lvgl/LVGLManager.h"

#include <Arduino.h>
#include <cstdlib>

#include "hardware/display/IDisplay.h"
#include "hardware/touch/ITouch.h"

namespace wirelessrecon::ui::lvgl {

    using hardware::display::Color;

    namespace {
        constexpr uint32_t kHandlerDelayMs = 5;
    }

    LVGLManager::LVGLManager(
        hardware::display::DisplayManager& displayManager,
        hardware::touch::TouchManager& touchManager
    )
        : displayManager_(displayManager),
          touchManager_(touchManager) {
    }

    bool LVGLManager::begin() {
        if (initialized_) {
            return true;
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

        drawBufferBytes_ =
            static_cast<uint32_t>(pixelCount * sizeof(uint16_t));

        drawBuffer_ = static_cast<uint16_t*>(malloc(drawBufferBytes_));

        if (drawBuffer_ == nullptr) {
            Serial.printf(
                "[LVGL] Unable to allocate %lu-byte draw buffer.\n",
                static_cast<unsigned long>(drawBufferBytes_)
            );
            return false;
        }

        display_ = lv_display_create(width, height);

        if (display_ == nullptr) {
            Serial.println("[LVGL] Display creation failed.");
            free(drawBuffer_);
            drawBuffer_ = nullptr;
            return false;
        }

        lv_display_set_user_data(display_, this);
        lv_display_set_color_format(display_, LV_COLOR_FORMAT_RGB565);
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
            return false;
        }

        lv_indev_set_type(inputDevice_, LV_INDEV_TYPE_POINTER);
        lv_indev_set_display(inputDevice_, display_);
        lv_indev_set_user_data(inputDevice_, this);
        lv_indev_set_read_cb(
            inputDevice_,
            &LVGLManager::touchReadCallback
        );

        createTestScreen();

        initialized_ = true;

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

        lv_timer_handler();
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
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
            data->point.x = 0;
            data->point.y = 0;
        }
    }

    void LVGLManager::buttonEventCallback(::lv_event_t* event) {
        auto* label = static_cast<lv_obj_t*>(
            lv_event_get_user_data(event)
        );

        if (label != nullptr) {
            lv_label_set_text(
                label,
                "Touch input is working"
            );
        }
    }

    void LVGLManager::flush(
        const ::lv_area_t* area,
        const uint8_t* pixelMap
    ) {
        if (area == nullptr || pixelMap == nullptr) {
            return;
        }

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

    void LVGLManager::createTestScreen() {
        lv_obj_t* screen = lv_screen_active();

        lv_obj_set_style_bg_color(
            screen,
            lv_color_hex(0x101820),
            LV_PART_MAIN
        );
        lv_obj_set_style_bg_opa(
            screen,
            LV_OPA_COVER,
            LV_PART_MAIN
        );

        lv_obj_t* title = lv_label_create(screen);
        lv_label_set_text(title, "WirelessRecon");
        lv_obj_set_style_text_color(
            title,
            lv_color_hex(0xFFFFFF),
            LV_PART_MAIN
        );
        lv_obj_set_style_text_font(
            title,
            &lv_font_montserrat_14,
            LV_PART_MAIN
        );
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 36);

        lv_obj_t* subtitle = lv_label_create(screen);
        lv_label_set_text(subtitle, "LVGL 9 interface test");
        lv_obj_set_style_text_color(
            subtitle,
            lv_color_hex(0xB8C7D9),
            LV_PART_MAIN
        );
        lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 78);

        lv_obj_t* status = lv_label_create(screen);
        lv_label_set_text(status, "Display adapter: READY");
        lv_obj_set_style_text_color(
            status,
            lv_color_hex(0x4ADE80),
            LV_PART_MAIN
        );
        lv_obj_align(status, LV_ALIGN_CENTER, 0, -32);

        lv_obj_t* button = lv_btn_create(screen);
        lv_obj_set_size(button, 190, 54);
        lv_obj_align(button, LV_ALIGN_CENTER, 0, 42);
        lv_obj_add_event_cb(
            button,
            &LVGLManager::buttonEventCallback,
            LV_EVENT_CLICKED,
            status
        );

        lv_obj_t* buttonLabel = lv_label_create(button);
        lv_label_set_text(buttonLabel, "Test Touch");
        lv_obj_center(buttonLabel);

        lv_obj_t* footer = lv_label_create(screen);
        lv_label_set_text(footer, "Phase 4 - UI foundation");
        lv_obj_set_style_text_color(
            footer,
            lv_color_hex(0x7F8C9A),
            LV_PART_MAIN
        );
        lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -18);
    }

} // namespace wirelessrecon::ui::lvgl
