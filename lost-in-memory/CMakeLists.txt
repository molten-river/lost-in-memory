cmake_minimum_required(VERSION 3.16)
project(lost-in-memory)

include(${PROJECT_SOURCE_DIR}/../cmake/embed.cmake)

set(SOURCES
    lim/main.cpp
    lim/level_params.cpp
    lim/font_library.cpp
    lim/byte_reader.cpp
    lim/region.cpp
    lim/shell.cpp
    lim/blob.cpp
    lim/player.cpp
    lim/level.cpp
    lim/sandbox_screen.cpp
    lim/game.cpp)

include(FetchContent)

set(ONYX_PATCH_STEPS
    PATCH_COMMAND
    ${CMAKE_COMMAND}
    -DONYX_SOURCE_DIR=${FETCHCONTENT_BASE_DIR}/onyx-src
    -P
    ${PROJECT_SOURCE_DIR}/../cmake/patch-onyx-dynamic-cull.cmake)

if(WIN32)
  list(
    APPEND
    ONYX_PATCH_STEPS
    COMMAND
    ${CMAKE_COMMAND}
    -DONYX_SOURCE_DIR=${FETCHCONTENT_BASE_DIR}/onyx-src
    -P
    ${PROJECT_SOURCE_DIR}/../cmake/patch-onyx-overlay.cmake
    COMMAND
    ${CMAKE_COMMAND}
    -DONYX_SOURCE_DIR=${FETCHCONTENT_BASE_DIR}/onyx-src
    -P
    ${PROJECT_SOURCE_DIR}/../cmake/patch-onyx-surface-name.cmake)
endif()

FetchContent_Declare(
  onyx
  GIT_REPOSITORY https://github.com/ismawno/onyx.git
  GIT_TAG v0.11.x
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE ${ONYX_PATCH_STEPS})
FetchContent_MakeAvailable(onyx)

add_subdirectory(platform)

if(NOT TARGET glfw)
  find_package(glfw3 REQUIRED)
endif()

if(WIN32)
  target_compile_definitions(onyx PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_executable(lost-in-memory ${SOURCES})
target_include_directories(lost-in-memory PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(lost-in-memory PRIVATE -Wall -Wextra -Wpedantic)
tkit_register_for_reflection(lost-in-memory SOURCES lim/level_params.hpp)
tkit_configure_compile_link_options(lost-in-memory NO_EXCEPTIONS NO_RTTI)
target_compile_definitions(lost-in-memory PRIVATE LIM_DEFAULTS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/defaults.yaml")
embed_assets(
  lost-in-memory
  NAMESPACE Lim::Embedded
  OUTPUT_HEADER lim/embedded_assets.hpp
  FILES assets/fonts/FiraCodeNerdFontMono-Regular.ttf)
target_link_libraries(lost-in-memory PRIVATE onyx platform glfw)
