/**
 * @file test_main.cpp
 * @brief Native Unity tests for responsive layout profile selection.
 *
 * Covers automatic breakpoint selection, explicit form-factor overrides,
 * safe-area calculations, and invalid display dimensions.
 */

#include <unity.h>
#include <cstdint>

#include "ui/display/DisplayCapabilities.h"
#include "ui/layout/ResponsiveLayout.h"

using namespace wirelessrecon::ui::display;
using namespace wirelessrecon::ui::layout;

namespace {

void test_compact_display_selects_compact_profile() {
    DisplayCapabilities display;
    display.width = 240;
    display.height = 320;

    const LayoutProfile profile = ResponsiveLayout::select(display);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(LayoutClass::Compact),
        static_cast<uint8_t>(profile.layoutClass));
    TEST_ASSERT_EQUAL_UINT8(1, profile.columns);
    TEST_ASSERT_TRUE(profile.preferPagedLists);
}

void test_explicit_wearable_overrides_display_dimensions() {
    DisplayCapabilities display;
    display.width = 240;
    display.height = 240;
    display.shape = ScreenShape::Circle;
    display.formFactor = FormFactor::Wearable;

    const LayoutProfile profile = ResponsiveLayout::select(display);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(LayoutClass::Wearable),
        static_cast<uint8_t>(profile.layoutClass));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NavigationMode::Paginated),
        static_cast<uint8_t>(profile.navigationMode));
    TEST_ASSERT_EQUAL_UINT8(1, profile.columns);
}

void test_explicit_tablet_selects_expanded_profile() {
    DisplayCapabilities display;
    display.width = 800;
    display.height = 1280;
    display.formFactor = FormFactor::Tablet;

    const LayoutProfile profile = ResponsiveLayout::select(display);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(LayoutClass::Expanded),
        static_cast<uint8_t>(profile.layoutClass));
    TEST_ASSERT_EQUAL_UINT8(2, profile.columns);
}

void test_explicit_desktop_panel_selects_wide_profile() {
    DisplayCapabilities display;
    display.width = 1920;
    display.height = 1080;
    display.formFactor = FormFactor::DesktopPanel;

    const LayoutProfile profile = ResponsiveLayout::select(display);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(LayoutClass::Wide),
        static_cast<uint8_t>(profile.layoutClass));
    TEST_ASSERT_EQUAL_UINT8(3, profile.columns);
}

void test_safe_insets_reduce_content_width() {
    DisplayCapabilities display;
    display.width = 480;
    display.height = 800;
    display.safeInsets.left = 20;
    display.safeInsets.right = 20;

    TEST_ASSERT_EQUAL_UINT16(440, display.contentWidth());
}

void test_invalid_dimensions_fall_back_to_compact_profile() {
    DisplayCapabilities display;
    display.width = 0;
    display.height = 320;

    const LayoutProfile profile = ResponsiveLayout::select(display);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(LayoutClass::Compact),
        static_cast<uint8_t>(profile.layoutClass));
}

} // namespace

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_compact_display_selects_compact_profile);
    RUN_TEST(test_explicit_wearable_overrides_display_dimensions);
    RUN_TEST(test_explicit_tablet_selects_expanded_profile);
    RUN_TEST(test_explicit_desktop_panel_selects_wide_profile);
    RUN_TEST(test_safe_insets_reduce_content_width);
    RUN_TEST(test_invalid_dimensions_fall_back_to_compact_profile);

    return UNITY_END();
}
