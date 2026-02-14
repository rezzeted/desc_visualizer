#include <diagram_render/renderer.hpp>
#include <diagram_placement/class_diagram_placement.hpp>
#include <diagram_model/class_diagram.hpp>
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_map>

namespace diagram_render {

namespace {

const float button_size = 20.0f;
const float padding = 8.0f;
const float header_height = 28.0f;
const float section_header_height = 20.0f;
const float row_height = 18.0f;

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
    const unsigned int separator_color = IM_COL32(80, 80, 85, 255);
    const float line_thickness = 2.0f;
    const float block_rounding = 8.0f;
    const float content_inset_bottom = 10.0f;
    const float content_inset_side = 6.0f;
    const float content_inset_top = 6.0f;

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

    const float accent_bar_width = 3.0f;
    const float content_indent = 4.0f;
    const float section_gap = 2.0f;
    const float group_gap = 6.0f;
    const float list_marker_radius = 2.0f;
    const float section_rounding = 2.0f;
    const unsigned int type_muted_color = IM_COL32(160, 160, 165, 255);
    const unsigned int empty_color = IM_COL32(120, 120, 125, 255);

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
        ImVec2 header_max = world_to_screen(x + w, y + header_height, offset_x, offset_y, zoom);
        draw_list->AddRectFilled(header_min, header_max, header_bg, block_rounding, ImDrawFlags_RoundCornersTop);
        ImVec2 header_sep_left = world_to_screen(x, y + header_height, offset_x, offset_y, zoom);
        ImVec2 header_sep_right = world_to_screen(x + w, y + header_height, offset_x, offset_y, zoom);
        draw_list->AddLine(header_sep_left, header_sep_right, border_color, 1.0f);

        float btn_x = x + w - padding - button_size;
        float btn_y = y + (header_height - button_size) * 0.5f;
        ImVec2 btn_min = world_to_screen(btn_x, btn_y, offset_x, offset_y, zoom);
        ImVec2 btn_max = world_to_screen(btn_x + button_size, btn_y + button_size, offset_x, offset_y, zoom);
        draw_list->AddRectFilled(btn_min, btn_max, button_bg);
        draw_list->AddRect(btn_min, btn_max, border_color, 0.0f, 0, 1.0f);

        float text_left = x + padding;
        float text_y = y + (header_height - ImGui::GetTextLineHeight()) * 0.5f;
        ImVec2 text_pos = world_to_screen(text_left, text_y, offset_x, offset_y, zoom);
        draw_list->AddText(text_pos, text_color, cl->type_name.c_str());

        ImVec2 plus_center = world_to_screen(btn_x + button_size * 0.5f, btn_y + button_size * 0.5f, offset_x, offset_y, zoom);
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

        float content_x = x + content_inset_side;
        float content_w = w - 2.0f * content_inset_side;
        float content_left = content_x + padding + accent_bar_width + content_indent;
        float row_text_left = content_left + list_marker_radius * 2.0f + 4.0f;

        float cy = y + header_height + content_inset_top + 4.0f;

        auto draw_section = [&](const char* title, unsigned int sect_bg, unsigned int sect_header, unsigned int sect_accent,
            size_t row_count, bool draw_top_separator, const std::function<void(float& row_y)>& draw_rows) {
            if (draw_top_separator) {
                ImVec2 line_min = world_to_screen(content_x, cy, offset_x, offset_y, zoom);
                ImVec2 line_max = world_to_screen(content_x + content_w, cy, offset_x, offset_y, zoom);
                draw_list->AddLine(line_min, line_max, separator_color, 1.0f);
            }
            float sect_top = cy;
            cy += section_header_height;
            float content_height = row_count > 0 ? row_count * row_height : row_height;
            cy += content_height + section_gap;
            float sect_bottom = cy - section_gap;
            ImVec2 sect_min = world_to_screen(content_x, sect_top, offset_x, offset_y, zoom);
            ImVec2 sect_max = world_to_screen(content_x + content_w, sect_bottom, offset_x, offset_y, zoom);
            draw_list->AddRectFilled(sect_min, sect_max, sect_bg, section_rounding);
            ImVec2 accent_min = world_to_screen(content_x, sect_top, offset_x, offset_y, zoom);
            ImVec2 accent_max = world_to_screen(content_x + accent_bar_width, sect_bottom, offset_x, offset_y, zoom);
            draw_list->AddRectFilled(accent_min, accent_max, sect_accent);
            ImVec2 title_pt = world_to_screen(content_left, sect_top, offset_x, offset_y, zoom);
            draw_list->AddText(title_pt, sect_header, title);
            float row_y = sect_top + section_header_height;
            draw_rows(row_y);
        };

