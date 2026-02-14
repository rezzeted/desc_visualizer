#pragma once

#include <diagram_model/types.hpp>
#include <diagram_model/class_diagram.hpp>
#include <optional>
#include <istream>
#include <string>

namespace diagram_loaders {

std::optional<diagram_model::Diagram> load_diagram_from_json(std::istream& in);
std::optional<diagram_model::Diagram> load_diagram_from_json_file(const std::string& path);

std::optional<diagram_model::ClassDiagram> load_class_diagram_from_json(std::istream& in);
std::optional<diagram_model::ClassDiagram> load_class_diagram_from_json_file(const std::string& path);

} // namespace diagram_loaders
