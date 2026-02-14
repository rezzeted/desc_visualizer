add_executable(main main.cpp)
target_link_libraries(main PRIVATE canvas diagram_loaders imgui_impl)
add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:main>/data
    COMMENT "Copy data to output directory"
)
