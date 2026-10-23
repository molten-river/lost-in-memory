cmake_minimum_required(VERSION 3.21)
project(alias)

add_library(alias INTERFACE)
target_include_directories(alias INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(alias INTERFACE toolkit)
