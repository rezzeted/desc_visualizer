#pragma once

#include <cstddef>

namespace diagram_placement {

// Shared layout constants for class diagram blocks (used by placer and renderer).
// All values in world units (double); renderer may cast to float.

namespace layout {

constexpr double button_size = 20.0;
constexpr double padding = 8.0;
constexpr double header_height = 28.0;
constexpr double section_header_height = 20.0;
// Base row height in world units; text is centered inside each row.
constexpr double row_height = 18.0;
constexpr double row_vertical_text_padding = 2.0;
// Small spacing between adjacent rows inside the same group (~33% of row height).
constexpr double row_inner_gap = row_height * 0.33;

constexpr double accent_bar_width = 3.0;
constexpr double content_indent = 4.0;
constexpr double section_gap = 2.0;
constexpr double group_gap = 6.0;
// Unified list layout: indent for group items (tree), large gap between groups.
constexpr double group_item_indent = 12.0;
// Group gap is intentionally larger than inner row gap.
constexpr double group_vertical_gap = row_inner_gap * 1.75;
constexpr double list_marker_radius = 2.0;
constexpr double list_marker_gap = 4.0; // gap between marker and row text

constexpr double content_inset_top = 6.0;
constexpr double content_inset_bottom = 10.0;
constexpr double content_inset_side = 6.0;
constexpr double header_content_gap = 4.0; // gap after header line before first section

constexpr double collapsed_width = 140.0;
constexpr double collapsed_height = 32.0;
constexpr double expanded_min_width = 180.0;

constexpr double block_margin = 16.0;
constexpr double gap = 8.0;

// Nested expansion: expanding parent/child classes inline inside a card.
constexpr double nesting_indent = 14.0;        // extra left indent per nesting level
constexpr double nested_button_size = 14.0;     // [+/-] button size for nested items
constexpr int max_nesting_depth = 10;           // safety limit to prevent runaway recursion

// Group row: minimal left offset (text after accent + padding + indent).
inline constexpr double group_row_left_offset() {
    return padding + accent_bar_width + content_indent;
}
// Text column start from content area left.
inline constexpr double content_left_offset() {
    return group_row_left_offset();
}
// Item row indent is twice the group row indent (for background/accent only; text aligns with group).
inline constexpr double item_row_indent() {
    return 2.0 * group_row_left_offset();
}
// Total horizontal padding: all text aligned at group_row_left_offset, then right padding.
inline constexpr double content_width_padding() {
    return group_row_left_offset() + padding;
}

// Empty groups still render a placeholder row.
inline constexpr std::size_t visible_item_rows(std::size_t raw_rows) {
    return raw_rows == 0u ? 1u : raw_rows;
}

// Safety bound for runtime row height based on current font metrics.
inline constexpr double min_row_height_for_font(double font_world_height) {
    return font_world_height + 2.0 * row_vertical_text_padding;
}

// Expanded content height without outer insets/header area.
inline constexpr double expanded_content_height(std::size_t parent_items,
    std::size_t prop_items,
    std::size_t comp_items,
    std::size_t child_items,
    double effective_row_height = row_height)
{
    const double parent_visible = static_cast<double>(visible_item_rows(parent_items));
    const double prop_visible = static_cast<double>(visible_item_rows(prop_items));
    const double comp_visible = static_cast<double>(visible_item_rows(comp_items));
    const double child_visible = static_cast<double>(visible_item_rows(child_items));
    const double row_gap_ratio = row_height > 0.0 ? (row_inner_gap / row_height) : 0.0;
    const double group_gap_ratio = row_height > 0.0 ? (group_vertical_gap / row_height) : 0.0;
    const double effective_row_inner_gap = effective_row_height * row_gap_ratio;
    const double effective_group_vertical_gap = effective_row_height * group_gap_ratio;
    return (1.0 + parent_visible) * effective_row_height
        + (1.0 + prop_visible) * effective_row_height
        + (1.0 + comp_visible) * effective_row_height
        + (1.0 + child_visible) * effective_row_height
        + (parent_visible + prop_visible + comp_visible + child_visible) * effective_row_inner_gap
        + 3.0 * effective_group_vertical_gap;
}

} // namespace layout
} // namespace diagram_placement
