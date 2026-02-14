add_library(diagram_placement STATIC
    src/placer.cpp
    src/class_diagram_placer.cpp
)
target_include_directories(diagram_placement PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(diagram_placement PUBLIC diagram_model)
