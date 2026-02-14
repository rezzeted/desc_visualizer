#pragma once

#include <string>
#include <vector>

namespace diagram_model {

struct Property {
    std::string name;
    std::string type;
};

struct Component {
    std::string name;
    std::string type;
};

struct ChildObject {
    std::string class_id;
    std::string label;
};

struct DiagramClass {
    std::string id;
    std::string type_name;
    std::string parent_class_id;
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
