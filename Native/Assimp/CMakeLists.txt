cmake_minimum_required(VERSION 3.16)
project(model_viewer CXX C)
set(CMAKE_CXX_STANDARD 17)

# Output the binary next to the shaders and source files
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_SOURCE_DIR})

find_package(OpenGL REQUIRED)
find_package(glfw3  REQUIRED)
find_package(assimp REQUIRED)
find_package(glm    QUIET)

include(FetchContent)

# GLAD
FetchContent_Declare(glad GIT_REPOSITORY https://github.com/Dav1dde/glad.git GIT_TAG v2.0.6 SOURCE_SUBDIR cmake)
FetchContent_MakeAvailable(glad)
glad_add_library(glad_gl_core REPRODUCIBLE LOADER API gl:core=3.3)

# ImGui
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.91.1)
FetchContent_MakeAvailable(imgui)
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui_lib PUBLIC glfw OpenGL::GL)

# Native File Dialog Extended
FetchContent_Declare(nfd GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git GIT_TAG v1.2.1)
FetchContent_MakeAvailable(nfd)

# Main executable
add_executable(model_viewer main.cpp)
target_include_directories(model_viewer PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(model_viewer PRIVATE
    glad_gl_core glfw assimp::assimp imgui_lib nfd OpenGL::GL
    $<$<BOOL:${glm_FOUND}>:glm::glm>)

# Platform-specific
if(APPLE)
    target_link_libraries(model_viewer PRIVATE "-framework AppKit")
endif()

if(MSVC)
    target_compile_options(model_viewer PRIVATE /W1 /fp:fast /MP)
    set_target_properties(model_viewer PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
elseif(MINGW)
    # MinGW: hide the console window, keep using standard main()
    target_link_options(model_viewer PRIVATE -mwindows)
    target_compile_options(model_viewer PRIVATE -O2)
endif()
