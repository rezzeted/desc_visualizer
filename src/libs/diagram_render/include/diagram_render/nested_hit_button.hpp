#pragma once

#include <string>

namespace diagram_render {

// Hit region for a nested expand/collapse button inside a class card.
// Coordinates are in world space. Populated during rendering, checked on click.
struct NestedHitButton {
    std::string block_class_id; // top-level block that owns this button
    std::string path;           // tree path key, e.g. "Player/parent" or "Player/child/0"
    double x = 0;               // world-space button rect
    double y = 0;
    double w = 0;
    double h = 0;
};

// Hit region for a navigate-to-class arrow button inside a class card.
// When clicked, the viewport centers on the target class's block.
struct NavHitButton {
    std::string target_class_id;  // ID of the class to navigate to
    double x = 0;                 // world-space button rect
    double y = 0;
    double w = 0;
    double h = 0;
};

} // namespace diagram_render
