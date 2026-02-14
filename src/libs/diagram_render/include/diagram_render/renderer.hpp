#pragma once

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
    float offset_x, float offset_y, float zoom);

} // namespace diagram_render
