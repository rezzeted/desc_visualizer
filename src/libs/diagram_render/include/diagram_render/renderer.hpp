#pragma once

#include <diagram_placement/types.hpp>
#include <diagram_placement/connection_lines.hpp>
#include <diagram_render/nested_hit_button.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct ImDrawList;
struct ImVec2;

namespace diagram_placement {
struct PlacedDiagram;
struct PlacedClassDiagram;
}
namespace diagram_model {
struct ClassDiagram;
}

namespace diagram_render {

void render_diagram(ImDrawList* draw_list,
    const diagram_placement::PlacedDiagram& placed,
    float offset_x, float offset_y, float zoom);

void render_class_diagram(ImDrawList* draw_list,
    const diagram_model::ClassDiagram& diagram,
    const diagram_placement::PlacedClassDiagram& placed,
    float offset_x, float offset_y, float zoom,
    const std::unordered_map<std::string, bool>& nested_expanded,
    std::vector<NestedHitButton>* out_hit_buttons = nullptr,
    std::vector<NavHitButton>* out_nav_buttons = nullptr,
    std::vector<ClassHoverRegion>* out_hover_regions = nullptr,
    const std::string& hovered_class_id = {},
    const std::vector<diagram_placement::ConnectionLine>& connection_lines = {},
    const std::unordered_set<std::string>& highlighted_class_ids = {});

// Computes block width/height from content using ImGui::CalcTextSize (current font).
// Call only when ImGui context is active. Returns map class_id -> Rect (width and height set; x,y zero).
std::unordered_map<std::string, diagram_placement::Rect> compute_class_block_sizes(
    const diagram_model::ClassDiagram& diagram,
    const std::unordered_map<std::string, bool>& expanded,
    const std::unordered_map<std::string, bool>& nested_expanded = {});

} // namespace diagram_render
