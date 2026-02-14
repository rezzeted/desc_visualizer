#include <diagram_render/renderer.hpp>
#include <diagram_placement/class_diagram_layout_constants.hpp>
#include <diagram_placement/class_diagram_placement.hpp>
#include <diagram_model/class_diagram.hpp>
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace diagram_render {

namespace {

using namespace diagram_placement::layout;

const diagram_model::DiagramClass* find_class(const diagram_model::ClassDiagram& diagram, const std::string& id) {
    for (const auto& c : diagram.classes)
        if (c.id == id) return &c;
    return nullptr;
}

// Measure text width in world units (ImGui returns pixels; at zoom 1 we treat 1 pixel = 1 world unit).
double measure_text_width(const char* text) {
    if (!text || !text[0]) return 0.0;
    return static_cast<double>(ImGui::CalcTextSize(text).x);
}

std::string format_typed_name_with_default(const std::string& type,
    const std::string& name,
    const std::string& default_value)
{
    std::string line = type + ": " + name;
    if (!default_value.empty())
        line += " = " + default_value;
    return line;
}

} // namespace

std::unordered_map<std::string, diagram_placement::Rect> compute_class_block_sizes(
    const diagram_model::ClassDiagram& diagram,
    const std::unordered_map<std::string, bool>& expanded)
{
    std::unordered_map<std::string, diagram_placement::Rect> out;
    const double font_world_height = static_cast<double>(ImGui::GetFontSize());
    const double effective_row_height = std::max(row_height, min_row_height_for_font(font_world_height));
    const double component_subproperty_indent = content_indent * 2.0;
    for (const auto& c : diagram.classes) {
        diagram_placement::Rect r;
        r.x = 0;
        r.y = 0;
        bool is_expanded = false;
        auto it = expanded.find(c.id);
        if (it != expanded.end()) is_expanded = it->second;

        if (!is_expanded) {
            r.width = collapsed_width;
            r.height = collapsed_height;
            out[c.id] = r;
            continue;
        }

        double max_text_w = 0.0;
        double header_text_w = measure_text_width(c.type_name.c_str());
        max_text_w = std::max(max_text_w, header_text_w);

        max_text_w = std::max(max_text_w, measure_text_width("Parent:"));
        max_text_w = std::max(max_text_w, measure_text_width("Properties:"));
        max_text_w = std::max(max_text_w, measure_text_width("Components:"));
        max_text_w = std::max(max_text_w, measure_text_width("Children:"));

        if (!c.parent_class_id.empty()) {
            const diagram_model::DiagramClass* parent = find_class(diagram, c.parent_class_id);
            const char* parent_name = parent ? parent->type_name.c_str() : c.parent_class_id.c_str();
            max_text_w = std::max(max_text_w, measure_text_width(parent_name));
        }

        for (const auto& p : c.properties) {
            std::string line = format_typed_name_with_default(p.type, p.name, p.default_value);
            max_text_w = std::max(max_text_w, measure_text_width(line.c_str()));
        }
        for (const auto& comp : c.components) {
            std::string line = comp.type + ": " + comp.name;
            max_text_w = std::max(max_text_w, measure_text_width(line.c_str()));
            for (const auto& p : comp.properties) {
                std::string sub_line = format_typed_name_with_default(p.type, p.name, p.default_value);
                max_text_w = std::max(max_text_w, measure_text_width(sub_line.c_str()) + component_subproperty_indent);
            }
        }
        for (const auto& co : c.child_objects) {
            const diagram_model::DiagramClass* child_class = find_class(diagram, co.class_id);
            const char* type_name = child_class ? child_class->type_name.c_str() : co.class_id.c_str();
            std::string name_part = co.label.empty() ? std::string(type_name) : co.label;
            std::string line = std::string(type_name) + ": " + name_part;
            max_text_w = std::max(max_text_w, measure_text_width(line.c_str()));
        }

        double content_area_width = 2.0 * content_inset_side + content_width_padding() + max_text_w;
        double header_width = 2.0 * padding + header_text_w + button_size;
        r.width = std::max({ expanded_min_width, content_area_width, header_width });
        r.width = std::max(r.width, 2.0 * padding + button_size);

        r.height = header_height + content_inset_top + header_content_gap;
        std::size_t component_rows = c.components.size();
        for (const auto& comp : c.components)
            component_rows += comp.properties.size();
        r.height += expanded_content_height(
            1u,
            c.properties.size(),
            component_rows,
            c.child_objects.size(),
            effective_row_height);
        r.height += content_inset_bottom;

        out[c.id] = r;
    }
    return out;
}

} // namespace diagram_render
