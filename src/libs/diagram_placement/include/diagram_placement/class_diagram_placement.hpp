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

// If block_sizes is non-null, use it for each block's width/height (content-driven layout).
// Otherwise fallback to internal size estimation.
// If previous_positions is non-null, use it for initial (x,y) of each block to preserve stability when expanding.
PlacedClassDiagram place_class_diagram(const diagram_model::ClassDiagram& diagram,
    const std::unordered_map<std::string, bool>& expanded,
    const std::unordered_map<std::string, Rect>* block_sizes = nullptr,
    const std::unordered_map<std::string, Rect>* previous_positions = nullptr);

} // namespace diagram_placement
