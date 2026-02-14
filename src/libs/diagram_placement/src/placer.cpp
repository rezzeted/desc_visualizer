#include <diagram_placement/placer.hpp>
#include <cmath>
#include <unordered_map>

namespace diagram_placement {

namespace {

const double default_node_width = 80;
const double default_node_height = 40;
const double grid_step = 120;

} // namespace

PlacedDiagram place_diagram(const diagram_model::Diagram& diagram,
    double view_width, double view_height)
{
    (void)view_width;
    (void)view_height;

    PlacedDiagram out;
    std::unordered_map<std::string, Rect> node_rects;

    for (const auto& n : diagram.nodes) {
        PlacedNode pn;
        pn.node_id = n.id;
        pn.label = n.label;
        pn.shape = n.shape;
        pn.rect.x = n.x;
        pn.rect.y = n.y;
        pn.rect.width = n.width > 0 ? n.width : default_node_width;
        pn.rect.height = n.height > 0 ? n.height : default_node_height;
        node_rects[n.id] = pn.rect;
        out.placed_nodes.push_back(std::move(pn));
    }

    // Default layout: stack nodes that have zero position in a column
    double next_y = 0;
    for (auto& pn : out.placed_nodes) {
        if (pn.rect.x == 0 && pn.rect.y == 0) {
            pn.rect.x = 50;
            pn.rect.y = next_y;
            next_y += pn.rect.height + 30;
            node_rects[pn.node_id] = pn.rect;
        }
    }

    for (const auto& e : diagram.edges) {
        PlacedEdge pe;
        pe.edge_id = e.id;
        pe.source_node_id = e.source_node_id;
        pe.target_node_id = e.target_node_id;
        pe.label = e.label;

        auto it_src = node_rects.find(e.source_node_id);
        auto it_tgt = node_rects.find(e.target_node_id);
        if (it_src != node_rects.end() && it_tgt != node_rects.end()) {
            const Rect& r1 = it_src->second;
            const Rect& r2 = it_tgt->second;
            double start_x = r1.x + r1.width / 2;
            double start_y = r1.y + r1.height;
            double end_x = r2.x + r2.width / 2;
            double end_y = r2.y;
            pe.points = { { start_x, start_y }, { end_x, end_y } };
        }
        out.placed_edges.push_back(std::move(pe));
    }

    return out;
}

} // namespace diagram_placement
