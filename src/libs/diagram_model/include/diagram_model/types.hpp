#pragma once

#include <string>
#include <vector>

namespace diagram_model {

struct Node {
    std::string id;
    std::string label;
    double x = 0;
    double y = 0;
    double width = 80;
    double height = 40;
    enum class Shape { Rectangle, Ellipse };
    Shape shape = Shape::Rectangle;
};

struct Edge {
    std::string id;
    std::string source_node_id;
    std::string target_node_id;
    std::string label;
};

struct Diagram {
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::string name;
    double canvas_width = 0;
    double canvas_height = 0;
};

} // namespace diagram_model
