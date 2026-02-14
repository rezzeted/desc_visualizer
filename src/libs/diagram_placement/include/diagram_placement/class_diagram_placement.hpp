#pragma once

#include <diagram_model/class_diagram.hpp>
#include <diagram_placement/types.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace diagram_placement {

struct PlacedClassBlock {
    std::string class_id;
    Rect rect;
    double margin = 8.0;
    bool expanded = false;
};

struct PlacedClassDiagram {
    std::vector<PlacedClassBlock> blocks;
};

PlacedClassDiagram place_class_diagram(const diagram_model::ClassDiagram& diagram,
    const std::unordered_map<std::string, bool>& expanded);

} // namespace diagram_placement