        auto draw_list_marker = [&](float row_y, unsigned int color) {
            float mx = content_left + list_marker_radius;
            float my = row_y + row_height * 0.5f;
            ImVec2 center = world_to_screen(mx, my, offset_x, offset_y, zoom);
            draw_list->AddCircleFilled(center, list_marker_radius * zoom, color);
        };

        draw_section("Parent:", parent_bg, parent_header, parent_accent,
            cl->parent_class_id.empty() ? 0 : 1, false,
            [&](float& row_y) {
                draw_list_marker(row_y, parent_accent);
                if (!cl->parent_class_id.empty()) {
                    const diagram_model::DiagramClass* parent = find_class(diagram, cl->parent_class_id);
                    const char* parent_name = parent ? parent->type_name.c_str() : cl->parent_class_id.c_str();
                    draw_list->AddText(world_to_screen(row_text_left, row_y, offset_x, offset_y, zoom), text_color, parent_name);
                } else {
                    draw_list->AddText(world_to_screen(row_text_left, row_y, offset_x, offset_y, zoom), empty_color, "\u2014");
                }
            });
        cy += group_gap;

        draw_section("Properties:", properties_bg, properties_header, properties_accent,
            cl->properties.empty() ? 0 : cl->properties.size(), true,
            [&](float& row_y) {
                if (!cl->properties.empty()) {
                    for (const auto& p : cl->properties) {
                        draw_list_marker(row_y, properties_accent);
                        ImVec2 name_pos = world_to_screen(row_text_left, row_y, offset_x, offset_y, zoom);
                        draw_list->AddText(name_pos, text_color, (p.name + ": ").c_str());
                        float name_w = ImGui::CalcTextSize((p.name + ": ").c_str()).x;
                        draw_list->AddText(world_to_screen(row_text_left + name_w / zoom, row_y, offset_x, offset_y, zoom), type_muted_color, p.type.c_str());
                        row_y += row_height;
                    }
                } else {
                    draw_list_marker(row_y, properties_accent);
                    draw_list->AddText(world_to_screen(row_text_left, row_y, offset_x, offset_y, zoom), empty_color, "\u2014");
                }
            });
        cy += group_gap;

        draw_section("Components:", components_bg, components_header, components_accent,
            cl->components.empty() ? 0 : cl->components.size(), true,
            [&](float& row_y) {
                if (!cl->components.empty()) {
                    for (const auto& comp : cl->components) {
                        draw_list_marker(row_y, components_accent);
                        ImVec2 name_pos = world_to_screen(row_text_left, row_y, offset_x, offset_y, zoom);
                        draw_list->AddText(name_pos, text_color, (comp.name + ": ").c_str());
                        float name_w = ImGui::CalcTextSize((comp.name + ": ").c_str()).x;
                        draw_list->AddText(world_to_screen(row_text_left + name_w / zoom, row_y, offset_x, offset_y, zoom), type_muted_color, comp.type.c_str());
                        row_y += row_height;
                    }
                } else {
                    draw_list_marker(row_y, components_accent);
                    draw_list->AddText(world_to_screen(row_text_left, row_y, offset_x, offset_y, zoom), empty_color, "\u2014");
                }
            });

        draw_section("Children:", children_bg, children_header, children_accent,
            cl->child_objects.empty() ? 0 : cl->child_objects.size(), true,
            [&](float& row_y) {
                if (!cl->child_objects.empty()) {
                    for (const auto& co : cl->child_objects) {
                        draw_list_marker(row_y, children_accent);
                        const diagram_model::DiagramClass* child_class = find_class(diagram, co.class_id);
                        const char* type_name = child_class ? child_class->type_name.c_str() : co.class_id.c_str();
                        if (co.label.empty()) {
                            draw_list->AddText(world_to_screen(row_text_left, row_y, offset_x, offset_y, zoom), text_color, type_name);
                        } else {
                            std::string label_part = co.label + " (";
                            std::string type_part = std::string(type_name) + ")";
                            draw_list->AddText(world_to_screen(row_text_left, row_y, offset_x, offset_y, zoom), text_color, label_part.c_str());
                            float label_w = ImGui::CalcTextSize(label_part.c_str()).x;
                            draw_list->AddText(world_to_screen(row_text_left + label_w / zoom, row_y, offset_x, offset_y, zoom), type_muted_color, type_part.c_str());
                        }
                        row_y += row_height;
                    }
                } else {
                    draw_list_marker(row_y, children_accent);
                    draw_list->AddText(world_to_screen(row_text_left, row_y, offset_x, offset_y, zoom), empty_color, "\u2014");
                }
            });

        draw_list->PopClipRect();
    }
}

} // namespace diagram_render
