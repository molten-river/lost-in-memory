cmake_minimum_required(VERSION 3.21)
project(shapes)

add_library(shapes STATIC
    convex_hull.cpp
    mesh_builder.cpp
    motion_factory.cpp
    piston_motion.cpp
    ratchet_motion.cpp
    shape_factory.cpp
    shape_renderer.cpp
    slide_motion.cpp
    still_motion.cpp
    walk_mass.cpp
    walk_mass_builder.cpp)
target_include_directories(shapes PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_options(shapes PRIVATE -Wall -Wextra -Wpedantic)
tkit_configure_compile_link_options(shapes NO_EXCEPTIONS NO_RTTI)
target_link_libraries(shapes PUBLIC alias tuning onyx)
