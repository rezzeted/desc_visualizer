#include <diagram_render/renderer.hpp>
#include <diagram_placement/class_diagram_layout_constants.hpp>
#include <diagram_placement/class_diagram_placement.hpp>
#include <diagram_model/class_diagram.hpp>
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

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

// Result of recursive content size computation.
struct ContentSize {
    double height = 0.0;
    double max_text_width = 0.0;
};

bool is_nested_expanded(const std::unordered_map<std::string, bool>& nested_expanded,
    const std::string& key)
{
    auto it = nested_expanded.find(key);
    return it != nested_expanded.end() && it->second;
}

// Recursively compute the height and max text width for all 4 sections
// (Parent, Properties, Components, Children) of a class, including nested expanded items.
// path_prefix: e.g. "Player/" for top-level, "Player/parent/" for nested parent content.
// depth: nesting depth (0 = top-level content of the block).
ContentSize compute_class_content_size(
    const diagram_model::ClassDiagram& diagram,
    const diagram_model::DiagramClass& cls,
    const std::string& path_prefix,
    const std::unordered_map<std::string, bool>& nested_expanded,
    std::unordered_set<std::string>& visited,
    int depth,
    double effective_row_height,
    double effective_row_inner_gap,
    double effective_group_vertical_gap,
    double component_subproperty_indent)
{
    ContentSize result;

    // Text widths are relative to this level's content area (no depth offset).
    // Card padding is added at nesting boundaries when combining widths.
    auto track_text_w = [&](double text_w) {
        result.max_text_width = std::max(result.max_text_width, text_w);
    };

    // --- Parent section ---
    track_text_w(measure_text_width("Parent:"));
    result.height += effective_row_height + effective_row_inner_gap; // header + gap

    if (!cls.parent_class_ids.empty()) {
        for (std::size_t pi = 0; pi < cls.parent_class_ids.size(); ++pi) {
            const diagram_model::DiagramClass* parent = find_class(diagram, cls.parent_class_ids[pi]);
            const char* parent_name = parent ? parent->type_name.c_str() : cls.parent_class_ids[pi].c_str();

            const std::string parent_key = path_prefix + "parent/" + std::to_string(pi);
            const bool is_expanded = parent
                && is_nested_expanded(nested_expanded, parent_key)
                && visited.find(parent->id) == visited.end()
                && depth + 1 < max_nesting_depth;

            if (is_expanded) {
                // Expanded: row merges into card — no separate row, card shown directly.
                visited.insert(parent->id);
                result.height += nested_header_height;
                result.height += nested_card_content_inset_top;
                ContentSize nested = compute_class_content_size(
                    diagram, *parent, parent_key + "/", nested_expanded,
                    visited, depth + 1, effective_row_height,
                    effective_row_inner_gap, effective_group_vertical_gap,
                    component_subproperty_indent);
                result.height += nested.height;
                result.height += nested_card_content_inset_bottom;
                // Card header width: name + buttons.
                track_text_w(measure_text_width(parent_name) + nav_button_size + nav_button_gap + nested_button_size + content_indent);
                result.max_text_width = std::max(result.max_text_width,
                    nested.max_text_width + 2.0 * nested_card_pad_x);
                visited.erase(parent->id);
            } else {
                // Collapsed: show row with name + buttons.
                track_text_w(measure_text_width(parent_name) + nav_button_size + nav_button_gap + nested_button_size + content_indent);
                result.height += effective_row_height;
            }

            if (pi + 1 < cls.parent_class_ids.size())
                result.height += effective_row_inner_gap;
        }
    } else {
        track_text_w(measure_text_width("\xE2\x80\x94")); // em dash
        result.height += effective_row_height;
    }
    result.height += effective_group_vertical_gap; // gap after parent section

    // --- Properties section ---
    track_text_w(measure_text_width("Properties:"));
    result.height += effective_row_height + effective_row_inner_gap; // header + gap

    if (!cls.properties.empty()) {
        for (std::size_t i = 0; i < cls.properties.size(); ++i) {
            const auto& p = cls.properties[i];
            std::string line = format_typed_name_with_default(p.type, p.name, p.default_value);
            track_text_w(measure_text_width(line.c_str()));
            result.height += effective_row_height;
            if (i + 1 < cls.properties.size())
                result.height += effective_row_inner_gap;
        }
    } else {
        track_text_w(measure_text_width("\xE2\x80\x94"));
        result.height += effective_row_height;
    }
    result.height += effective_group_vertical_gap; // gap after properties

    // --- Components section ---
    track_text_w(measure_text_width("Components:"));
    result.height += effective_row_height + effective_row_inner_gap; // header + gap

    if (!cls.components.empty()) {
        for (std::size_t i = 0; i < cls.components.size(); ++i) {
            const auto& comp = cls.components[i];
            std::string line = comp.type + ": " + comp.name;
            track_text_w(measure_text_width(line.c_str()));
            result.height += effective_row_height;

            if (!comp.properties.empty() || i + 1 < cls.components.size())
                result.height += effective_row_inner_gap;

            for (std::size_t j = 0; j < comp.properties.size(); ++j) {
                const auto& p = comp.properties[j];
                std::string sub_line = format_typed_name_with_default(p.type, p.name, p.default_value);
                track_text_w(measure_text_width(sub_line.c_str()) + component_subproperty_indent);
                result.height += effective_row_height;
                if (j + 1 < comp.properties.size() || i + 1 < cls.components.size())
                    result.height += effective_row_inner_gap;
            }
        }
    } else {
        track_text_w(measure_text_width("\xE2\x80\x94"));
        result.height += effective_row_height;
    }
    result.height += effective_group_vertical_gap; // gap after components

    // --- Children section ---
    track_text_w(measure_text_width("Children:"));
    result.height += effective_row_height + effective_row_inner_gap; // header + gap

    if (!cls.child_objects.empty()) {
        for (std::size_t i = 0; i < cls.child_objects.size(); ++i) {
            const auto& co = cls.child_objects[i];
            const diagram_model::DiagramClass* child_class = find_class(diagram, co.class_id);
            const char* type_name = child_class ? child_class->type_name.c_str() : co.class_id.c_str();
            std::string name_part = co.label.empty() ? std::string(type_name) : co.label;
            std::string line = std::string(type_name) + ": " + name_part;

            const std::string child_key = path_prefix + "child/" + std::to_string(i);
            const bool is_expanded = child_class
                && is_nested_expanded(nested_expanded, child_key)
                && visited.find(child_class->id) == visited.end()
                && depth + 1 < max_nesting_depth;

            if (is_expanded) {
                // Expanded: row merges into card.
                visited.insert(child_class->id);
                result.height += nested_header_height;
                result.height += nested_card_content_inset_top;
                ContentSize nested = compute_class_content_size(
                    diagram, *child_class, child_key + "/", nested_expanded,
                    visited, depth + 1, effective_row_height,
                    effective_row_inner_gap, effective_group_vertical_gap,
                    component_subproperty_indent);
                result.height += nested.height;
                result.height += nested_card_content_inset_bottom;
                track_text_w(measure_text_width(line.c_str()) + nav_button_size + nav_button_gap + nested_button_size + content_indent);
                result.max_text_width = std::max(result.max_text_width,
                    nested.max_text_width + 2.0 * nested_card_pad_x);
                visited.erase(child_class->id);
            } else {
                // Collapsed: show row.
                track_text_w(measure_text_width(line.c_str()) + nav_button_size + nav_button_gap + nested_button_size + content_indent);
                result.height += effective_row_height;
            }

            if (i + 1 < cls.child_objects.size())
                result.height += effective_row_inner_gap;
        }
    } else {
        track_text_w(measure_text_width("\xE2\x80\x94"));
        result.height += effective_row_height;
    }
    // No trailing group_vertical_gap after last section.

    return result;
}

} // namespace

