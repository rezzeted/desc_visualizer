#include <diagram_render/renderer.hpp>
#include <diagram_render/nested_hit_button.hpp>
#include <diagram_placement/class_diagram_layout_constants.hpp>
#include <diagram_placement/class_diagram_placement.hpp>
#include <diagram_model/class_diagram.hpp>
#include "imgui.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace diagram_render {

namespace {

using namespace diagram_placement::layout;

ImVec2 world_to_screen(float wx, float wy, float offset_x, float offset_y, float zoom) {
    return ImVec2(wx * zoom + offset_x, wy * zoom + offset_y);
}

const diagram_model::DiagramClass* find_class(const diagram_model::ClassDiagram& diagram, const std::string& id) {
    for (const auto& c : diagram.classes)
        if (c.id == id) return &c;
    return nullptr;
}

bool is_nested_expanded(const std::unordered_map<std::string, bool>& nested_expanded,
    const std::string& key)
{
    auto it = nested_expanded.find(key);
    return it != nested_expanded.end() && it->second;
}

// Shared rendering context passed to the recursive content renderer.
struct RenderContext {
    ImDrawList* draw_list;
    const diagram_model::ClassDiagram& diagram;
    float offset_x;
    float offset_y;
    float zoom;
    float safe_zoom;
    ImFont* font;
    float scaled_font_size;
    float font_world_height;
    float f_row_height_effective;
    float f_row_inner_gap_effective;
    float f_group_vertical_gap_effective;
    float f_content_inset_side;
    float f_accent_bar_width;
    float f_content_indent;
    float f_nested_button_size;
    float f_nav_button_size;
    float f_nav_button_gap;
    float section_rounding;
    // Colors.
    unsigned int text_color;
    unsigned int type_muted_color;
    unsigned int empty_color;
    unsigned int button_bg;
    unsigned int border_color;
    unsigned int parent_bg;
    unsigned int parent_header_color;
    unsigned int parent_accent;
    unsigned int properties_bg;
    unsigned int properties_header_color;
    unsigned int properties_accent;
    unsigned int components_bg;
    unsigned int components_header_color;
    unsigned int components_accent;
    unsigned int children_bg;
    unsigned int children_header_color;
    unsigned int children_accent;
    // Nested card visuals -- parent context (blue tint).
    unsigned int parent_card_bg;
    unsigned int parent_card_border;
    unsigned int parent_card_header_bg;
    // Nested card visuals -- child context (purple tint).
    unsigned int child_card_bg;
    unsigned int child_card_border;
    unsigned int child_card_header_bg;
    unsigned int nav_button_color;
    // Nested state.
    const std::unordered_map<std::string, bool>& nested_expanded;
    std::vector<NestedHitButton>* out_hit_buttons;
    std::vector<NavHitButton>* out_nav_buttons;
    std::vector<ClassHoverRegion>* out_hover_regions;
};

// Draw a small [+] or [-] button for nested expand/collapse.
// Returns the world-space rect as a NestedHitButton (without path/id filled).
void draw_nested_button(const RenderContext& ctx, float btn_x, float btn_y, bool is_expanded) {
    const float bs = ctx.f_nested_button_size;
    ImVec2 btn_min = world_to_screen(btn_x, btn_y, ctx.offset_x, ctx.offset_y, ctx.zoom);
    ImVec2 btn_max = world_to_screen(btn_x + bs, btn_y + bs, ctx.offset_x, ctx.offset_y, ctx.zoom);
    ctx.draw_list->AddRectFilled(btn_min, btn_max, ctx.button_bg);
    ctx.draw_list->AddRect(btn_min, btn_max, ctx.border_color, 0.0f, 0, 1.0f);

    ImVec2 center = world_to_screen(btn_x + bs * 0.5f, btn_y + bs * 0.5f,
        ctx.offset_x, ctx.offset_y, ctx.zoom);
    float half = 3.0f * ctx.zoom;
    // Horizontal line (always present).
    ctx.draw_list->AddLine(
        ImVec2(center.x - half, center.y),
        ImVec2(center.x + half, center.y),
        ctx.text_color, 1.2f);
    if (!is_expanded) {
        // Vertical line (only when collapsed -- makes a '+').
        ctx.draw_list->AddLine(
            ImVec2(center.x, center.y - half),
            ImVec2(center.x, center.y + half),
            ctx.text_color, 1.2f);
    }
}

// Record a hit region for a nested button.
void record_hit_button(const RenderContext& ctx,
    const std::string& block_class_id,
    const std::string& path,
    float btn_x, float btn_y)
{
    if (!ctx.out_hit_buttons) return;
    NestedHitButton hb;
    hb.block_class_id = block_class_id;
    hb.path = path;
    hb.x = static_cast<double>(btn_x);
    hb.y = static_cast<double>(btn_y);
    hb.w = static_cast<double>(ctx.f_nested_button_size);
    hb.h = static_cast<double>(ctx.f_nested_button_size);
    ctx.out_hit_buttons->push_back(std::move(hb));
}

// Draw a small navigation arrow button (right-pointing triangle).
void draw_nav_button(const RenderContext& ctx, float btn_x, float btn_y) {
    const float bs = ctx.f_nav_button_size;
    ImVec2 btn_min = world_to_screen(btn_x, btn_y, ctx.offset_x, ctx.offset_y, ctx.zoom);
    ImVec2 btn_max = world_to_screen(btn_x + bs, btn_y + bs, ctx.offset_x, ctx.offset_y, ctx.zoom);
    ctx.draw_list->AddRectFilled(btn_min, btn_max, ctx.button_bg);
    ctx.draw_list->AddRect(btn_min, btn_max, ctx.border_color, 0.0f, 0, 1.0f);

    // Draw a right-pointing triangle (arrow) inside the button.
    ImVec2 center = world_to_screen(btn_x + bs * 0.5f, btn_y + bs * 0.5f,
        ctx.offset_x, ctx.offset_y, ctx.zoom);
    float h = 3.0f * ctx.zoom;  // half-height of triangle
    float w = 2.5f * ctx.zoom;  // half-width of triangle
    ImVec2 p1(center.x - w, center.y - h);  // top-left
    ImVec2 p2(center.x + w, center.y);      // right-center
    ImVec2 p3(center.x - w, center.y + h);  // bottom-left
    ctx.draw_list->AddTriangleFilled(p1, p2, p3, ctx.nav_button_color);
}

// Record a hit region for a navigation button.
void record_nav_button(const RenderContext& ctx,
    const std::string& target_class_id,
    float btn_x, float btn_y)
{
    if (!ctx.out_nav_buttons) return;
    NavHitButton nb;
    nb.target_class_id = target_class_id;
    nb.x = static_cast<double>(btn_x);
    nb.y = static_cast<double>(btn_y);
    nb.w = static_cast<double>(ctx.f_nav_button_size);
    nb.h = static_cast<double>(ctx.f_nav_button_size);
    ctx.out_nav_buttons->push_back(std::move(nb));
}

// Record a hover region that maps an area to a target class.
void record_hover_region(const RenderContext& ctx,
    const std::string& target_class_id,
    float rx, float ry, float rw, float rh)
{
    if (!ctx.out_hover_regions) return;
    ClassHoverRegion hr;
    hr.target_class_id = target_class_id;
    hr.x = static_cast<double>(rx);
    hr.y = static_cast<double>(ry);
    hr.w = static_cast<double>(rw);
    hr.h = static_cast<double>(rh);
    ctx.out_hover_regions->push_back(std::move(hr));
}

// Colors for a nested card (mini-block drawn inside a parent card).
struct NestedCardColors {
    unsigned int bg;
    unsigned int border;
    unsigned int header_bg;
};

// Draw a mini-card (block-inside-block) around nested expanded content.
// Caller provides exact card bounds (no internal padding applied here).
// Uses draw list channels: card background/header on channel 0,
// border + header text on channel 1 (so they render on top of row backgrounds).
void draw_nested_card(const RenderContext& ctx,
    float card_left, float card_top, float card_right, float card_bottom,
    const char* class_name, const NestedCardColors& colors)
{
    const float card_rounding = 6.0f;
    const float border_thickness = 2.0f;
    const float f_header_h = static_cast<float>(nested_header_height);
    const float header_bottom = card_top + f_header_h;

    ImVec2 cmin = world_to_screen(card_left, card_top, ctx.offset_x, ctx.offset_y, ctx.zoom);
    ImVec2 cmax = world_to_screen(card_right, card_bottom, ctx.offset_x, ctx.offset_y, ctx.zoom);

    // --- Channel 0: card background + header bg (behind row content) ---
    ctx.draw_list->ChannelsSetCurrent(0);

    // Card body background.
    ctx.draw_list->AddRectFilled(cmin, cmax, colors.bg, card_rounding);

    // Header bar background (top portion with rounded top corners).
    ImVec2 hdr_max = world_to_screen(card_right, header_bottom, ctx.offset_x, ctx.offset_y, ctx.zoom);
    ctx.draw_list->AddRectFilled(cmin, hdr_max, colors.header_bg, card_rounding,
        ImDrawFlags_RoundCornersTop);

    // --- Channel 1: border + header text + separator (on top of row content) ---
    ctx.draw_list->ChannelsSetCurrent(1);

    // Card border.
    ctx.draw_list->AddRect(cmin, cmax, colors.border, card_rounding, 0, border_thickness);

    // Separator line under header.
    ImVec2 sep_left = world_to_screen(card_left, header_bottom, ctx.offset_x, ctx.offset_y, ctx.zoom);
    ImVec2 sep_right = world_to_screen(card_right, header_bottom, ctx.offset_x, ctx.offset_y, ctx.zoom);
    ctx.draw_list->AddLine(sep_left, sep_right, colors.border, 1.0f);

    // Header class name text.
    const float text_pad = 6.0f;
    const float text_y = card_top + (f_header_h - ctx.font_world_height) * 0.5f;
    ctx.draw_list->AddText(ctx.font, ctx.scaled_font_size,
        world_to_screen(card_left + text_pad, text_y, ctx.offset_x, ctx.offset_y, ctx.zoom),
        ctx.text_color, class_name);
}

// Recursively render the 4 content sections (Parent, Properties, Components, Children)
// of a class inside a block card.
// area_left, area_right: content area bounds (rows are drawn within these).
// cy: current y position (world); returns the new cy after rendering.
// depth: nesting depth (0 = direct content of the block), used for depth limit.
// path_prefix: tree path prefix, e.g. "Player/" or "Player/parent/".
// block_class_id: the top-level block's class id (for hit button recording).
// visited: set of class ids already on the current expansion path (cycle guard).
float render_class_content(
    const RenderContext& ctx,
    const diagram_model::DiagramClass& cls,
    float area_left, float area_right,
    float cy,
    int depth,
    const std::string& path_prefix,
    const std::string& block_class_id,
    std::unordered_set<std::string>& visited)
{
    const float content_x = area_left;
    const float content_right = area_right;
    const float content_w = content_right - content_x;
    if (content_w <= 0.0f) return cy;

    const float list_text_left = content_x + static_cast<float>(content_left_offset());
    const float item_left = std::max(content_x,
        list_text_left - (ctx.f_accent_bar_width + ctx.f_content_indent));

    // --- Lambdas for drawing rows ---

    auto draw_row_background = [&](float row_left, float row_top,
        unsigned int sect_bg, unsigned int sect_accent)
    {
        ImVec2 sect_min = world_to_screen(row_left, row_top, ctx.offset_x, ctx.offset_y, ctx.zoom);
        ImVec2 sect_max = world_to_screen(content_right, row_top + ctx.f_row_height_effective,
            ctx.offset_x, ctx.offset_y, ctx.zoom);
        ctx.draw_list->AddRectFilled(sect_min, sect_max, sect_bg, ctx.section_rounding);
        ImVec2 accent_min = world_to_screen(row_left, row_top, ctx.offset_x, ctx.offset_y, ctx.zoom);
        ImVec2 accent_max = world_to_screen(row_left + ctx.f_accent_bar_width,
            row_top + ctx.f_row_height_effective, ctx.offset_x, ctx.offset_y, ctx.zoom);
        ctx.draw_list->AddRectFilled(accent_min, accent_max, sect_accent);
    };

    auto row_text_y = [&](float row_top) {
        return row_top + (ctx.f_row_height_effective - ctx.font_world_height) * 0.5f;
    };

    auto draw_row_text = [&](float row_top, unsigned int color, const char* text) {
        ctx.draw_list->AddText(ctx.font, ctx.scaled_font_size,
            world_to_screen(list_text_left, row_text_y(row_top), ctx.offset_x, ctx.offset_y, ctx.zoom),
            color, text);
    };

    auto draw_typed_row_text = [&](float row_top, const std::string& type_part,
        const std::string& name_part)
    {
        const std::string type_colon = type_part + ": ";
        ctx.draw_list->AddText(ctx.font, ctx.scaled_font_size,
            world_to_screen(list_text_left, row_text_y(row_top), ctx.offset_x, ctx.offset_y, ctx.zoom),
            ctx.type_muted_color, type_colon.c_str());
        const float type_w = ctx.font->CalcTextSizeA(ctx.scaled_font_size, FLT_MAX, 0.0f,
            type_colon.c_str(), nullptr).x / ctx.safe_zoom;
        ctx.draw_list->AddText(ctx.font, ctx.scaled_font_size,
            world_to_screen(list_text_left + type_w, row_text_y(row_top),
                ctx.offset_x, ctx.offset_y, ctx.zoom),
            ctx.text_color, name_part.c_str());
    };

    auto draw_property_row_text = [&](float row_top, const diagram_model::Property& p,
        float extra_indent)
    {
        const float text_x = list_text_left + extra_indent;
        const std::string type_colon = p.type + ": ";
        ctx.draw_list->AddText(ctx.font, ctx.scaled_font_size,
            world_to_screen(text_x, row_text_y(row_top), ctx.offset_x, ctx.offset_y, ctx.zoom),
            ctx.type_muted_color, type_colon.c_str());

        const float type_w = ctx.font->CalcTextSizeA(ctx.scaled_font_size, FLT_MAX, 0.0f,
            type_colon.c_str(), nullptr).x / ctx.safe_zoom;
        ctx.draw_list->AddText(ctx.font, ctx.scaled_font_size,
            world_to_screen(text_x + type_w, row_text_y(row_top),
                ctx.offset_x, ctx.offset_y, ctx.zoom),
            ctx.text_color, p.name.c_str());

        if (!p.default_value.empty()) {
            const float name_w = ctx.font->CalcTextSizeA(ctx.scaled_font_size, FLT_MAX, 0.0f,
                p.name.c_str(), nullptr).x / ctx.safe_zoom;
            const std::string default_text = " = " + p.default_value;
            ctx.draw_list->AddText(ctx.font, ctx.scaled_font_size,
                world_to_screen(text_x + type_w + name_w, row_text_y(row_top),
                    ctx.offset_x, ctx.offset_y, ctx.zoom),
                ctx.type_muted_color, default_text.c_str());
        }
    };

    // --- Group: Parent ---
    {
        const float row_top = cy;
        draw_row_background(content_x, row_top, ctx.parent_bg, ctx.parent_accent);
        draw_row_text(row_top, ctx.parent_header_color, "Parent:");
        cy += ctx.f_row_height_effective + ctx.f_row_inner_gap_effective;
    }
    if (!cls.parent_class_ids.empty()) {
        for (std::size_t pi = 0; pi < cls.parent_class_ids.size(); ++pi) {
            const float row_top = cy;
            draw_row_background(item_left, row_top, ctx.parent_bg, ctx.parent_accent);
            const diagram_model::DiagramClass* parent_cls = find_class(ctx.diagram, cls.parent_class_ids[pi]);
            const char* parent_name = parent_cls
                ? parent_cls->type_name.c_str() : cls.parent_class_ids[pi].c_str();
            draw_row_text(row_top, ctx.text_color, parent_name);

            // Nested expand button for parent.
            const std::string parent_key = path_prefix + "parent/" + std::to_string(pi);
            const bool parent_is_cycle = parent_cls && visited.find(parent_cls->id) != visited.end();
            const bool can_expand = parent_cls && !parent_is_cycle
                && depth + 1 < max_nesting_depth;
            if (can_expand) {
                const bool parent_expanded = is_nested_expanded(ctx.nested_expanded, parent_key);
                const float nbtn_x = content_right - ctx.f_nested_button_size;
                const float nbtn_y = row_top + (ctx.f_row_height_effective - ctx.f_nested_button_size) * 0.5f;
                draw_nested_button(ctx, nbtn_x, nbtn_y, parent_expanded);
                record_hit_button(ctx, block_class_id, parent_key, nbtn_x, nbtn_y);
                const float nav_x = nbtn_x - ctx.f_nav_button_gap - ctx.f_nav_button_size;
                const float nav_y = row_top + (ctx.f_row_height_effective - ctx.f_nav_button_size) * 0.5f;
                draw_nav_button(ctx, nav_x, nav_y);
                record_nav_button(ctx, parent_cls->id, nav_x, nav_y);
            } else if (parent_is_cycle) {
                const float cycle_x = content_right - ctx.font->CalcTextSizeA(
                    ctx.scaled_font_size, FLT_MAX, 0.0f, "(cycle)", nullptr).x / ctx.safe_zoom;
                ctx.draw_list->AddText(ctx.font, ctx.scaled_font_size,
                    world_to_screen(cycle_x, row_text_y(row_top),
                        ctx.offset_x, ctx.offset_y, ctx.zoom),
                    ctx.empty_color, "(cycle)");
            }
            if (parent_cls && !can_expand) {
                const float nav_x = content_right - ctx.f_nav_button_size;
                const float nav_y = row_top + (ctx.f_row_height_effective - ctx.f_nav_button_size) * 0.5f;
                draw_nav_button(ctx, nav_x, nav_y);
                record_nav_button(ctx, parent_cls->id, nav_x, nav_y);
            }
            if (parent_cls) {
                record_hover_region(ctx, parent_cls->id,
                    item_left, row_top, content_right - item_left, ctx.f_row_height_effective);
            }
            cy += ctx.f_row_height_effective;

            // If parent is expanded, render its content as a nested card.
            if (parent_cls && is_nested_expanded(ctx.nested_expanded, parent_key)
                && visited.find(parent_cls->id) == visited.end()
                && depth + 1 < max_nesting_depth)
            {
                visited.insert(parent_cls->id);
                cy += ctx.f_group_vertical_gap_effective;
                const float card_left = content_x;
                const float card_right = content_right;
                const float card_top = cy;
                cy += static_cast<float>(nested_header_height);
                cy += static_cast<float>(nested_card_content_inset_top);
                const float inner_left = card_left + static_cast<float>(nested_card_pad_x);
                const float inner_right = card_right - static_cast<float>(nested_card_pad_x);
                cy = render_class_content(ctx, *parent_cls, inner_left, inner_right,
                    cy, depth + 1, parent_key + "/", block_class_id, visited);
                cy += static_cast<float>(nested_card_content_inset_bottom);
                const NestedCardColors parent_colors {
                    ctx.parent_card_bg, ctx.parent_card_border,
                    ctx.parent_card_header_bg };
                draw_nested_card(ctx, card_left, card_top, card_right, cy,
                    parent_cls->type_name.c_str(), parent_colors);
                visited.erase(parent_cls->id);
            }

            if (pi + 1 < cls.parent_class_ids.size())
                cy += ctx.f_row_inner_gap_effective;
        }
    } else {
        const float row_top = cy;
        draw_row_background(item_left, row_top, ctx.parent_bg, ctx.parent_accent);
        draw_row_text(row_top, ctx.empty_color, "\xE2\x80\x94");
        cy += ctx.f_row_height_effective;
    }
    cy += ctx.f_group_vertical_gap_effective;

    // --- Group: Properties ---
    {
        const float row_top = cy;
        draw_row_background(content_x, row_top, ctx.properties_bg, ctx.properties_accent);
        draw_row_text(row_top, ctx.properties_header_color, "Properties:");
        cy += ctx.f_row_height_effective + ctx.f_row_inner_gap_effective;
    }
    if (!cls.properties.empty()) {
        for (size_t i = 0; i < cls.properties.size(); ++i) {
            const auto& p = cls.properties[i];
            const float row_top = cy;
            draw_row_background(item_left, row_top, ctx.properties_bg, ctx.properties_accent);
            draw_property_row_text(row_top, p, 0.0f);
            cy += ctx.f_row_height_effective;
            if (i + 1 < cls.properties.size())
                cy += ctx.f_row_inner_gap_effective;
        }
    } else {
        const float row_top = cy;
        draw_row_background(item_left, row_top, ctx.properties_bg, ctx.properties_accent);
        draw_row_text(row_top, ctx.empty_color, "\xE2\x80\x94");
        cy += ctx.f_row_height_effective;
    }
    cy += ctx.f_group_vertical_gap_effective;

    // --- Group: Components ---
    {
        const float row_top = cy;
        draw_row_background(content_x, row_top, ctx.components_bg, ctx.components_accent);
        draw_row_text(row_top, ctx.components_header_color, "Components:");
        cy += ctx.f_row_height_effective + ctx.f_row_inner_gap_effective;
    }
    if (!cls.components.empty()) {
        for (size_t i = 0; i < cls.components.size(); ++i) {
            const auto& comp = cls.components[i];
            const float row_top = cy;
            draw_row_background(item_left, row_top, ctx.components_bg, ctx.components_accent);
            draw_typed_row_text(row_top, comp.type, comp.name);
            cy += ctx.f_row_height_effective;

            if (!comp.properties.empty() || i + 1 < cls.components.size())
                cy += ctx.f_row_inner_gap_effective;

            const float sub_item_left = item_left + ctx.f_content_indent * 2.0f;
            const float sub_text_indent = ctx.f_content_indent * 2.0f;
            for (size_t j = 0; j < comp.properties.size(); ++j) {
                const auto& p = comp.properties[j];
                const float sub_row_top = cy;
                draw_row_background(sub_item_left, sub_row_top,
                    ctx.components_bg, ctx.components_accent);
                draw_property_row_text(sub_row_top, p, sub_text_indent);
                cy += ctx.f_row_height_effective;
                if (j + 1 < comp.properties.size() || i + 1 < cls.components.size())
                    cy += ctx.f_row_inner_gap_effective;
            }
        }
    } else {
        const float row_top = cy;
        draw_row_background(item_left, row_top, ctx.components_bg, ctx.components_accent);
        draw_row_text(row_top, ctx.empty_color, "\xE2\x80\x94");
        cy += ctx.f_row_height_effective;
    }
    cy += ctx.f_group_vertical_gap_effective;

    // --- Group: Children ---
    {
        const float row_top = cy;
        draw_row_background(content_x, row_top, ctx.children_bg, ctx.children_accent);
        draw_row_text(row_top, ctx.children_header_color, "Children:");
        cy += ctx.f_row_height_effective + ctx.f_row_inner_gap_effective;
    }
    if (!cls.child_objects.empty()) {
        for (size_t i = 0; i < cls.child_objects.size(); ++i) {
            const auto& co = cls.child_objects[i];
            const float row_top = cy;
            draw_row_background(item_left, row_top, ctx.children_bg, ctx.children_accent);
            const diagram_model::DiagramClass* child_class = find_class(ctx.diagram, co.class_id);
            const char* type_name = child_class
                ? child_class->type_name.c_str() : co.class_id.c_str();
            const std::string name_part = co.label.empty() ? std::string(type_name) : co.label;
            draw_typed_row_text(row_top, type_name, name_part);

            // Nested expand button for child object.
            const std::string child_key = path_prefix + "child/" + std::to_string(i);
            const bool child_is_cycle = child_class && visited.find(child_class->id) != visited.end();
            const bool can_expand = child_class && !child_is_cycle
                && depth + 1 < max_nesting_depth;
            if (can_expand) {
                const bool child_expanded = is_nested_expanded(ctx.nested_expanded, child_key);
                const float nbtn_x = content_right - ctx.f_nested_button_size;
                const float nbtn_y = row_top + (ctx.f_row_height_effective - ctx.f_nested_button_size) * 0.5f;
                draw_nested_button(ctx, nbtn_x, nbtn_y, child_expanded);
                record_hit_button(ctx, block_class_id, child_key, nbtn_x, nbtn_y);
                // Nav button to the left of expand button.
                const float nav_x = nbtn_x - ctx.f_nav_button_gap - ctx.f_nav_button_size;
                const float nav_y = row_top + (ctx.f_row_height_effective - ctx.f_nav_button_size) * 0.5f;
                draw_nav_button(ctx, nav_x, nav_y);
                record_nav_button(ctx, child_class->id, nav_x, nav_y);
            } else if (child_is_cycle) {
                const float cycle_x = content_right - ctx.font->CalcTextSizeA(
                    ctx.scaled_font_size, FLT_MAX, 0.0f, "(cycle)", nullptr).x / ctx.safe_zoom;
                ctx.draw_list->AddText(ctx.font, ctx.scaled_font_size,
                    world_to_screen(cycle_x, row_text_y(row_top),
                        ctx.offset_x, ctx.offset_y, ctx.zoom),
                    ctx.empty_color, "(cycle)");
            }
            // Nav button: show even when expand is unavailable, if class exists.
            if (child_class && !can_expand) {
                const float nav_x = content_right - ctx.f_nav_button_size;
                const float nav_y = row_top + (ctx.f_row_height_effective - ctx.f_nav_button_size) * 0.5f;
                draw_nav_button(ctx, nav_x, nav_y);
                record_nav_button(ctx, child_class->id, nav_x, nav_y);
            }
            // Hover region: entire child row maps to the child class.
            if (child_class) {
                record_hover_region(ctx, child_class->id,
                    item_left, row_top, content_right - item_left, ctx.f_row_height_effective);
            }

            cy += ctx.f_row_height_effective;

            // If child is expanded, render its content as a nested card.
            if (child_class && is_nested_expanded(ctx.nested_expanded, child_key)
                && visited.find(child_class->id) == visited.end()
                && depth + 1 < max_nesting_depth)
            {
                visited.insert(child_class->id);
                cy += ctx.f_group_vertical_gap_effective;
                // Card fills the current content area.
                const float card_left = content_x;
                const float card_right = content_right;
                const float card_top = cy;
                cy += static_cast<float>(nested_header_height);            // card header
                cy += static_cast<float>(nested_card_content_inset_top);   // top inset
                // Inner bounds: content area inside the card border.
                const float inner_left = card_left + static_cast<float>(nested_card_pad_x);
                const float inner_right = card_right - static_cast<float>(nested_card_pad_x);
                cy = render_class_content(ctx, *child_class, inner_left, inner_right,
                    cy, depth + 1, child_key + "/", block_class_id, visited);
                cy += static_cast<float>(nested_card_content_inset_bottom); // bottom inset
                // Draw the card around everything.
                const NestedCardColors child_colors {
                    ctx.child_card_bg, ctx.child_card_border,
                    ctx.child_card_header_bg };
                draw_nested_card(ctx, card_left, card_top, card_right, cy,
                    child_class->type_name.c_str(), child_colors);
                visited.erase(child_class->id);
            }

            if (i + 1 < cls.child_objects.size())
                cy += ctx.f_row_inner_gap_effective;
        }
    } else {
        const float row_top = cy;
        draw_row_background(item_left, row_top, ctx.children_bg, ctx.children_accent);
        draw_row_text(row_top, ctx.empty_color, "\xE2\x80\x94");
        cy += ctx.f_row_height_effective;
    }

    return cy;
}

} // namespace

