cmake_minimum_required(VERSION 3.16)
project(lost-in-memory)

set(SOURCES
    lim/main.cpp
    lim/level_params.cpp
    lim/shell.cpp
    lim/blob.cpp
    lim/player.cpp
    lim/shape_grammar.cpp
    lim/body.cpp
    lim/world_decoder.cpp
    lim/section_graph.cpp
    lim/level.cpp
    lim/sandbox_screen.cpp
    lim/game.cpp)

add_executable(lost-in-memory ${SOURCES})
target_include_directories(lost-in-memory PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(lost-in-memory PRIVATE -Wall -Wextra -Wpedantic)
tkit_register_for_reflection(lost-in-memory SOURCES
    lim/level_params.hpp
    lim/palette.hpp
    lim/player_params.hpp
    lim/shell_params.hpp
    lim/shape_params.hpp
    lim/form_params.hpp
    lim/formula_params.hpp)
tkit_configure_compile_link_options(lost-in-memory NO_EXCEPTIONS NO_RTTI)
target_compile_definitions(lost-in-memory PRIVATE LIM_DEFAULTS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/defaults.yaml")
target_compile_definitions(lost-in-memory PRIVATE LIM_DEBUG_TOOLS=$<IF:$<CONFIG:Debug>,1,0>)
target_link_libraries(lost-in-memory PRIVATE alias platform memory look shapes onyx glfw)
