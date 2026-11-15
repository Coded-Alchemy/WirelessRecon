/**
 * @file ResponsiveLayout.h
 * @brief Selects a responsive layout profile from display capabilities.
 *
 * This class is backend-independent and has no runtime allocation.
 *
 * Selection order:
 * 1. Explicit wearable form factor.
 * 2. Explicit tablet or desktop-panel form factor.
 * 3. Automatic selection from usable dimensions.
 *
 * A screen's shape and input capabilities remain available to screen
 * components for further adaptation. For example, circular displays should
 * apply their configured safe insets to keep text and controls out of corners.
 */

#pragma once

#include "ui/display/DisplayCapabilities.h"
#include "ui/layout/Breakpoints.h"
#include "ui/layout/LayoutProfile.h"

namespace wirelessrecon {
namespace ui {
namespace layout {

class ResponsiveLayout {
public:
    /**
     * Selects a layout profile for the supplied display.
     *
     * Invalid dimensions return a conservative compact profile. This avoids
     * division-by-zero and prevents an uninitialized display from selecting
     * an expansive layout.
     */
    static LayoutProfile select(
        const display::DisplayCapabilities& capabilities
    ) {
        if (!capabilities.isValid()) {
            return compactProfile(0);
        }

        const uint16_t contentWidth = capabilities.contentWidth();
        const uint16_t shortestSide = capabilities.shortestContentSide();

        if (capabilities.formFactor == display::FormFactor::Wearable) {
            return wearableProfile(contentWidth);
        }

        if (capabilities.formFactor == display::FormFactor::DesktopPanel) {
            return wideProfile(contentWidth);
        }

        if (capabilities.formFactor == display::FormFactor::Tablet) {
            if (contentWidth >= Breakpoints::kWideMinContentWidth) {
                return wideProfile(contentWidth);
            }
            return expandedProfile(contentWidth);
        }

        if (contentWidth >= Breakpoints::kWideMinContentWidth) {
            return wideProfile(contentWidth);
        }

        if (shortestSide >= Breakpoints::kExpandedMinShortestSide) {
            return expandedProfile(contentWidth);
        }

        if (shortestSide >= Breakpoints::kStandardMinShortestSide) {
            return standardProfile(contentWidth);
        }

        return compactProfile(contentWidth);
    }

private:
    static LayoutProfile wearableProfile(uint16_t contentWidth) {
        LayoutProfile profile;
        profile.layoutClass = LayoutClass::Wearable;
        profile.navigationMode = NavigationMode::Paginated;
        profile.contentDensity = ContentDensity::Comfortable;
        profile.columns = 1;
        profile.outerMargin = 6;
        profile.itemSpacing = 6;
        profile.sectionSpacing = 8;
        profile.minimumTouchTarget =
            Breakpoints::kWearableTouchTarget;
        profile.maximumContentWidth = contentWidth;
        profile.useCompactLabels = true;
        profile.useSinglePurposeScreens = true;
        profile.allowMultiColumnCards = false;
        profile.preferPagedLists = true;
        return profile;
    }

    static LayoutProfile compactProfile(uint16_t contentWidth) {
        LayoutProfile profile;
        profile.layoutClass = LayoutClass::Compact;
        profile.navigationMode = NavigationMode::Paginated;
        profile.contentDensity = ContentDensity::Comfortable;
        profile.columns = 1;
        profile.outerMargin = Breakpoints::kCompactMargin;
        profile.itemSpacing = 8;
        profile.sectionSpacing = 12;
        profile.minimumTouchTarget =
            Breakpoints::kCompactTouchTarget;
        profile.maximumContentWidth = contentWidth;
        profile.useCompactLabels = true;
        profile.useSinglePurposeScreens = true;
        profile.allowMultiColumnCards = false;
        profile.preferPagedLists = true;
        return profile;
    }

    static LayoutProfile standardProfile(uint16_t contentWidth) {
        LayoutProfile profile;
        profile.layoutClass = LayoutClass::Standard;
        profile.navigationMode = NavigationMode::BottomNavigation;
        profile.contentDensity = ContentDensity::Balanced;
        profile.columns = 1;
        profile.outerMargin = Breakpoints::kStandardMargin;
        profile.itemSpacing = 12;
        profile.sectionSpacing = 16;
        profile.minimumTouchTarget =
            Breakpoints::kStandardTouchTarget;
        profile.maximumContentWidth = contentWidth;
        profile.useCompactLabels = false;
        profile.useSinglePurposeScreens = false;
        profile.allowMultiColumnCards = false;
        profile.preferPagedLists = false;
        return profile;
    }

    static LayoutProfile expandedProfile(uint16_t contentWidth) {
        LayoutProfile profile;
        profile.layoutClass = LayoutClass::Expanded;
        profile.navigationMode = NavigationMode::NavigationRail;
        profile.contentDensity = ContentDensity::Balanced;
        profile.columns = 2;
        profile.outerMargin = Breakpoints::kExpandedMargin;
        profile.itemSpacing = 16;
        profile.sectionSpacing = 20;
        profile.minimumTouchTarget =
            Breakpoints::kStandardTouchTarget;
        profile.maximumContentWidth = contentWidth;
        profile.useCompactLabels = false;
        profile.useSinglePurposeScreens = false;
        profile.allowMultiColumnCards = true;
        profile.preferPagedLists = false;
        return profile;
    }

    static LayoutProfile wideProfile(uint16_t contentWidth) {
        LayoutProfile profile;
        profile.layoutClass = LayoutClass::Wide;
        profile.navigationMode = NavigationMode::Sidebar;
        profile.contentDensity = ContentDensity::Dense;
        profile.columns = 3;
        profile.outerMargin = Breakpoints::kWideMargin;
        profile.itemSpacing = 20;
        profile.sectionSpacing = 24;
        profile.minimumTouchTarget =
            Breakpoints::kStandardTouchTarget;
        profile.maximumContentWidth = contentWidth;
        profile.useCompactLabels = false;
        profile.useSinglePurposeScreens = false;
        profile.allowMultiColumnCards = true;
        profile.preferPagedLists = false;
        return profile;
    }
};

} // namespace layout
} // namespace ui
} // namespace wirelessrecon
