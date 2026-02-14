#include <diagram_loaders/json_loader.hpp>
#include <nlohmann/json.hpp>
#include <fstream>

namespace diagram_loaders {

namespace {

diagram_model::Node::Shape shape_from_string(const std::string& s) {
    if (s == "ellipse") return diagram_model::Node::Shape::Ellipse;
    return diagram_model::Node::Shape::Rectangle;
}

std::optional<diagram_model::Diagram> parse_json(const nlohmann::json& j) {
    diagram_model::Diagram d;
    if (!j.contains("nodes") || !j["nodes"].is_array()) return std::nullopt;
    if (!j.contains("edges") || !j["edges"].is_array()) return std::nullopt;

    for (const auto& n : j["nodes"]) {
        diagram_model::Node node;
        if (!n.contains("id") || !n["id"].is_string()) return std::nullopt;
        node.id = n["id"].get<std::string>();
        node.label = n.contains("label") && n["label"].is_string() ? n["label"].get<std::string>() : "";
        node.x = n.contains("x") && n["x"].is_number() ? n["x"].get<double>() : 0;
        node.y = n.contains("y") && n["y"].is_number() ? n["y"].get<double>() : 0;
        node.width = n.contains("width") && n["width"].is_number() ? n["width"].get<double>() : 80;
        node.height = n.contains("height") && n["height"].is_number() ? n["height"].get<double>() : 40;
        if (n.contains("shape") && n["shape"].is_string())
            node.shape = shape_from_string(n["shape"].get<std::string>());
        d.nodes.push_back(std::move(node));
    }

    for (const auto& e : j["edges"]) {
        diagram_model::Edge edge;
        if (!e.contains("source") || !e["source"].is_string()) return std::nullopt;
        if (!e.contains("target") || !e["target"].is_string()) return std::nullopt;
        edge.source_node_id = e["source"].get<std::string>();
        edge.target_node_id = e["target"].get<std::string>();
        edge.id = e.contains("id") && e["id"].is_string() ? e["id"].get<std::string>() : "";
        edge.label = e.contains("label") && e["label"].is_string() ? e["label"].get<std::string>() : "";
        d.edges.push_back(std::move(edge));
    }

    if (j.contains("name") && j["name"].is_string()) d.name = j["name"].get<std::string>();
    if (j.contains("canvas_width") && j["canvas_width"].is_number()) d.canvas_width = j["canvas_width"].get<double>();
    if (j.contains("canvas_height") && j["canvas_height"].is_number()) d.canvas_height = j["canvas_height"].get<double>();

    return d;
}

} // namespace

std::optional<diagram_model::Diagram> load_diagram_from_json(std::istream& in) {
    try {
        nlohmann::json j = nlohmann::json::parse(in);
        return parse_json(j);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<diagram_model::Diagram> load_diagram_from_json_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) return std::nullopt;
    return load_diagram_from_json(f);
}

} // namespace diagram_loaders
