add_library(diagram_loaders STATIC
    src/json_loader.cpp
)
target_include_directories(diagram_loaders PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(diagram_loaders PUBLIC
    diagram_model
    nlohmann_json::nlohmann_json
)
