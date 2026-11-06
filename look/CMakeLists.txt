cmake_minimum_required(VERSION 3.21)
project(look)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/embed.cmake)

add_library(look STATIC engine_specs.cpp font_library.cpp window_specs.cpp)
target_include_directories(look PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_options(look PRIVATE -Wall -Wextra -Wpedantic)
tkit_configure_compile_link_options(look NO_EXCEPTIONS NO_RTTI)
target_link_libraries(look PUBLIC alias onyx)
embed_assets(
  look
  NAMESPACE Lim::Embedded
  OUTPUT_HEADER look/embedded_assets.hpp
  FILES assets/fonts/FiraCodeNerdFontMonoPlus-Medium.ttf)