std::unordered_map<std::string, diagram_placement::Rect> compute_class_block_sizes(
    const diagram_model::ClassDiagram& diagram,
    const std::unordered_map<std::string, bool>& expanded,
    const std::unordered_map<std::string, bool>& nested_expanded)
{
    std::unordered_map<std::string, diagram_placement::Rect> out;
    const double font_world_height = static_cast<double>(ImGui::GetFontSize());
    const double effective_row_height = std::max(row_height, min_row_height_for_font(font_world_height));
    const double row_gap_ratio = row_height > 0.0 ? (row_inner_gap / row_height) : 0.0;
    const double group_gap_ratio = row_height > 0.0 ? (group_vertical_gap / row_height) : 0.0;
    const double effective_row_inner_gap = effective_row_height * row_gap_ratio;
    const double effective_group_vertical_gap = effective_row_height * group_gap_ratio;
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

        // Recursively compute content size including nested expanded items.
        const std::string path_prefix = c.id + "/";
        std::unordered_set<std::string> visited;
        visited.insert(c.id);
        ContentSize content = compute_class_content_size(
            diagram, c, path_prefix, nested_expanded,
            visited, 0, effective_row_height,
            effective_row_inner_gap, effective_group_vertical_gap,
            component_subproperty_indent);

        double max_text_w = content.max_text_width;
        double header_text_w = measure_text_width(c.type_name.c_str());
        max_text_w = std::max(max_text_w, header_text_w);

        double content_area_width = 2.0 * content_inset_side + content_width_padding() + max_text_w;
        double header_width = 2.0 * padding + header_text_w + button_size;
        r.width = std::max({ expanded_min_width, content_area_width, header_width });
        r.width = std::max(r.width, 2.0 * padding + button_size);

        r.height = header_height + content_inset_top + header_content_gap;
        r.height += content.height;
        r.height += content_inset_bottom;

        out[c.id] = r;
    }
    return out;
}

} // namespace diagram_render
