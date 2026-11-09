
#pragma once

#include <cstdint>

namespace wirelessrecon::hardware::display {

    using Color = uint16_t;

    // Common RGB565 colors.
    namespace Colors {
        constexpr Color Black   = 0x0000;
        constexpr Color White   = 0xFFFF;
        constexpr Color Red     = 0xF800;
        constexpr Color Green   = 0x07E0;
        constexpr Color Blue    = 0x001F;
        constexpr Color Yellow  = 0xFFE0;
        constexpr Color Cyan    = 0x07FF;
        constexpr Color Magenta = 0xF81F;
    }

    class IDisplay {
    public:
        virtual ~IDisplay() = default;

        // Hardware initialization.
        virtual bool begin() = 0;

        // Display dimensions.
        virtual uint16_t width() const = 0;
        virtual uint16_t height() const = 0;

        // Display orientation.
        virtual void setRotation(uint8_t rotation) = 0;

        // Screen operations.
        virtual void clear(Color color) = 0;

        // Drawing primitives.
        virtual void drawPixel(
            int16_t x,
            int16_t y,
            Color color
        ) = 0;

        virtual void drawLine(
            int16_t x0,
            int16_t y0,
            int16_t x1,
            int16_t y1,
            Color color
        ) = 0;

        virtual void drawRect(
            int16_t x,
            int16_t y,
            int16_t width,
            int16_t height,
            Color color
        ) = 0;

        virtual void fillRect(
            int16_t x,
            int16_t y,
            int16_t width,
            int16_t height,
            Color color
        ) = 0;

        // Text rendering.
        virtual void drawText(
            int16_t x,
            int16_t y,
            const char* text,
            Color foreground,
            Color background
        ) = 0;

        // Backlight control.
        virtual void setBacklight(bool enabled) = 0;
    };

} // namespace wirelessrecon::hardware::display
