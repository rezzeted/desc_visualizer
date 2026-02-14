#pragma once

#include <diagram_model/class_diagram.hpp>
#include <diagram_placement/class_diagram_placement.hpp>
#include <string>
#include <utility>
#include <vector>

namespace diagram_placement {

enum class ConnectionKind {
    PrimaryInheritance,     // first parent — always visible
    SecondaryInheritance,   // parents [1..N] — visible on hover only
    Composition             // child_objects — always visible
};

struct ConnectionLine {
    std::string from_class_id;  // child / owner
    std::string to_class_id;    // parent / target
    ConnectionKind kind;
    std::string label;          // for Composition: field name (e.g. "inventory")
    std::vector<std::pair<double, double>> points; // route in world coords
};

// Compute all connection lines for the placed diagram.
// Lines include routing points (currently simple 2-point segments).
std::vector<ConnectionLine> compute_connection_lines(
    const diagram_model::ClassDiagram& diagram,
    const PlacedClassDiagram& placed);

} // namespace diagram_placement
