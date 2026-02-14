#include <diagram_render/renderer.hpp>
#include <diagram_placement/class_diagram_placement.hpp>
#include <diagram_model/class_diagram.hpp>
#include "imgui.h"
#include <algorithm>
#include <cmath>
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
    const unsigned int border_color = IM_COL32(100, 100, 105, 255);
    const unsigned int text_color = IM_COL32(220, 220, 220, 255);
    const unsigned int section_color = IM_COL32(160, 160, 165, 255);
    const unsigned int button_bg = IM_COL32(70, 70, 75, 255);
    const unsigned int separator_color = IM_COL32(80, 80, 85, 255);
    const float line_thickness = 1.5f;

    for (const auto& block : placed.blocks) {
        const diagram_model::DiagramClass* cl = find_class(diagram, block.class_id);
        if (!cl) continue;

        float x = (float)block.rect.x;
        float y = (float)block.rect.y;
        float w = (float)block.rect.width;
        float h = (float)block.rect.height;
        ImVec2 min_pt = world_to_screen(x, y, offset_x, offset_y, zoom);
        ImVec2 max_pt = world_to_screen(x + w, y + h, offset_x, offset_y, zoom);

        draw_list->AddRectFilled(min_pt, max_pt, bg_color);
        draw_list->AddRect(min_pt, max_pt, border_color, 0.0f, 0, line_thickness);

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

        if (!block.expanded) continue;

        float cy = y + header_height;
        ImVec2 sep_left = world_to_screen(x, cy, offset_x, offset_y, zoom);
        ImVec2 sep_right = world_to_screen(x + w, cy, offset_x, offset_y, zoom);
        draw_list->AddLine(sep_left, sep_right, separator_color, 1.0f);
        cy += 4.0f;

        if (!cl->parent_class_id.empty()) {
            const diagram_model::DiagramClass* parent = find_class(diagram, cl->parent_class_id);
            const char* parent_name = parent ? parent->type_name.c_str() : cl->parent_class_id.c_str();
            ImVec2 sect_pos = world_to_screen(x + padding, cy, offset_x, offset_y, zoom);
            draw_list->AddText(sect_pos, section_color, "Parent:");
            cy += section_header_height;
            draw_list->AddText(world_to_screen(x + padding, cy, offset_x, offset_y, zoom), text_color, parent_name);
            cy += row_height + 4.0f;
        }

        if (!cl->properties.empty()) {
            ImVec2 sect_pos = world_to_screen(x + padding, cy, offset_x, offset_y, zoom);
            draw_list->AddText(sect_pos, section_color, "Properties:");
            cy += section_header_height;
            for (const auto& p : cl->properties) {
                std::string line = p.name + ": " + p.type;
                draw_list->AddText(world_to_screen(x + padding, cy, offset_x, offset_y, zoom), text_color, line.c_str());
                cy += row_height;
            }
            cy += 4.0f;
        }

        if (!cl->components.empty()) {
            ImVec2 sect_pos = world_to_screen(x + padding, cy, offset_x, offset_y, zoom);
            draw_list->AddText(sect_pos, section_color, "Components:");
            cy += section_header_height;
            for (const auto& comp : cl->components) {
                std::string line = comp.name + ": " + comp.type;
                draw_list->AddText(world_to_screen(x + padding, cy, offset_x, offset_y, zoom), text_color, line.c_str());
                cy += row_height;
            }
            cy += 4.0f;
        }

        if (!cl->child_objects.empty()) {
            ImVec2 sect_pos = world_to_screen(x + padding, cy, offset_x, offset_y, zoom);
            draw_list->AddText(sect_pos, section_color, "Children:");
            cy += section_header_height;
            for (const auto& co : cl->child_objects) {
                const diagram_model::DiagramClass* child_class = find_class(diagram, co.class_id);
                const char* type_name = child_class ? child_class->type_name.c_str() : co.class_id.c_str();
                std::string line = co.label.empty() ? std::string(type_name) : (co.label + " (" + type_name + ")");
                draw_list->AddText(world_to_screen(x + padding, cy, offset_x, offset_y, zoom), text_color, line.c_str());
                cy += row_height;
            }
        }
    }
}

} // namespace diagram_render
