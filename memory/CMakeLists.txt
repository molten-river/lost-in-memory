cmake_minimum_required(VERSION 3.21)
project(memory)

add_library(memory STATIC region.cpp byte_reader.cpp)
target_include_directories(memory PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_options(memory PRIVATE -Wall -Wextra -Wpedantic)
tkit_configure_compile_link_options(memory NO_EXCEPTIONS NO_RTTI)
target_link_libraries(memory PUBLIC alias platform toolkit)
