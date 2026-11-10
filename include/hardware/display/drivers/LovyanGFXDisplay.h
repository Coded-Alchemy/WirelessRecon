//
// Created by dedlee on 11/8/26.
//

#pragma once

#include "hardware/display/IDisplay.h"
#include "hardware/display/DisplayConfig.h"

#include <LovyanGFX.hpp>

namespace wirelessrecon::hardware::display {

    class LovyanGFXDisplay final : public IDisplay {
    public:
        explicit LovyanGFXDisplay(const DisplayConfig& config);

        bool begin() override;

        uint16_t width() const override;
        uint16_t height() const override;

        void setRotation(uint8_t rotation) override;
        void clear(Color color) override;

        void drawPixel(
            int16_t x, int16_t y, Color color
        ) override;

        void drawLine(
            int16_t x0, int16_t y0,
            int16_t x1, int16_t y1,
            Color color
        ) override;

        void drawRect(
            int16_t x, int16_t y,
            int16_t width, int16_t height,
            Color color
        ) override;

        void fillRect(
            int16_t x, int16_t y,
            int16_t width, int16_t height,
            Color color
        ) override;

        void pushPixels(
            int16_t x, int16_t y,
            uint16_t width, uint16_t height,
            const Color* pixels
        ) override;

        void drawText(
            int16_t x, int16_t y,
            const char* text,
            Color foreground,
            Color background
        ) override;

        void setBacklight(bool enabled) override;

    private:
        const DisplayConfig& config_;

        lgfx::LGFX_Device device_;
        lgfx::Panel_ILI9488 panel9488_;
        lgfx::Panel_ILI9341 panel9341_;
        lgfx::Bus_SPI bus_;

        bool initialized_ = false;

        void configureBus();
    };

} // namespace wirelessrecon::hardware::display
