/**
 * @file UITheme.h
 * @brief Shared WirelessRecon LVGL theme tokens and button styling helpers.
 *
 * @details
 * Button roles are intentionally distinct:
 * - Back and global navigation use the neutral navigation appearance.
 * - Scan actions use the cyan-accented primary-action appearance.
 * - Wi-Fi pagination uses the primary-action appearance, matching scan buttons.
 *
 * Helpers configure appearance only. Call sites remain responsible for
 * geometry, event callbacks, and navigation behavior.
 */

#pragma once

#include <lvgl.h>

#include <cstdint>

namespace wirelessrecon::ui::theme {

/**
 * @brief Shared WirelessRecon color palette.
 */
namespace Colors {
inline constexpr uint32_t background    = 0x08141B;
inline constexpr uint32_t surface       = 0x10232D;
inline constexpr uint32_t surfaceRaised = 0x17313D;
inline constexpr uint32_t border        = 0x254653;
inline constexpr uint32_t primary       = 0x39E6C0;
inline constexpr uint32_t secondary     = 0x56B4D3;
inline constexpr uint32_t textPrimary   = 0xE5F0F4;
inline constexpr uint32_t textSecondary = 0x91A8B5;
inline constexpr uint32_t textMuted     = 0x627985;
inline constexpr uint32_t success       = 0x45D483;
inline constexpr uint32_t warning       = 0xF0C674;
inline constexpr uint32_t error         = 0xEF6461;
}

/**
 * @brief Shared layout dimensions in LVGL logical pixels.
 */
namespace Spacing {
inline constexpr int32_t compact      = 4;
inline constexpr int32_t standard     = 8;
inline constexpr int32_t section      = 12;
inline constexpr int32_t screenMargin = 8;
inline constexpr int32_t cardRadius   = 8;
inline constexpr int32_t borderWidth  = 1;
}

/**
 * @brief Logical destinations represented by the persistent bottom bar.
 */
enum class NavigationDestination : uint8_t {
    Dashboard,
    Wifi,
    Bluetooth,
    Settings
};

/**
 * @brief Applies the application background without a visible border.
 *
 * @param object LVGL object to style.
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

    lv_obj_set_style_radius(
        object,
        0,
        LV_PART_MAIN
    );
}

/**
 * @brief Applies the standard raised panel appearance.
 *
 * @param object LVGL object to style.
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

    lv_obj_set_style_shadow_width(
        object,
        0,
        LV_PART_MAIN
    );
}

/**
 * @brief Applies the original neutral Back-button appearance.
 *
 * @details
 * This is the established appearance used by the Wi-Fi Back button.
 * Global bottom-navigation buttons reuse this same styling.
 *
 * This helper deliberately does not set width or height. The original
 * Back-button geometry remains controlled by its screen call site.
 *
 * Do not apply scan-button styling to Back or bottom-navigation controls.
 *
 * @param button LVGL button object to style.
 */
inline void applyNavigationButton(lv_obj_t* button) {
    if (button == nullptr) {
        return;
    }

    lv_obj_set_style_bg_color(
        button,
        lv_color_hex(Colors::surfaceRaised),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        button,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_color(
        button,
        lv_color_hex(Colors::border),
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        button,
        Spacing::borderWidth,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        button,
        Spacing::cardRadius,
        LV_PART_MAIN
    );

    lv_obj_set_style_shadow_width(
        button,
        0,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        button,
        lv_color_hex(Colors::textPrimary),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        button,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );
}

/**
 * @brief Applies the established cyan-accented scan-action appearance.
 *
 * @details
 * This preserves the original Dashboard scan-button styling:
 * - Raised surface background.
 * - Border color changes to the primary cyan accent.
 * - Original pressed background.
 * - Existing card radius.
 * - No shadow.
 * - Zero pressed transform dimensions.
 *
 * Geometry remains the caller's responsibility.
 *
 * @param button LVGL button object to style.
 */
inline void applyPrimaryActionButton(lv_obj_t* button) {
    if (button == nullptr) {
        return;
    }

    lv_obj_set_style_bg_color(
        button,
        lv_color_hex(Colors::surfaceRaised),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_color(
        button,
        lv_color_hex(Colors::border),
        LV_STATE_PRESSED
    );

    lv_obj_set_style_border_color(
        button,
        lv_color_hex(Colors::primary),
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        button,
        Spacing::borderWidth,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        button,
        Spacing::cardRadius,
        LV_PART_MAIN
    );

    lv_obj_set_style_shadow_width(
        button,
        0,
        LV_PART_MAIN
    );

    lv_obj_set_style_transform_width(
        button,
        0,
        LV_PART_MAIN
    );

    lv_obj_set_style_transform_height(
        button,
        0,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        button,
        lv_color_hex(Colors::textPrimary),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        button,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );
}

/**
 * @brief Applies a subtle selected-state accent to a navigation button.
 *
 * @details
 * The base shape and background remain identical to the Back button.
 * Only the border and text color change to identify the active destination.
 *
 * @param button Navigation button to mark selected.
 */
inline void applyNavigationSelected(lv_obj_t* button) {
    if (button == nullptr) {
        return;
    }

    lv_obj_set_style_border_color(
        button,
        lv_color_hex(Colors::primary),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        button,
        lv_color_hex(Colors::primary),
        LV_PART_MAIN
    );
}

/**
 * @brief Marks a visible navigation destination as unavailable.
 *
 * @details
 * The button retains the neutral navigation appearance but is disabled
 * and uses muted text. This is intended for unimplemented destinations
 * such as Bluetooth and Settings.
 *
 * @param button Navigation button to disable.
 */
inline void applyNavigationDisabled(lv_obj_t* button) {
    if (button == nullptr) {
        return;
    }

    lv_obj_add_state(
        button,
        LV_STATE_DISABLED
    );

    lv_obj_set_style_text_color(
        button,
        lv_color_hex(Colors::textMuted),
        LV_PART_MAIN
    );
}

} // namespace wirelessrecon::ui::theme