#include <diagram_render/renderer.hpp>
#include <diagram_placement/class_diagram_layout_constants.hpp>
#include <diagram_placement/class_diagram_placement.hpp>
#include <diagram_model/class_diagram.hpp>
#include "imgui.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <unordered_map>

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

} // namespace

void render_class_diagram(ImDrawList* draw_list,
    const diagram_model::ClassDiagram& diagram,
    const diagram_placement::PlacedClassDiagram& placed,
    float offset_x, float offset_y, float zoom)
{
    if (!draw_list) return;

    std::unordered_map<std::string, const diagram_model::DiagramClass*> by_id;
    for (const auto& c : diagram.classes)
        by_id[c.id] = &c;

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

    const float section_rounding = 2.0f;

    const unsigned int parent_bg = IM_COL32(42, 45, 52, 255);
    const unsigned int parent_header = IM_COL32(130, 140, 155, 255);
    const unsigned int parent_accent = IM_COL32(70, 85, 110, 255);

    const unsigned int properties_bg = IM_COL32(42, 50, 46, 255);
    const unsigned int properties_header = IM_COL32(130, 155, 145, 255);
    const unsigned int properties_accent = IM_COL32(65, 95, 80, 255);

    const unsigned int components_bg = IM_COL32(52, 48, 42, 255);
    const unsigned int components_header = IM_COL32(155, 145, 130, 255);
    const unsigned int components_accent = IM_COL32(110, 95, 70, 255);

    const unsigned int children_bg = IM_COL32(48, 44, 52, 255);
    const unsigned int children_header = IM_COL32(145, 135, 155, 255);
    const unsigned int children_accent = IM_COL32(95, 80, 110, 255);

    const unsigned int type_muted_color = IM_COL32(160, 160, 165, 255);
    const unsigned int empty_color = IM_COL32(120, 120, 125, 255);

    ImFont* const font = ImGui::GetFont();
    const float scaled_font_size = ImGui::GetFontSize() * zoom;

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

        ImVec2 header_min = min_pt;
        ImVec2 header_max = world_to_screen(x + w, y + f_header_height, offset_x, offset_y, zoom);
        draw_list->AddRectFilled(header_min, header_max, header_bg, block_rounding, ImDrawFlags_RoundCornersTop);
        ImVec2 header_sep_left = world_to_screen(x, y + f_header_height, offset_x, offset_y, zoom);
        ImVec2 header_sep_right = world_to_screen(x + w, y + f_header_height, offset_x, offset_y, zoom);
        draw_list->AddLine(header_sep_left, header_sep_right, border_color, 1.0f);

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

        ImVec2 plus_center = world_to_screen(btn_x + f_button_size * 0.5f, btn_y + f_button_size * 0.5f, offset_x, offset_y, zoom);
        float plus_half = 4.0f * zoom;
        if (block.expanded) {
            draw_list->AddLine(ImVec2(plus_center.x - plus_half, plus_center.y), ImVec2(plus_center.x + plus_half, plus_center.y), text_color, 1.5f);
        } else {
            draw_list->AddLine(ImVec2(plus_center.x - plus_half, plus_center.y), ImVec2(plus_center.x + plus_half, plus_center.y), text_color, 1.5f);
            draw_list->AddLine(ImVec2(plus_center.x, plus_center.y - plus_half), ImVec2(plus_center.x, plus_center.y + plus_half), text_color, 1.5f);
        }

        if (!block.expanded) {
            draw_list->PopClipRect();
            continue;
        }

        float content_x = x + f_content_inset_side;
        float content_w = w - 2.0f * f_content_inset_side;
        const float list_text_left = content_x + static_cast<float>(content_left_offset());
        // Keep item background shifted, but ensure it starts before text.
        const float item_left = std::max(content_x, list_text_left - (f_accent_bar_width + f_content_indent));

        const float safe_zoom = std::max(zoom, 1e-4f);
        const float font_world_height = scaled_font_size / safe_zoom;
        const float f_row_height_effective = std::max(
            f_row_height,
            static_cast<float>(min_row_height_for_font(font_world_height)));
        const float row_gap_ratio = f_row_height > 0.0f ? (f_row_inner_gap / f_row_height) : 0.0f;
        const float group_gap_ratio = f_row_height > 0.0f ? (f_group_vertical_gap / f_row_height) : 0.0f;
        const float f_row_inner_gap_effective = f_row_height_effective * row_gap_ratio;
        const float f_group_vertical_gap_effective = f_row_height_effective * group_gap_ratio;

        float cy = y + f_header_height + f_content_inset_top + f_header_content_gap;

        auto draw_row_background = [&](float row_left, float row_top, unsigned int sect_bg, unsigned int sect_accent) {
            ImVec2 sect_min = world_to_screen(row_left, row_top, offset_x, offset_y, zoom);
            ImVec2 sect_max = world_to_screen(content_x + content_w, row_top + f_row_height_effective, offset_x, offset_y, zoom);
            draw_list->AddRectFilled(sect_min, sect_max, sect_bg, section_rounding);
            ImVec2 accent_min = world_to_screen(row_left, row_top, offset_x, offset_y, zoom);
            ImVec2 accent_max = world_to_screen(row_left + f_accent_bar_width, row_top + f_row_height_effective, offset_x, offset_y, zoom);
            draw_list->AddRectFilled(accent_min, accent_max, sect_accent);
        };

        auto row_text_y = [&](float row_top) {
            return row_top + (f_row_height_effective - font_world_height) * 0.5f;
        };

        auto draw_row_text = [&](float row_top, unsigned int color, const char* text) {
            draw_list->AddText(font, scaled_font_size,
                world_to_screen(list_text_left, row_text_y(row_top), offset_x, offset_y, zoom),
                color, text);
        };

        auto draw_typed_row_text = [&](float row_top, const std::string& type_part, const std::string& name_part) {
            const std::string type_colon = type_part + ": ";
            draw_list->AddText(font, scaled_font_size,
                world_to_screen(list_text_left, row_text_y(row_top), offset_x, offset_y, zoom),
                type_muted_color, type_colon.c_str());
            const float type_w = font->CalcTextSizeA(scaled_font_size, FLT_MAX, 0.0f, type_colon.c_str(), nullptr).x / safe_zoom;
            draw_list->AddText(font, scaled_font_size,
                world_to_screen(list_text_left + type_w, row_text_y(row_top), offset_x, offset_y, zoom),
                text_color, name_part.c_str());
        };

        // Group Parent
        {
            const float row_top = cy;
            draw_row_background(content_x, row_top, parent_bg, parent_accent);
            draw_row_text(row_top, parent_header, "Parent:");
            cy += f_row_height_effective + f_row_inner_gap_effective;
        }
        {
            const float row_top = cy;
            draw_row_background(item_left, row_top, parent_bg, parent_accent);
            if (!cl->parent_class_id.empty()) {
                const diagram_model::DiagramClass* parent = find_class(diagram, cl->parent_class_id);
                const char* parent_name = parent ? parent->type_name.c_str() : cl->parent_class_id.c_str();
                draw_row_text(row_top, text_color, parent_name);
            } else {
                draw_row_text(row_top, empty_color, "\u2014");
            }
            cy += f_row_height_effective;
        }
        cy += f_group_vertical_gap_effective;

        // Group Properties
        {
            const float row_top = cy;
            draw_row_background(content_x, row_top, properties_bg, properties_accent);
            draw_row_text(row_top, properties_header, "Properties:");
            cy += f_row_height_effective + f_row_inner_gap_effective;
        }
        if (!cl->properties.empty()) {
            for (size_t i = 0; i < cl->properties.size(); ++i) {
                const auto& p = cl->properties[i];
                const float row_top = cy;
                draw_row_background(item_left, row_top, properties_bg, properties_accent);
                draw_typed_row_text(row_top, p.type, p.name);
                cy += f_row_height_effective;
                if (i + 1 < cl->properties.size())
                    cy += f_row_inner_gap_effective;
            }
        } else {
            const float row_top = cy;
            draw_row_background(item_left, row_top, properties_bg, properties_accent);
            draw_row_text(row_top, empty_color, "\u2014");
            cy += f_row_height_effective;
        }
        cy += f_group_vertical_gap_effective;

        // Group Components
        {
            const float row_top = cy;
            draw_row_background(content_x, row_top, components_bg, components_accent);
            draw_row_text(row_top, components_header, "Components:");
            cy += f_row_height_effective + f_row_inner_gap_effective;
        }
        if (!cl->components.empty()) {
            for (size_t i = 0; i < cl->components.size(); ++i) {
                const auto& comp = cl->components[i];
                const float row_top = cy;
                draw_row_background(item_left, row_top, components_bg, components_accent);
                draw_typed_row_text(row_top, comp.type, comp.name);
                cy += f_row_height_effective;
                if (i + 1 < cl->components.size())
                    cy += f_row_inner_gap_effective;
            }
        } else {
            const float row_top = cy;
            draw_row_background(item_left, row_top, components_bg, components_accent);
            draw_row_text(row_top, empty_color, "\u2014");
            cy += f_row_height_effective;
        }
        cy += f_group_vertical_gap_effective;

        // Group Children
        {
            const float row_top = cy;
            draw_row_background(content_x, row_top, children_bg, children_accent);
            draw_row_text(row_top, children_header, "Children:");
            cy += f_row_height_effective + f_row_inner_gap_effective;
        }
        if (!cl->child_objects.empty()) {
            for (size_t i = 0; i < cl->child_objects.size(); ++i) {
                const auto& co = cl->child_objects[i];
                const float row_top = cy;
                draw_row_background(item_left, row_top, children_bg, children_accent);
                const diagram_model::DiagramClass* child_class = find_class(diagram, co.class_id);
                const char* type_name = child_class ? child_class->type_name.c_str() : co.class_id.c_str();
                const std::string name_part = co.label.empty() ? std::string(type_name) : co.label;
                draw_typed_row_text(row_top, type_name, name_part);
                cy += f_row_height_effective;
                if (i + 1 < cl->child_objects.size())
                    cy += f_row_inner_gap_effective;
            }
        } else {
            const float row_top = cy;
            draw_row_background(item_left, row_top, children_bg, children_accent);
            draw_row_text(row_top, empty_color, "\u2014");
            cy += f_row_height_effective;
        }

        draw_list->PopClipRect();
    }
}

} // namespace diagram_render
