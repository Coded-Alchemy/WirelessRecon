
/**
 * @file UITheme.h
 * @brief Shared visual design tokens and styling helpers for WirelessRecon.
 *
 * @details
 * UITheme defines the presentation layer's shared visual language.
 *
 * Responsibilities:
 * - Centralize application colors, spacing, and control geometry.
 * - Provide reusable panel and button styling.
 * - Define shared navigation destinations.
 *
 * This header has no dependency on radio hardware or scanner services.
 * Styling helpers do not take ownership of LVGL objects.
 */

#pragma once

#include <cstdint>
#include <lvgl.h>

namespace wirelessrecon::ui::theme {

/** @brief Shared RGB color palette in 0xRRGGBB format. */
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

/** @brief Shared spacing and geometry recommendations. */
struct Spacing {
    static constexpr uint8_t compact = 4;
    static constexpr uint8_t standard = 8;
    static constexpr uint8_t section = 12;
    static constexpr uint8_t screenMargin = 8;
    static constexpr uint8_t cardRadius = 8;
    static constexpr uint8_t borderWidth = 1;

    static constexpr uint8_t networkCardHorizontal = 12;
    static constexpr uint8_t networkCardVertical = 8;
    static constexpr uint8_t networkCardLineGap = 4;
    static constexpr uint8_t networkCardGap = 6;

    static constexpr uint8_t scrollGutter = 9;
};

/**
 * @brief Logical destinations exposed by the application navigation bar.
 *
 * Bluetooth and Settings are reserved destinations. Their navigation
 * buttons remain disabled until their respective screens are implemented.
 */
enum class NavigationDestination : uint8_t {
    Dashboard = 0,
    Wifi = 1,
    Bluetooth = 2,
    Settings = 3
};

/**
 * @brief Applies the application background to an LVGL object.
 */
inline void applyBackground(lv_obj_t* object) {
    if (object == nullptr) {
        return;
    }

    lv_obj_set_style_bg_color(
        object, lv_color_hex(Colors::background), LV_PART_MAIN
    );
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
}

/**
 * @brief Applies the standard raised-panel appearance.
 */
inline void applyPanel(lv_obj_t* object) {
    if (object == nullptr) {
        return;
    }

    lv_obj_set_style_bg_color(
        object, lv_color_hex(Colors::surface), LV_PART_MAIN
    );
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(
        object, lv_color_hex(Colors::border), LV_PART_MAIN
    );
    lv_obj_set_style_border_width(
        object, Spacing::borderWidth, LV_PART_MAIN
    );
    lv_obj_set_style_radius(
        object, Spacing::cardRadius, LV_PART_MAIN
    );
}

/**
 * @brief Applies the standard Back/navigation button appearance.
 *
 * This is the visual style extracted from the original Wi-Fi Back button.
 * The caller remains responsible for button dimensions and label creation.
 */
inline void applyNavigationButton(lv_obj_t* button) {
    if (button == nullptr) {
        return;
    }

    lv_obj_set_style_bg_color(
        button, lv_color_hex(Colors::surfaceRaised), LV_PART_MAIN
    );
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(
        button, lv_color_hex(Colors::border), LV_PART_MAIN
    );
    lv_obj_set_style_border_width(
        button, Spacing::borderWidth, LV_PART_MAIN
    );
    lv_obj_set_style_radius(
        button, Spacing::cardRadius, LV_PART_MAIN
    );
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
}

/**
 * @brief Applies the approved scan-button appearance.
 *
 * Previous and Next use this style so pagination controls match the
 * existing Wi-Fi and BLE scan actions.
 */
inline void applyPrimaryActionButton(lv_obj_t* button) {
    if (button == nullptr) {
        return;
    }

    lv_obj_set_style_bg_color(
        button, lv_color_hex(Colors::surfaceRaised), LV_PART_MAIN
    );
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(
        button, lv_color_hex(Colors::border), LV_STATE_PRESSED
    );

    lv_obj_set_style_border_color(
        button, lv_color_hex(Colors::primary), LV_PART_MAIN
    );
    lv_obj_set_style_border_width(
        button, Spacing::borderWidth, LV_PART_MAIN
    );
    lv_obj_set_style_radius(
        button, Spacing::cardRadius, LV_PART_MAIN
    );
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);

    // Prevent pressed-state geometry changes from clipping the control.
    lv_obj_set_style_transform_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_height(button, 0, LV_PART_MAIN);

    lv_obj_set_style_bg_color(
        button, lv_color_hex(Colors::surface), LV_STATE_DISABLED
    );
    lv_obj_set_style_border_color(
        button, lv_color_hex(Colors::border), LV_STATE_DISABLED
    );
    lv_obj_set_style_text_color(
        button, lv_color_hex(Colors::textMuted), LV_STATE_DISABLED
    );
}

/**
 * @brief Applies shared bottom-navigation item styling.
 *
 * @param button Navigation button.
 * @param selected True when this item represents the active screen.
 * @param enabled False for destinations that are not implemented yet.
 */
inline void applyBottomNavigationButton(
    lv_obj_t* button,
    bool selected,
    bool enabled = true
) {
    if (button == nullptr) {
        return;
    }

    const uint32_t background =
        selected ? Colors::surfaceRaised : Colors::surface;

    lv_obj_set_style_bg_color(
        button, lv_color_hex(background), LV_PART_MAIN
    );
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_border_color(
        button,
        lv_color_hex(selected ? Colors::primary : Colors::border),
        LV_PART_MAIN
    );
    lv_obj_set_style_border_width(
        button, Spacing::borderWidth, LV_PART_MAIN
    );
    lv_obj_set_style_radius(
        button, Spacing::cardRadius, LV_PART_MAIN
    );
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);

    lv_obj_set_style_bg_color(
        button, lv_color_hex(Colors::border), LV_STATE_PRESSED
    );

    if (!enabled) {
        lv_obj_add_state(button, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(
            button, lv_color_hex(Colors::surface), LV_PART_MAIN
        );
        lv_obj_set_style_border_color(
            button, lv_color_hex(Colors::border), LV_PART_MAIN
        );
    }
}

/**
 * @brief Applies the shared text appearance to a button label.
 */
inline void applyButtonLabel(lv_obj_t* label, bool enabled = true) {
    if (label == nullptr) {
        return;
    }

    lv_obj_set_style_text_color(
        label,
        lv_color_hex(enabled ? Colors::textPrimary : Colors::textMuted),
        LV_PART_MAIN
    );
    lv_obj_set_style_text_font(
        label, &lv_font_montserrat_14, LV_PART_MAIN
    );
}

} // namespace wirelessrecon::ui::theme
