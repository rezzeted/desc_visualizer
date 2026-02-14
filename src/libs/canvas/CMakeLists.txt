add_library(canvas STATIC
    src/canvas.cpp
)
target_include_directories(canvas PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(canvas PUBLIC
    diagram_render
    diagram_placement
    diagram_model
    imgui_impl
)
