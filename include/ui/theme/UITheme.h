//
// Created by dedlee on 11/15/26.
//

/**
 * @file UITheme.h
 * @brief Shared visual design tokens and styling helpers for WirelessRecon.
 *
 * @details
 * UITheme defines the visual language used by the LVGL application.
 *
 * Responsibilities:
 * - Provide a consistent dark reconnaissance interface.
 * - Centralize colors used by dashboard cards, labels, and controls.
 * - Provide reusable LVGL styling helpers.
 *
 * Architecture:
 * - This header belongs to the presentation layer.
 * - It must not depend on hardware drivers or radio implementations.
 * - It does not own LVGL objects; callers retain widget ownership.
 *
 * Memory:
 * - All theme values are compile-time constants.
 * - Styling helpers do not allocate application-owned memory.
 *
 * Board portability:
 * - Colors and styling are independent of display resolution.
 * - Individual screens are responsible for responsive geometry.
 */

#pragma once

#include <cstdint>
#include <lvgl.h>

namespace wirelessrecon::ui::theme {

/**
 * @brief Shared RGB color palette.
 *
 * Colors use 24-bit RGB values in 0xRRGGBB format.
 */
struct Colors {
    static constexpr uint32_t background = 0x08141B;
    static constexpr uint32_t surface = 0x10232D;
    static constexpr uint32_t surfaceRaised = 0x17313D;
    static constexpr uint32_t border = 0x254653;

    static constexpr uint32_t primary = 0x39E6C0;
    static constexpr uint32_t secondary = 0x56B4D3;

    static constexpr uint32_t textPrimary = 0xE5F0F4;
    static constexpr uint32_t textSecondary = 0x91A8B5;
    static constexpr uint32_t textMuted = 0x627985;

    static constexpr uint32_t success = 0x45D483;
    static constexpr uint32_t warning = 0xF0C674;
    static constexpr uint32_t error = 0xEF6461;
};

/**
 * @brief Shared geometry and spacing recommendations.
 *
 * These values are starting points. ResponsiveLayout determines which
 * profile is active; screens may adjust spacing using that profile.
 */
struct Spacing {
    static constexpr uint8_t compact = 4;
    static constexpr uint8_t standard = 8;
    static constexpr uint8_t section = 12;
    static constexpr uint8_t screenMargin = 8;
    static constexpr uint8_t cardRadius = 8;
    static constexpr uint8_t borderWidth = 1;
};

/**
 * @brief Applies the application background to an LVGL object.
 *
 * @param object LVGL object whose background should be styled.
 *
 * @note The caller retains ownership of the object.
 */
inline void applyBackground(lv_obj_t* object) {
    if (object == nullptr) {
        return;
    }

    lv_obj_set_style_bg_color(
        object,
        lv_color_hex(Colors::background),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        object,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        object,
        0,
        LV_PART_MAIN
    );
}

/**
 * @brief Applies the standard raised-panel appearance.
 *
 * @param object LVGL object to style.
 *
 * @note This helper does not change the object's dimensions or layout.
 */
inline void applyPanel(lv_obj_t* object) {
    if (object == nullptr) {
        return;
    }

    lv_obj_set_style_bg_color(
        object,
        lv_color_hex(Colors::surface),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        object,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_color(
        object,
        lv_color_hex(Colors::border),
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        object,
        Spacing::borderWidth,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        object,
        Spacing::cardRadius,
        LV_PART_MAIN
    );
}

} // namespace wirelessrecon::ui::theme
