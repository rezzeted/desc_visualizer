#pragma once

#include <diagram_placement/types.hpp>
#include <diagram_model/types.hpp>

namespace diagram_placement {

PlacedDiagram place_diagram(const diagram_model::Diagram& diagram,
    double view_width = 0, double view_height = 0);

} // namespace diagram_placement
