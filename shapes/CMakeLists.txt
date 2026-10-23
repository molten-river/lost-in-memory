cmake_minimum_required(VERSION 3.21)
project(shapes)

add_library(shapes STATIC convex_hull.cpp)
target_include_directories(shapes PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_options(shapes PRIVATE -Wall -Wextra -Wpedantic)
tkit_configure_compile_link_options(shapes NO_EXCEPTIONS NO_RTTI)
target_link_libraries(shapes PUBLIC alias onyx)
