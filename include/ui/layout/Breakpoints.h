/**
 * @file Breakpoints.h
 * @brief Centralized responsive-layout thresholds.
 *
 * Breakpoints are based on usable logical pixels, not board identity.
 * They are starting values for layout selection and should be tuned using
 * hardware usability testing. They are not font sizes or display modes.
 */

#pragma once

#include <cstdint>

namespace wirelessrecon {
namespace ui {
namespace layout {

struct Breakpoints {
    /**
     * Below this usable shortest-side dimension, use compact layouts.
     * This naturally includes the current 240x320 target.
     */
    static constexpr uint16_t kStandardMinShortestSide = 420;

    /** Use expanded layouts when the shortest usable side reaches this size. */
    static constexpr uint16_t kExpandedMinShortestSide = 600;

    /**
     * Use wide layouts when usable width reaches this value.
     * This allows landscape tablets and large panels to use extra columns.
     */
    static constexpr uint16_t kWideMinContentWidth = 1100;

    /** Minimum recommended touch target for compact touch displays. */
    static constexpr uint16_t kCompactTouchTarget = 40;

    /** Minimum recommended touch target for standard and larger displays. */
    static constexpr uint16_t kStandardTouchTarget = 44;

    /** Conservative target for wearable controls. */
    static constexpr uint16_t kWearableTouchTarget = 40;

    /** Default outer content margin for compact layouts. */
    static constexpr uint8_t kCompactMargin = 8;

    /** Default outer content margin for standard layouts. */
    static constexpr uint8_t kStandardMargin = 16;

    /** Default outer content margin for expanded layouts. */
    static constexpr uint8_t kExpandedMargin = 24;

    /** Default outer content margin for wide layouts. */
    static constexpr uint8_t kWideMargin = 32;
};

} // namespace layout
} // namespace ui
} // namespace wirelessrecon
