/**
 * @file LayoutProfile.h
 * @brief Describes the layout strategy selected for a display.
 *
 * A LayoutProfile is a set of recommendations for screen composition.
 * It does not create widgets or render content. UI screens use the profile
 * to decide how much information to show and how to arrange it.
 */

#pragma once

#include <cstdint>

namespace wirelessrecon {
namespace ui {
namespace layout {

/** Responsive presentation class selected for the active display. */
enum class LayoutClass : uint8_t {
    Wearable = 0,
    Compact,
    Standard,
    Expanded,
    Wide
};

/** Navigation pattern appropriate to the available display and input. */
enum class NavigationMode : uint8_t {
    Paginated = 0,
    BottomNavigation,
    NavigationRail,
    Sidebar
};

/** Recommended density for cards, rows, and information panels. */
enum class ContentDensity : uint8_t {
    Comfortable = 0,
    Balanced,
    Dense
};

/**
 * Layout recommendations shared by all UI backends.
 *
 * Values are logical pixels unless otherwise stated. They are minimum
 * usability recommendations, not instructions to shrink content below
 * readable or touch-accessible dimensions.
 */
struct LayoutProfile {
    LayoutClass layoutClass = LayoutClass::Compact;
    NavigationMode navigationMode = NavigationMode::Paginated;
    ContentDensity contentDensity = ContentDensity::Comfortable;

    uint8_t columns = 1;
    uint8_t outerMargin = 8;
    uint8_t itemSpacing = 8;
    uint8_t sectionSpacing = 12;

    uint16_t minimumTouchTarget = 40;
    uint16_t maximumContentWidth = 0;

    bool useCompactLabels = true;
    bool useSinglePurposeScreens = true;
    bool allowMultiColumnCards = false;
    bool preferPagedLists = true;
};

} // namespace layout
} // namespace ui
} // namespace wirelessrecon
