cmake_minimum_required(VERSION 3.21)
project(tuning)

add_library(tuning STATIC param_panel.cpp)
target_include_directories(tuning PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_options(tuning PRIVATE -Wall -Wextra -Wpedantic)
tkit_configure_compile_link_options(tuning NO_EXCEPTIONS NO_RTTI)
target_link_libraries(tuning PUBLIC alias look onyx)
