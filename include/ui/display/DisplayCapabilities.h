/**
 * @file DisplayCapabilities.h
 * @brief Backend-independent description of display geometry and input.
 *
 * This file deliberately contains no Arduino, LVGL, LovyanGFX, or board
 * dependencies. A board profile or hardware adapter supplies these values.
 *
 * Display dimensions describe the active logical drawing area after the
 * hardware driver has applied its configured rotation.
 */

#pragma once

#include <cstdint>

namespace wirelessrecon {
namespace ui {
namespace display {

/** Physical/logical outline of the active display area. */
enum class ScreenShape : uint8_t {
    Rectangle = 0,
    Square,
    Circle,
    RoundedRectangle
};

/**
 * Broad device category.
 *
 * This is an explicit capability supplied by the target configuration.
 * It must not be inferred only from pixel dimensions: a small rectangular
 * diagnostic display and a round watch can have similar resolutions but
 * require very different interaction models.
 */
enum class FormFactor : uint8_t {
    Automatic = 0,
    Wearable,
    Handheld,
    Tablet,
    DesktopPanel,
    DedicatedDisplay
};

/** Orientation of the logical display coordinate system. */
enum class DisplayOrientation : uint8_t {
    Portrait = 0,
    Landscape,
    Square
};

/** Insets that keep content away from physical or obscured screen edges. */
struct SafeInsets {
    uint16_t top = 0;
    uint16_t right = 0;
    uint16_t bottom = 0;
    uint16_t left = 0;
};

/**
 * Capabilities reported by the active display and input hardware.
 *
 * The UI consumes this structure instead of checking board macros or
 * querying a specific display driver.
 */
struct DisplayCapabilities {
    uint16_t width = 0;
    uint16_t height = 0;

    ScreenShape shape = ScreenShape::Rectangle;
    FormFactor formFactor = FormFactor::Automatic;
    DisplayOrientation orientation = DisplayOrientation::Portrait;

    SafeInsets safeInsets{};

    bool hasTouch = false;
    bool supportsSwipe = false;
    bool hasPhysicalButtons = false;
    bool hasRotaryInput = false;

    /** True only when both logical dimensions are non-zero. */
    bool isValid() const {
        return width > 0 && height > 0;
    }

    /** Available width after subtracting configured safe insets. */
    uint16_t contentWidth() const {
        const uint32_t insetWidth =
            static_cast<uint32_t>(safeInsets.left) +
            static_cast<uint32_t>(safeInsets.right);

        return insetWidth >= width
            ? 0
            : static_cast<uint16_t>(width - insetWidth);
    }

    /** Available height after subtracting configured safe insets. */
    uint16_t contentHeight() const {
        const uint32_t insetHeight =
            static_cast<uint32_t>(safeInsets.top) +
            static_cast<uint32_t>(safeInsets.bottom);

        return insetHeight >= height
            ? 0
            : static_cast<uint16_t>(height - insetHeight);
    }

    /** Shorter usable dimension, useful for compact-layout decisions. */
    uint16_t shortestContentSide() const {
        const uint16_t w = contentWidth();
        const uint16_t h = contentHeight();
        return w < h ? w : h;
    }
};

} // namespace display
} // namespace ui
} // namespace wirelessrecon
