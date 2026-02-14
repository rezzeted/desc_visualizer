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
    float f_nesting_indent;
    float f_nested_button_size;
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
    // Nested state.
    const std::unordered_map<std::string, bool>& nested_expanded;
    std::vector<NestedHitButton>* out_hit_buttons;
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

// Recursively render the 4 content sections (Parent, Properties, Components, Children)
// of a class inside a block card.
// block_x, block_w: the top-level block's x position and width (used to compute content area).
// cy: current y position (world); returns the new cy after rendering.
// depth: nesting depth (0 = direct content of the block).
// path_prefix: tree path prefix, e.g. "Player/" or "Player/parent/".
// block_class_id: the top-level block's class id (for hit button recording).
// visited: set of class ids already on the current expansion path (cycle guard).
float render_class_content(
    const RenderContext& ctx,
    const diagram_model::DiagramClass& cls,
    float block_x, float block_w,
    float cy,
    int depth,
    const std::string& path_prefix,
    const std::string& block_class_id,
    std::unordered_set<std::string>& visited)
{
    const float depth_offset = ctx.f_nesting_indent * static_cast<float>(depth);
    const float content_x = block_x + ctx.f_content_inset_side + depth_offset;
    const float content_right = block_x + block_w - ctx.f_content_inset_side;
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
    {
        const float row_top = cy;
        draw_row_background(item_left, row_top, ctx.parent_bg, ctx.parent_accent);
        if (!cls.parent_class_id.empty()) {
            const diagram_model::DiagramClass* parent_cls = find_class(ctx.diagram, cls.parent_class_id);
            const char* parent_name = parent_cls
                ? parent_cls->type_name.c_str() : cls.parent_class_id.c_str();
            draw_row_text(row_top, ctx.text_color, parent_name);

            // Nested expand button for parent (only if the parent class exists in diagram).
            const std::string parent_key = path_prefix + "parent";
            const bool parent_is_cycle = parent_cls && visited.find(parent_cls->id) != visited.end();
            const bool can_expand = parent_cls && !parent_is_cycle
                && depth + 1 < max_nesting_depth;
            if (can_expand) {
                const bool parent_expanded = is_nested_expanded(ctx.nested_expanded, parent_key);
                const float nbtn_x = content_right - ctx.f_nested_button_size;
                const float nbtn_y = row_top + (ctx.f_row_height_effective - ctx.f_nested_button_size) * 0.5f;
                draw_nested_button(ctx, nbtn_x, nbtn_y, parent_expanded);
                record_hit_button(ctx, block_class_id, parent_key, nbtn_x, nbtn_y);
            } else if (parent_is_cycle) {
                // Show cycle indicator.
                const float cycle_x = content_right - ctx.font->CalcTextSizeA(
                    ctx.scaled_font_size, FLT_MAX, 0.0f, "(cycle)", nullptr).x / ctx.safe_zoom;
                ctx.draw_list->AddText(ctx.font, ctx.scaled_font_size,
                    world_to_screen(cycle_x, row_text_y(row_top),
                        ctx.offset_x, ctx.offset_y, ctx.zoom),
                    ctx.empty_color, "(cycle)");
            }
        } else {
            draw_row_text(row_top, ctx.empty_color, "\xE2\x80\x94");
        }
        cy += ctx.f_row_height_effective;

        // If parent is expanded, render its content recursively.
        if (!cls.parent_class_id.empty()) {
            const diagram_model::DiagramClass* parent_cls = find_class(ctx.diagram, cls.parent_class_id);
            const std::string parent_key = path_prefix + "parent";
            if (parent_cls && is_nested_expanded(ctx.nested_expanded, parent_key)
                && visited.find(parent_cls->id) == visited.end()
                && depth + 1 < max_nesting_depth)
            {
                visited.insert(parent_cls->id);
                cy += ctx.f_group_vertical_gap_effective;
                cy = render_class_content(ctx, *parent_cls, block_x, block_w,
                    cy, depth + 1, parent_key + "/", block_class_id, visited);
                visited.erase(parent_cls->id);
            }
        }
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
            } else if (child_is_cycle) {
                const float cycle_x = content_right - ctx.font->CalcTextSizeA(
                    ctx.scaled_font_size, FLT_MAX, 0.0f, "(cycle)", nullptr).x / ctx.safe_zoom;
                ctx.draw_list->AddText(ctx.font, ctx.scaled_font_size,
                    world_to_screen(cycle_x, row_text_y(row_top),
                        ctx.offset_x, ctx.offset_y, ctx.zoom),
                    ctx.empty_color, "(cycle)");
            }

            cy += ctx.f_row_height_effective;

            // If child is expanded, render its content recursively.
            if (child_class && is_nested_expanded(ctx.nested_expanded, child_key)
                && visited.find(child_class->id) == visited.end()
                && depth + 1 < max_nesting_depth)
            {
                visited.insert(child_class->id);
                cy += ctx.f_group_vertical_gap_effective;
                cy = render_class_content(ctx, *child_class, block_x, block_w,
                    cy, depth + 1, child_key + "/", block_class_id, visited);
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
    std::vector<NestedHitButton>* out_hit_buttons)
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
        static_cast<float>(nesting_indent),
        static_cast<float>(nested_button_size),
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
        nested_expanded,
        out_hit_buttons,
    };

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
        float cy = y + f_header_height + static_cast<float>(content_inset_top) + f_header_content_gap;
        const std::string path_prefix = block.class_id + "/";
        std::unordered_set<std::string> visited;
        visited.insert(cl->id);
        render_class_content(ctx, *cl, x, w, cy, 0, path_prefix, block.class_id, visited);

        draw_list->PopClipRect();
    }
}

} // namespace diagram_render
