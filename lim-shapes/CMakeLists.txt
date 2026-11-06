cmake_minimum_required(VERSION 3.21)
project(lim-shapes)

set(SOURCES
    viewer/main.cpp
    viewer/viewer.cpp
    viewer/camera_registry.cpp
    viewer/fly_camera.cpp
    viewer/frame_timings.cpp
    viewer/orbit_camera.cpp
    viewer/playback_clock.cpp
    viewer/record_gallery.cpp
    viewer/record_hud.cpp
    viewer/viewer_visibility.cpp)

add_executable(lim-shapes ${SOURCES})
target_include_directories(lim-shapes PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(lim-shapes PRIVATE -Wall -Wextra -Wpedantic)
tkit_register_for_reflection(lim-shapes SOURCES
    viewer/viewer_params.hpp)
tkit_configure_compile_link_options(lim-shapes NO_EXCEPTIONS NO_RTTI)
target_compile_definitions(lim-shapes PRIVATE LIM_SHAPES_DEFAULTS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/defaults.yaml")
target_link_libraries(lim-shapes PRIVATE memory look tuning shapes onyx)
