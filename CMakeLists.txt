cmake_minimum_required(VERSION 3.16)
project(atnmCalc CXX C)

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

# --- nlohmann/json (save/open file format) ------------------------------
FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(json)

# --- tinyfiledialogs (native Save As / Open dialogs) ---------------------
# No CMakeLists of its own -- just a single .c/.h pair -- so we fetch the
# source and compile it directly into our target, same as ImGui above.
FetchContent_Declare(
        tinyfiledialogs
        GIT_REPOSITORY https://github.com/native-toolkit/tinyfiledialogs.git
        GIT_TAG        master
)
FetchContent_MakeAvailable(tinyfiledialogs)

find_package(OpenGL REQUIRED)

add_executable(atnmCalc
        src/main.cpp
        src/app.cpp
        src/ast_renderer.cpp


        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        ${tinyfiledialogs_SOURCE_DIR}/tinyfiledialogs.c
        src/lexer.cpp
        src/parser.cpp
        src/parser.h
        src/evaluator.cpp
        src/evaluator.h
        src/backend.cpp
)

target_include_directories(atnmCalc PRIVATE
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${tinyfiledialogs_SOURCE_DIR}
)

target_link_libraries(atnmCalc PRIVATE glfw OpenGL::GL nlohmann_json::nlohmann_json)

# Statically link the MinGW runtime (libgcc, libstdc++, and libwinpthread
# transitively) into the .exe, so a release build runs on a Windows machine
# that doesn't have MinGW installed. Guarded to Windows+MinGW only: fully
# static linking isn't viable on Linux (X11/GL system libraries have no
# static versions to link against) or macOS (doesn't support it at all),
# and applying -static there would break the build rather than help it.
# Only applied to the GUI executable, not the gtest targets -- those are
# developer-only and never get shipped.
if(WIN32 AND MINGW)
    target_link_options(atnmCalc PRIVATE -static)

    # MinGW defaults to the console subsystem, so Windows opens a terminal
    # window alongside the GUI. -mwindows switches to the GUI subsystem
    # (a plain main() still works as the entry point -- MinGW supplies the
    # WinMain shim). Applied to non-Debug builds only: in Debug the console
    # stays, so GLFW errors printed to stderr by GlfwErrorCallback in
    # main.cpp remain visible while developing. In a release build with no
    # console, those messages go nowhere.
    target_link_options(atnmCalc PRIVATE $<$<NOT:$<CONFIG:Debug>>:-mwindows>)
endif()

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
add_executable(evaluatorTests
        src/tests/evaluatorTesting.cpp
        src/evaluator.cpp
        src/lexer.cpp
        src/parser.cpp
)
target_link_libraries(evaluatorTests PRIVATE gtest_main)