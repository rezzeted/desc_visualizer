add_library(animation STATIC
    src/rect_animator.cpp
)
target_include_directories(animation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(animation PUBLIC diagram_placement)
