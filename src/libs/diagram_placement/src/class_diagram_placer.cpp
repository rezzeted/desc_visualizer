#include <diagram_placement/class_diagram_placement.hpp>
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace diagram_placement {

namespace {

const double collapsed_width = 140;
const double collapsed_height = 32;
const double expanded_min_width = 180;
const double header_height = 28;
const double section_header_height = 20;
const double row_height = 18;
const double padding = 8;
const double button_size = 20;
const double block_margin = 16;

double estimate_text_width(const std::string& s) {
    return std::max(expanded_min_width - 2 * padding, static_cast<double>(s.size()) * 7.0);
}

} // namespace

PlacedClassDiagram place_class_diagram(const diagram_model::ClassDiagram& diagram,
    const std::unordered_map<std::string, bool>& expanded)
{
    PlacedClassDiagram out;
    if (diagram.classes.empty()) return out;

    std::unordered_map<std::string, const diagram_model::DiagramClass*> by_id;
    for (const auto& c : diagram.classes)
        by_id[c.id] = &c;

    double next_x = padding;
    double row_top = padding;
    double row_bottom = padding;

    for (const auto& c : diagram.classes) {
        PlacedClassBlock block;
        block.class_id = c.id;
        block.expanded = false;
        auto it = expanded.find(c.id);
        if (it != expanded.end()) block.expanded = it->second;

        double w, h;
        if (!block.expanded) {
            w = collapsed_width;
            h = collapsed_height;
        } else {
            double content_w = estimate_text_width(c.type_name);
            size_t rows = 0;
            if (!c.parent_class_id.empty()) rows += 1;
            rows += c.properties.size();
            rows += c.components.size();
            rows += c.child_objects.size();
            content_w = std::max(content_w, expanded_min_width);
            for (const auto& p : c.properties)
                content_w = std::max(content_w, estimate_text_width(p.name + ": " + p.type));
            for (const auto& comp : c.components)
                content_w = std::max(content_w, estimate_text_width(comp.name + ": " + comp.type));
            for (const auto& co : c.child_objects)
                content_w = std::max(content_w, estimate_text_width(co.label.empty() ? co.class_id : co.label + " (" + co.class_id + ")"));
            w = content_w + 2 * padding + button_size;
            h = header_height;
            if (!c.parent_class_id.empty()) h += section_header_height + row_height;
            h += (c.properties.empty() ? 0 : section_header_height + c.properties.size() * row_height);
            h += (c.components.empty() ? 0 : section_header_height + c.components.size() * row_height);
            h += (c.child_objects.empty() ? 0 : section_header_height + c.child_objects.size() * row_height);
        }

        block.rect.width = w;
        block.rect.height = h;
        block.rect.x = c.x != 0 || c.y != 0 ? c.x : next_x;
        block.rect.y = c.x != 0 || c.y != 0 ? c.y : row_top;

        if (c.x == 0 && c.y == 0) {
            next_x += w + block_margin;
            row_bottom = std::max(row_bottom, row_top + h);
        }

        out.blocks.push_back(std::move(block));
    }

    return out;
}

} // namespace diagram_placement
