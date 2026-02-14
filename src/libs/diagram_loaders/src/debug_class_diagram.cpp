#include <diagram_loaders/debug_class_diagram.hpp>

namespace diagram_loaders {

diagram_model::ClassDiagram generate_debug_class_diagram() {
    diagram_model::ClassDiagram out;
    out.name = "Debug class hierarchy";

    // Entity (root)
    diagram_model::DiagramClass entity;
    entity.id = "Entity";
    entity.type_name = "Entity";
    entity.x = 50;
    entity.y = 20;
    entity.properties.push_back({"id", "int"});
    entity.components.push_back({"transform", "Transform"});
    out.classes.push_back(entity);

    // Node : Entity
    diagram_model::DiagramClass node;
    node.id = "Node";
    node.type_name = "Node";
    node.parent_class_id = "Entity";
    node.x = 50;
    node.y = 120;
    node.properties.push_back({"name", "string"});
    node.components.push_back({"mesh", "MeshComponent"});
    node.child_objects.push_back({"Widget", "ui"});
    out.classes.push_back(node);

    // Component : Node
    diagram_model::DiagramClass comp;
    comp.id = "Component";
    comp.type_name = "Component";
    comp.parent_class_id = "Node";
    comp.x = 50;
    comp.y = 220;
    comp.properties.push_back({"enabled", "bool"});
    comp.properties.push_back({"priority", "int"});
    comp.components.push_back({"state", "StateComponent"});
    out.classes.push_back(comp);

    // Widget : Entity
    diagram_model::DiagramClass widget;
    widget.id = "Widget";
    widget.type_name = "Widget";
    widget.parent_class_id = "Entity";
    widget.x = 280;
    widget.y = 20;
    widget.properties.push_back({"visible", "bool"});
    widget.child_objects.push_back({"View", "content"});
    out.classes.push_back(widget);

    // View : Widget
    diagram_model::DiagramClass view;
    view.id = "View";
    view.type_name = "View";
    view.parent_class_id = "Widget";
    view.x = 280;
    view.y = 120;
    view.properties.push_back({"title", "string"});
    view.components.push_back({"layout", "Layout"});
    view.child_objects.push_back({"Component", "root"});
    out.classes.push_back(view);

    return out;
}

} // namespace diagram_loaders
