#pragma once

#include <cstdint>

#include "hardware/display/DisplayManager.h"
#include "hardware/touch/TouchManager.h"

#include <lvgl.h>

namespace wirelessrecon::ui::lvgl {

    class LVGLManager {
    public:
        LVGLManager(
            hardware::display::DisplayManager& displayManager,
            hardware::touch::TouchManager& touchManager
        );

        LVGLManager(const LVGLManager&) = delete;
        LVGLManager& operator=(const LVGLManager&) = delete;

        bool begin();
        void handle();

        bool isInitialized() const;

    private:
        static constexpr uint16_t kDrawBufferLines = 40;

        hardware::display::DisplayManager& displayManager_;
        hardware::touch::TouchManager& touchManager_;

        ::lv_display_t* display_ = nullptr;
        ::lv_indev_t* inputDevice_ = nullptr;

        uint16_t* drawBuffer_ = nullptr;
        uint32_t drawBufferBytes_ = 0;

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
        static void buttonEventCallback(
            ::lv_event_t* event
        );

        void createTestScreen();
        void flush(
            const ::lv_area_t* area,
            const uint8_t* pixelMap
        );
        void readTouch(::lv_indev_data_t* data);
    };

} // namespace wirelessrecon::ui::lvgl
