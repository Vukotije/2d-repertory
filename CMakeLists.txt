# Build for macOS / Linux. On Windows use 2d-repertory.sln (Visual Studio + NuGet).
cmake_minimum_required(VERSION 3.16)
project(2d-repertory CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

file(GLOB SOURCES CONFIGURE_DEPENDS Source/*.cpp)
add_executable(2d-repertory ${SOURCES})
target_link_libraries(2d-repertory PRIVATE OpenGL::GL glfw GLEW::GLEW)

if(APPLE)
    target_compile_definitions(2d-repertory PRIVATE GL_SILENCE_DEPRECATION)
endif()

# Shaders and resources are loaded through relative paths (Shaders/..., Resources/...),
# so the program is run from the repository root: `cmake --build build --target run`
add_custom_target(run
    COMMAND 2d-repertory
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL)
