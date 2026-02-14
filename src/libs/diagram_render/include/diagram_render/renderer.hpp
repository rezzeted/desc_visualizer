#pragma once

struct ImDrawList;
struct ImVec2;

namespace diagram_placement {
struct PlacedDiagram;
}

namespace diagram_render {

void render_diagram(ImDrawList* draw_list,
    const diagram_placement::PlacedDiagram& placed,
    float offset_x, float offset_y, float zoom);

} // namespace diagram_render
