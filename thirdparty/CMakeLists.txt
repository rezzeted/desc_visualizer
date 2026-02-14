# Third-party: imgui, SDL3 (via FetchContent)

cmake_minimum_required(VERSION 3.20)

find_package(OpenGL REQUIRED)
include(FetchContent)

# SDL3
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
    GIT_TAG "release-3.2.30"
    GIT_SHALLOW TRUE
)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)

# ImGui (no root CMakeLists; we build our own target)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
    GIT_TAG "docking"
    GIT_SHALLOW TRUE
)
FetchContent_Populate(imgui)

# nlohmann/json (header-only)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY "https://github.com/nlohmann/json.git"
    GIT_TAG "v3.11.3"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# Box2D (physics)
FetchContent_Declare(
    box2d
    GIT_REPOSITORY "https://github.com/erincatto/box2d.git"
    GIT_TAG "v3.1.0"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(box2d)

# spdlog (logging)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY "https://github.com/gabime/spdlog.git"
    GIT_TAG "v1.14.1"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(spdlog)

# ImGui static library with SDL3 + OpenGL3 backends
add_library(imgui_impl STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_impl PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_impl PUBLIC
    SDL3::SDL3
    OpenGL::GL
)
if(WIN32)
    target_link_libraries(imgui_impl PUBLIC opengl32)
endif()
