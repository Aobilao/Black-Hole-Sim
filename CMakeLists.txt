cmake_minimum_required(VERSION 3.16)
project(blackhole LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

add_library(glad external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

add_executable(blackhole src/main.cpp src/Shader.cpp src/Camera.cpp src/stb_image_impl.cpp src/Framebuffer.cpp)
target_include_directories(blackhole PRIVATE external/stb)
target_link_libraries(blackhole PRIVATE glfw glad glm::glm)

find_package(OpenGL REQUIRED)
target_link_libraries(blackhole PRIVATE OpenGL::GL)

target_compile_definitions(blackhole PRIVATE
    SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
