cmake_minimum_required(VERSION 3.16)
project(gltf_loader VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

find_package(glfw3 REQUIRED)
find_package(glm   REQUIRED)

include(FetchContent)

FetchContent_Declare(glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v0.1.36)

FetchContent_Declare(tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        v2.8.21)

FetchContent_Declare(tinyfiledialogs
    GIT_REPOSITORY https://git.code.sf.net/p/tinyfiledialogs/code
    GIT_TAG        master)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.4)

set(GLAD_PROFILE   "core"   CACHE STRING "" FORCE)
set(GLAD_API       "gl=3.3" CACHE STRING "" FORCE)
set(GLAD_GENERATOR "c"      CACHE STRING "" FORCE)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_INSTALL              OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glad tinygltf tinyfiledialogs imgui)

add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

# tinyfiledialogs
add_library(tinyfiledialogs STATIC
    ${tinyfiledialogs_SOURCE_DIR}/tinyfiledialogs.c)
target_include_directories(tinyfiledialogs PUBLIC
    ${tinyfiledialogs_SOURCE_DIR})

# ImGui (core + GLFW + OpenGL3 backends)
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw glad)

add_executable(gltf_loader
    src/main.cpp
    src/shader.cpp
    src/model.cpp
    src/camera.cpp)

target_include_directories(gltf_loader PRIVATE
    include
    ${tinygltf_SOURCE_DIR}
    ${glm_SOURCE_DIR}
    ${tinyfiledialogs_SOURCE_DIR})

target_link_libraries(gltf_loader PRIVATE
    glad glfw glm::glm tinyfiledialogs imgui ${CMAKE_DL_LIBS})

if(APPLE)
    target_link_libraries(gltf_loader PRIVATE
        "-framework OpenGL" "-framework Cocoa" "-framework IOKit")
elseif(UNIX)
    target_link_libraries(gltf_loader PRIVATE GL)
endif()