cmake_minimum_required(VERSION 3.20)
# Allow FetchContent_Populate for imgui (no root CMakeLists in upstream)
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

project(ImguiPlayground VERSION 0.1.0 LANGUAGES CXX)

# Basic C++ settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Project structure
add_subdirectory(thirdparty)
add_subdirectory(src/libs)
add_subdirectory(src/apps)
enable_testing()
add_subdirectory(src/tests)
