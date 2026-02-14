#pragma once

#include <diagram_model/types.hpp>
#include <string>
#include <vector>

namespace diagram_placement {

struct Rect {
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;
};

struct PlacedNode {
    std::string node_id;
    Rect rect;
    diagram_model::Node::Shape shape = diagram_model::Node::Shape::Rectangle;
    std::string label;
};

struct PlacedEdge {
    std::string edge_id;
    std::string source_node_id;
    std::string target_node_id;
    std::vector<std::pair<double, double>> points; // polyline in world coordinates
    std::string label;
};

struct PlacedDiagram {
    std::vector<PlacedNode> placed_nodes;
    std::vector<PlacedEdge> placed_edges;
};

} // namespace diagram_placement