void render_class_diagram(ImDrawList* draw_list,
    const diagram_model::ClassDiagram& diagram,
    const diagram_placement::PlacedClassDiagram& placed,
    float offset_x, float offset_y, float zoom,
    const std::unordered_map<std::string, bool>& nested_expanded,
    std::vector<NestedHitButton>* out_hit_buttons,
    std::vector<NavHitButton>* out_nav_buttons,
    std::vector<ClassHoverRegion>* out_hover_regions,
    const std::string& hovered_class_id,
    const std::vector<diagram_placement::ConnectionLine>& connection_lines)
{
    if (!draw_list) return;

    const unsigned int bg_color = IM_COL32(45, 45, 48, 255);
    const unsigned int border_color = IM_COL32(125, 125, 132, 255);
    const unsigned int text_color = IM_COL32(220, 220, 220, 255);
    const unsigned int header_bg = IM_COL32(38, 38, 42, 255);
    const unsigned int button_bg = IM_COL32(70, 70, 75, 255);
    const float line_thickness = 2.0f;
    const float block_rounding = 8.0f;

    const float f_padding = static_cast<float>(padding);
    const float f_button_size = static_cast<float>(button_size);
    const float f_header_height = static_cast<float>(header_height);
    const float f_row_height = static_cast<float>(row_height);
    const float f_content_inset_side = static_cast<float>(content_inset_side);
    const float f_content_inset_top = static_cast<float>(content_inset_top);
    const float f_accent_bar_width = static_cast<float>(accent_bar_width);
    const float f_content_indent = static_cast<float>(content_indent);
    const float f_group_vertical_gap = static_cast<float>(group_vertical_gap);
    const float f_row_inner_gap = static_cast<float>(row_inner_gap);
    const float f_header_content_gap = static_cast<float>(header_content_gap);

    const float safe_zoom = std::max(zoom, 1e-4f);
    ImFont* const font = ImGui::GetFont();
    const float scaled_font_size = ImGui::GetFontSize() * zoom;
    const float font_world_height = scaled_font_size / safe_zoom;
    const float f_row_height_effective = std::max(
        f_row_height,
        static_cast<float>(min_row_height_for_font(font_world_height)));
    const float row_gap_ratio = f_row_height > 0.0f ? (f_row_inner_gap / f_row_height) : 0.0f;
    const float group_gap_ratio = f_row_height > 0.0f ? (f_group_vertical_gap / f_row_height) : 0.0f;
    const float f_row_inner_gap_effective = f_row_height_effective * row_gap_ratio;
    const float f_group_vertical_gap_effective = f_row_height_effective * group_gap_ratio;

    // Build render context (shared state for recursive content rendering).
    RenderContext ctx {
        draw_list,
        diagram,
        offset_x,
        offset_y,
        zoom,
        safe_zoom,
        font,
        scaled_font_size,
        font_world_height,
        f_row_height_effective,
        f_row_inner_gap_effective,
        f_group_vertical_gap_effective,
        f_content_inset_side,
        f_accent_bar_width,
        f_content_indent,
        static_cast<float>(nested_button_size),
        static_cast<float>(nav_button_size),
        static_cast<float>(nav_button_gap),
        2.0f, // section_rounding
        text_color,
        IM_COL32(160, 160, 165, 255), // type_muted_color
        IM_COL32(120, 120, 125, 255), // empty_color
        button_bg,
        border_color,
        IM_COL32(42, 45, 52, 255),    // parent_bg
        IM_COL32(130, 140, 155, 255), // parent_header_color
        IM_COL32(70, 85, 110, 255),   // parent_accent
        IM_COL32(42, 50, 46, 255),    // properties_bg
        IM_COL32(130, 155, 145, 255), // properties_header_color
        IM_COL32(65, 95, 80, 255),    // properties_accent
        IM_COL32(52, 48, 42, 255),    // components_bg
        IM_COL32(155, 145, 130, 255), // components_header_color
        IM_COL32(110, 95, 70, 255),   // components_accent
        IM_COL32(48, 44, 52, 255),    // children_bg
        IM_COL32(145, 135, 155, 255), // children_header_color
        IM_COL32(95, 80, 110, 255),   // children_accent
        IM_COL32(35, 45, 62, 255),    // parent_card_bg (blue-tinted)
        IM_COL32(80, 110, 165, 255),  // parent_card_border (bright blue, fully opaque)
        IM_COL32(28, 36, 52, 255),    // parent_card_header_bg (darker blue)
        IM_COL32(50, 38, 62, 255),    // child_card_bg (purple-tinted)
        IM_COL32(120, 85, 165, 255),  // child_card_border (bright purple, fully opaque)
        IM_COL32(42, 30, 52, 255),    // child_card_header_bg (darker purple)
        IM_COL32(100, 180, 255, 255),  // nav_button_color (bright blue arrow)
        nested_expanded,
        out_hit_buttons,
        out_nav_buttons,
        out_hover_regions,
    };

    // ====== Permanent connection lines (behind blocks) ======
    {
        const unsigned int primary_inh_color = IM_COL32(100, 120, 150, 100);
        const unsigned int primary_inh_hover = IM_COL32(100, 120, 150, 220);
        const unsigned int composition_color = IM_COL32(130, 100, 150, 100);
        const unsigned int composition_hover = IM_COL32(130, 100, 150, 220);
        const float line_w = 1.5f;
        const float line_w_hover = 2.5f;
        const float marker_size = 6.0f * zoom;

        for (const auto& cl : connection_lines) {
            if (cl.kind == diagram_placement::ConnectionKind::SecondaryInheritance)
                continue; // Drawn later, on top of blocks.

            const bool is_hovered = (!hovered_class_id.empty()) &&
                (cl.from_class_id == hovered_class_id || cl.to_class_id == hovered_class_id);
            unsigned int color = (cl.kind == diagram_placement::ConnectionKind::PrimaryInheritance)
                ? (is_hovered ? primary_inh_hover : primary_inh_color)
                : (is_hovered ? composition_hover : composition_color);
            float thickness = is_hovered ? line_w_hover : line_w;

            // Draw line segments.
            for (std::size_t i = 0; i + 1 < cl.points.size(); ++i) {
                ImVec2 p0 = world_to_screen(static_cast<float>(cl.points[i].first),
                    static_cast<float>(cl.points[i].second), offset_x, offset_y, zoom);
                ImVec2 p1 = world_to_screen(static_cast<float>(cl.points[i + 1].first),
                    static_cast<float>(cl.points[i + 1].second), offset_x, offset_y, zoom);
                draw_list->AddLine(p0, p1, color, thickness);
            }

            // Marker at the "to" end (parent / target).
            if (cl.points.size() >= 2) {
                auto& last = cl.points.back();
                ImVec2 tip = world_to_screen(static_cast<float>(last.first),
                    static_cast<float>(last.second), offset_x, offset_y, zoom);
                auto& prev = cl.points[cl.points.size() - 2];
                float dx = static_cast<float>(last.first - prev.first);
                float dy = static_cast<float>(last.second - prev.second);
                float len = std::sqrt(dx * dx + dy * dy);
                if (len > 1e-4f) {
                    dx /= len; dy /= len;
                    float px = -dy, py = dx; // perpendicular
                    if (cl.kind == diagram_placement::ConnectionKind::PrimaryInheritance) {
                        // Empty triangle marker.
                        ImVec2 a = ImVec2(tip.x - dx * marker_size + px * marker_size * 0.5f,
                                          tip.y - dy * marker_size + py * marker_size * 0.5f);
                        ImVec2 b = ImVec2(tip.x - dx * marker_size - px * marker_size * 0.5f,
                                          tip.y - dy * marker_size - py * marker_size * 0.5f);
                        draw_list->AddTriangle(tip, a, b, color, thickness);
                    } else {
                        // Filled diamond marker for composition.
                        float hs = marker_size * 0.5f;
                        ImVec2 top_d = tip;
                        ImVec2 right_d = ImVec2(tip.x - dx * hs + px * hs * 0.5f,
                                                tip.y - dy * hs + py * hs * 0.5f);
                        ImVec2 bottom_d = ImVec2(tip.x - dx * marker_size,
                                                 tip.y - dy * marker_size);
                        ImVec2 left_d = ImVec2(tip.x - dx * hs - px * hs * 0.5f,
                                               tip.y - dy * hs - py * hs * 0.5f);
                        draw_list->AddQuadFilled(top_d, right_d, bottom_d, left_d, color);
                    }
                }
            }

            // Label for composition lines.
            if (cl.kind == diagram_placement::ConnectionKind::Composition && !cl.label.empty()
                && cl.points.size() >= 2)
            {
                std::size_t mid_idx = cl.points.size() / 2;
                float lx = static_cast<float>((cl.points[mid_idx - 1].first + cl.points[mid_idx].first) * 0.5);
                float ly = static_cast<float>((cl.points[mid_idx - 1].second + cl.points[mid_idx].second) * 0.5);
                ImFont* font = ImGui::GetFont();
                float fs = ImGui::GetFontSize() * zoom * 0.8f;
                ImVec2 label_pos = world_to_screen(lx, ly, offset_x, offset_y, zoom);
                label_pos.x += 3.0f;
                label_pos.y -= fs * 0.5f;
                draw_list->AddText(font, fs, label_pos, color, cl.label.c_str());
            }
        }
    }

    for (const auto& block : placed.blocks) {
        const diagram_model::DiagramClass* cl = find_class(diagram, block.class_id);
        if (!cl) continue;

        float x = (float)block.rect.x;
        float y = (float)block.rect.y;
        float w = (float)block.rect.width;
        float h = (float)block.rect.height;
        ImVec2 min_pt = world_to_screen(x, y, offset_x, offset_y, zoom);
        ImVec2 max_pt = world_to_screen(x + w, y + h, offset_x, offset_y, zoom);

        draw_list->AddRectFilled(min_pt, max_pt, bg_color, block_rounding);
        draw_list->AddRect(min_pt, max_pt, border_color, block_rounding, 0, line_thickness);

        draw_list->PushClipRect(min_pt, max_pt, true);

        // --- Header ---
        ImVec2 header_min = min_pt;
        ImVec2 header_max = world_to_screen(x + w, y + f_header_height, offset_x, offset_y, zoom);
        draw_list->AddRectFilled(header_min, header_max, header_bg,
            block_rounding, ImDrawFlags_RoundCornersTop);
        ImVec2 header_sep_left = world_to_screen(x, y + f_header_height, offset_x, offset_y, zoom);
        ImVec2 header_sep_right = world_to_screen(x + w, y + f_header_height, offset_x, offset_y, zoom);
        draw_list->AddLine(header_sep_left, header_sep_right, border_color, 1.0f);

        // Main expand/collapse button.
        float btn_x = x + w - f_padding - f_button_size;
        float btn_y = y + (f_header_height - f_button_size) * 0.5f;
        ImVec2 btn_min = world_to_screen(btn_x, btn_y, offset_x, offset_y, zoom);
        ImVec2 btn_max = world_to_screen(btn_x + f_button_size, btn_y + f_button_size, offset_x, offset_y, zoom);
        draw_list->AddRectFilled(btn_min, btn_max, button_bg);
        draw_list->AddRect(btn_min, btn_max, border_color, 0.0f, 0, 1.0f);

        float text_left = x + f_padding;
        float text_y = y + (f_header_height - scaled_font_size / zoom) * 0.5f;
        ImVec2 text_pos = world_to_screen(text_left, text_y, offset_x, offset_y, zoom);
        draw_list->AddText(font, scaled_font_size, text_pos, text_color, cl->type_name.c_str());

        ImVec2 plus_center = world_to_screen(btn_x + f_button_size * 0.5f,
            btn_y + f_button_size * 0.5f, offset_x, offset_y, zoom);
        float plus_half = 4.0f * zoom;
        if (block.expanded) {
            draw_list->AddLine(
                ImVec2(plus_center.x - plus_half, plus_center.y),
                ImVec2(plus_center.x + plus_half, plus_center.y), text_color, 1.5f);
        } else {
            draw_list->AddLine(
                ImVec2(plus_center.x - plus_half, plus_center.y),
                ImVec2(plus_center.x + plus_half, plus_center.y), text_color, 1.5f);
            draw_list->AddLine(
                ImVec2(plus_center.x, plus_center.y - plus_half),
                ImVec2(plus_center.x, plus_center.y + plus_half), text_color, 1.5f);
        }

        if (!block.expanded) {
            draw_list->PopClipRect();
            continue;
        }

        // --- Content (4 sections, recursively expandable) ---
        // Use draw list channels so nested frame backgrounds go behind content:
        //   channel 0 = frame backgrounds + accents
        //   channel 1 = row content + frame borders
        draw_list->ChannelsSplit(2);
        draw_list->ChannelsSetCurrent(1);

        float cy = y + f_header_height + static_cast<float>(content_inset_top) + f_header_content_gap;
        const std::string path_prefix = block.class_id + "/";
        std::unordered_set<std::string> visited;
        visited.insert(cl->id);
        const float area_left = x + f_content_inset_side;
        const float area_right = x + w - f_content_inset_side;
        render_class_content(ctx, *cl, area_left, area_right, cy, 0, path_prefix, block.class_id, visited);

        draw_list->ChannelsMerge();
        draw_list->PopClipRect();
    }

    // ====== Hover connection lines (SecondaryInheritance) — drawn over blocks ======
    if (!hovered_class_id.empty()) {
        const unsigned int sec_inh_color = IM_COL32(180, 140, 80, 180);
        const float sec_line_w = 2.0f;
        const float marker_size = 6.0f * zoom;
        const float dash_len = 8.0f * zoom;
        const float gap_len = 4.0f * zoom;

        for (const auto& cl : connection_lines) {
            if (cl.kind != diagram_placement::ConnectionKind::SecondaryInheritance) continue;
            if (cl.from_class_id != hovered_class_id) continue;

            // Draw dashed line segments.
            for (std::size_t i = 0; i + 1 < cl.points.size(); ++i) {
                ImVec2 p0 = world_to_screen(static_cast<float>(cl.points[i].first),
                    static_cast<float>(cl.points[i].second), offset_x, offset_y, zoom);
                ImVec2 p1 = world_to_screen(static_cast<float>(cl.points[i + 1].first),
                    static_cast<float>(cl.points[i + 1].second), offset_x, offset_y, zoom);
                // Draw dashed.
                float seg_dx = p1.x - p0.x;
                float seg_dy = p1.y - p0.y;
                float seg_len = std::sqrt(seg_dx * seg_dx + seg_dy * seg_dy);
                if (seg_len < 1e-3f) continue;
                float ndx = seg_dx / seg_len, ndy = seg_dy / seg_len;
                float drawn = 0.0f;
                bool drawing = true;
                while (drawn < seg_len) {
                    float step = drawing ? dash_len : gap_len;
                    float end = std::min(drawn + step, seg_len);
                    if (drawing) {
                        ImVec2 a = ImVec2(p0.x + ndx * drawn, p0.y + ndy * drawn);
                        ImVec2 b = ImVec2(p0.x + ndx * end, p0.y + ndy * end);
                        draw_list->AddLine(a, b, sec_inh_color, sec_line_w);
                    }
                    drawn = end;
                    drawing = !drawing;
                }
            }

            // Empty triangle marker at the "to" end.
            if (cl.points.size() >= 2) {
                auto& last = cl.points.back();
                ImVec2 tip = world_to_screen(static_cast<float>(last.first),
                    static_cast<float>(last.second), offset_x, offset_y, zoom);
                auto& prev = cl.points[cl.points.size() - 2];
                float dx = static_cast<float>(last.first - prev.first);
                float dy = static_cast<float>(last.second - prev.second);
                float len = std::sqrt(dx * dx + dy * dy);
                if (len > 1e-4f) {
                    dx /= len; dy /= len;
                    float px = -dy, py = dx;
                    ImVec2 a = ImVec2(tip.x - dx * marker_size + px * marker_size * 0.5f,
                                      tip.y - dy * marker_size + py * marker_size * 0.5f);
                    ImVec2 b = ImVec2(tip.x - dx * marker_size - px * marker_size * 0.5f,
                                      tip.y - dy * marker_size - py * marker_size * 0.5f);
                    draw_list->AddTriangle(tip, a, b, sec_inh_color, sec_line_w);
                }
            }
        }
    }

    // --- Hover highlight pass: draw a glow overlay on the hovered block ---
    if (!hovered_class_id.empty()) {
        for (const auto& block : placed.blocks) {
            if (block.class_id != hovered_class_id) continue;

            const float x = static_cast<float>(block.rect.x);
            const float y = static_cast<float>(block.rect.y);
            const float w = static_cast<float>(block.rect.width);
            const float h = static_cast<float>(block.rect.height);

            // Outer glow (slightly larger than block).
            const float glow_pad = 4.0f;
            ImVec2 glow_min = world_to_screen(x - glow_pad, y - glow_pad, offset_x, offset_y, zoom);
            ImVec2 glow_max = world_to_screen(x + w + glow_pad, y + h + glow_pad, offset_x, offset_y, zoom);
            const unsigned int glow_color = IM_COL32(100, 180, 255, 50);
            draw_list->AddRectFilled(glow_min, glow_max, glow_color, 12.0f);

            // Inner highlight overlay on the block itself.
            ImVec2 min_pt = world_to_screen(x, y, offset_x, offset_y, zoom);
            ImVec2 max_pt = world_to_screen(x + w, y + h, offset_x, offset_y, zoom);
            const unsigned int highlight_fill = IM_COL32(100, 180, 255, 25);
            const unsigned int highlight_border = IM_COL32(100, 180, 255, 160);
            draw_list->AddRectFilled(min_pt, max_pt, highlight_fill, 8.0f);
            draw_list->AddRect(min_pt, max_pt, highlight_border, 8.0f, 0, 2.5f);
            break;
        }
    }
}

} // namespace diagram_render
