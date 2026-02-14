#pragma once

#include <string>
#include <vector>

namespace diagram_model {

struct Property {
    std::string name;
    std::string type;
    std::string default_value;
};

struct Component {
    std::string name;
    std::string type;
    std::vector<Property> properties;
};

struct ChildObject {
    std::string class_id;
    std::string label;
};

struct DiagramClass {
    std::string id;
    std::string type_name;
    // [0] = primary parent (defines tree layout, color/family, permanent inheritance line)
    // [1..N] = secondary parents (lines shown only on hover)
    std::vector<std::string> parent_class_ids;
    double x = 0;
    double y = 0;
    double margin = 8.0;
    std::vector<Property> properties;
    std::vector<Component> components;
    std::vector<ChildObject> child_objects;
};

struct ClassDiagram {
    std::string name;
    std::vector<DiagramClass> classes;
    double canvas_width = 0;
    double canvas_height = 0;
};

} // namespace diagram_model
