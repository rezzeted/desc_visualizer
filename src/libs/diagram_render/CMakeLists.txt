add_library(diagram_render STATIC
    src/renderer.cpp
)
target_include_directories(diagram_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(diagram_render PUBLIC
    diagram_placement
    imgui_impl
)
