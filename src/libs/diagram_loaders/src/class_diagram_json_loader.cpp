#include <diagram_loaders/json_loader.hpp>
#include <nlohmann/json.hpp>
#include <fstream>

namespace diagram_loaders {

namespace {

diagram_model::Property parse_property(const nlohmann::json& p) {
    diagram_model::Property prop;
    prop.name = p.contains("name") && p["name"].is_string() ? p["name"].get<std::string>() : "";
    prop.type = p.contains("type") && p["type"].is_string() ? p["type"].get<std::string>() : "";
    prop.default_value = p.contains("default_value") && p["default_value"].is_string()
        ? p["default_value"].get<std::string>() : "";
    return prop;
}

diagram_model::Component parse_component(const nlohmann::json& comp) {
    diagram_model::Component comp_val;
    comp_val.name = comp.contains("name") && comp["name"].is_string() ? comp["name"].get<std::string>() : "";
    comp_val.type = comp.contains("type") && comp["type"].is_string() ? comp["type"].get<std::string>() : "";
    if (comp.contains("properties") && comp["properties"].is_array()) {
        for (const auto& p : comp["properties"])
            comp_val.properties.push_back(parse_property(p));
    }
    return comp_val;
}

std::optional<diagram_model::ClassDiagram> parse_class_diagram_json(const nlohmann::json& j) {
    diagram_model::ClassDiagram out;
    if (!j.contains("classes") || !j["classes"].is_array()) return std::nullopt;

    for (const auto& c : j["classes"]) {
        diagram_model::DiagramClass cl;
        if (!c.contains("id") || !c["id"].is_string()) return std::nullopt;
        cl.id = c["id"].get<std::string>();
        cl.type_name = c.contains("type_name") && c["type_name"].is_string()
            ? c["type_name"].get<std::string>() : cl.id;
        // Support both array (new) and single-string (legacy) parent formats.
        if (c.contains("parent_class_ids") && c["parent_class_ids"].is_array()) {
            for (const auto& pid : c["parent_class_ids"])
                if (pid.is_string()) cl.parent_class_ids.push_back(pid.get<std::string>());
        } else if (c.contains("parent_class_id")) {
            if (c["parent_class_id"].is_string())
                cl.parent_class_ids.push_back(c["parent_class_id"].get<std::string>());
            // null -> leave empty
        }
        cl.x = c.contains("x") && c["x"].is_number() ? c["x"].get<double>() : 0;
        cl.y = c.contains("y") && c["y"].is_number() ? c["y"].get<double>() : 0;
        cl.margin = c.contains("margin") && c["margin"].is_number() ? c["margin"].get<double>() : 8.0;

        if (c.contains("properties") && c["properties"].is_array()) {
            for (const auto& p : c["properties"])
                cl.properties.push_back(parse_property(p));
        }
        if (c.contains("components") && c["components"].is_array()) {
            for (const auto& comp : c["components"])
                cl.components.push_back(parse_component(comp));
        }
        if (c.contains("child_objects") && c["child_objects"].is_array()) {
            for (const auto& co : c["child_objects"]) {
                diagram_model::ChildObject child;
                child.class_id = co.contains("class_id") && co["class_id"].is_string() ? co["class_id"].get<std::string>() : "";
                child.label = co.contains("label") && co["label"].is_string() ? co["label"].get<std::string>() : "";
                cl.child_objects.push_back(std::move(child));
            }
        }
        out.classes.push_back(std::move(cl));
    }

    if (j.contains("name") && j["name"].is_string()) out.name = j["name"].get<std::string>();
    if (j.contains("canvas_width") && j["canvas_width"].is_number()) out.canvas_width = j["canvas_width"].get<double>();
    if (j.contains("canvas_height") && j["canvas_height"].is_number()) out.canvas_height = j["canvas_height"].get<double>();

    return out;
}

} // namespace

std::optional<diagram_model::ClassDiagram> load_class_diagram_from_json(std::istream& in) {
    try {
        nlohmann::json j = nlohmann::json::parse(in);
        return parse_class_diagram_json(j);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<diagram_model::ClassDiagram> load_class_diagram_from_json_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) return std::nullopt;
    return load_class_diagram_from_json(f);
}

} // namespace diagram_loaders
