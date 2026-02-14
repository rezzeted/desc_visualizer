#include <diagram_render/renderer.hpp>
#include <diagram_placement/types.hpp>
#include <diagram_model/types.hpp>
#include "imgui.h"
#include <cmath>
#include <algorithm>

namespace diagram_render {

namespace {

ImVec2 world_to_screen(float wx, float wy, float offset_x, float offset_y, float zoom) {
    return ImVec2(wx * zoom + offset_x, wy * zoom + offset_y);
}

} // namespace

void render_diagram(ImDrawList* draw_list,
    const diagram_placement::PlacedDiagram& placed,
    float offset_x, float offset_y, float zoom)
{
    if (!draw_list) return;

    const unsigned int edge_color = IM_COL32(120, 120, 120, 255);
    const unsigned int node_fill = IM_COL32(45, 45, 48, 255);
    const unsigned int node_border = IM_COL32(100, 100, 105, 255);
    const unsigned int text_color = IM_COL32(220, 220, 220, 255);
    const float line_thickness = 2.0f;

    for (const auto& pe : placed.placed_edges) {
        if (pe.points.size() < 2) continue;
        for (size_t i = 0; i + 1 < pe.points.size(); ++i) {
            ImVec2 a = world_to_screen(
                (float)pe.points[i].first, (float)pe.points[i].second,
                offset_x, offset_y, zoom);
            ImVec2 b = world_to_screen(
                (float)pe.points[i + 1].first, (float)pe.points[i + 1].second,
                offset_x, offset_y, zoom);
            draw_list->AddLine(a, b, edge_color, line_thickness);
        }
    }

    for (const auto& pn : placed.placed_nodes) {
        float x = (float)pn.rect.x;
        float y = (float)pn.rect.y;
        float w = (float)pn.rect.width;
        float h = (float)pn.rect.height;
        ImVec2 min_pt = world_to_screen(x, y, offset_x, offset_y, zoom);
        ImVec2 max_pt = world_to_screen(x + w, y + h, offset_x, offset_y, zoom);

        if (pn.shape == diagram_model::Node::Shape::Ellipse) {
            float cx = x + w * 0.5f;
            float cy = y + h * 0.5f;
            float r = std::min(w, h) * 0.5f;
            ImVec2 center = world_to_screen(cx, cy, offset_x, offset_y, zoom);
            float radius = r * zoom;
            draw_list->AddCircleFilled(center, radius, node_fill);
            draw_list->AddCircle(center, radius, node_border, 0, line_thickness);
        } else {
            draw_list->AddRectFilled(min_pt, max_pt, node_fill);
            draw_list->AddRect(min_pt, max_pt, node_border, 0.0f, 0, line_thickness);
        }

        if (!pn.label.empty()) {
            ImVec2 text_size = ImGui::CalcTextSize(pn.label.c_str());
            float tx = min_pt.x + (max_pt.x - min_pt.x - text_size.x) * 0.5f;
            float ty = min_pt.y + (max_pt.y - min_pt.y - text_size.y) * 0.5f;
            draw_list->AddText(ImVec2(tx, ty), text_color, pn.label.c_str());
        }
    }
}

} // namespace diagram_render
