cmake_minimum_required(VERSION 3.16)
project(atnmCalc CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# --- GLFW (windowing) -------------------------------------------------
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE) # avoids requiring wayland-scanner; X11 backend still builds
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
FetchContent_MakeAvailable(glfw)

# --- Dear ImGui (UI) ----------------------------------------------------
# ImGui doesn't ship a CMakeLists for the library itself, so we fetch the
# source and compile it directly into our target below.
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.9
)
FetchContent_MakeAvailable(imgui)

find_package(OpenGL REQUIRED)

add_executable(atnmCalc
    src/main.cpp
    src/app.cpp
    src/ast_renderer.cpp
    src/mock_backend.cpp   # TODO: remove once your real backend replaces this

    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        src/lexer.cpp
        src/parser.cpp
        src/parser.h
)

target_include_directories(atnmCalc PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(atnmCalc PRIVATE glfw OpenGL::GL)

#tests------------------
#gtest
#testing library

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

add_executable(lexerTests
        src/lexer.cpp
        src/tests/lexerTesting.cpp
)

target_link_libraries(lexerTests PRIVATE gtest_main)

add_executable(parserTests
        src/lexer.cpp
        src/tests/parserTesting.cpp
        src/parser.cpp
)

target_link_libraries(parserTests PRIVATE gtest_main)
