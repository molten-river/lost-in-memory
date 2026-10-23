cmake_minimum_required(VERSION 3.16...3.31)
project(lost-in-memory-root)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_BUILD_TYPE
    "Debug"
    CACHE STRING "")

include(FetchContent)

set(ONYX_PATCH_STEPS
    PATCH_COMMAND
    ${CMAKE_COMMAND}
    -DONYX_SOURCE_DIR=${FETCHCONTENT_BASE_DIR}/onyx-src
    -P
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patch-onyx-dynamic-cull.cmake)

if(WIN32)
  list(
    APPEND
    ONYX_PATCH_STEPS
    COMMAND
    ${CMAKE_COMMAND}
    -DONYX_SOURCE_DIR=${FETCHCONTENT_BASE_DIR}/onyx-src
    -P
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patch-onyx-overlay.cmake
    COMMAND
    ${CMAKE_COMMAND}
    -DONYX_SOURCE_DIR=${FETCHCONTENT_BASE_DIR}/onyx-src
    -P
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patch-onyx-surface-name.cmake)
endif()

FetchContent_Declare(
  onyx
  GIT_REPOSITORY https://github.com/ismawno/onyx.git
  GIT_TAG v0.11.x
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE ${ONYX_PATCH_STEPS})
FetchContent_MakeAvailable(onyx)

if(NOT TARGET glfw)
  find_package(glfw3 REQUIRED)
endif()

if(WIN32)
  target_compile_definitions(onyx PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_subdirectory(alias)
add_subdirectory(platform)
add_subdirectory(memory)
add_subdirectory(look)
add_subdirectory(tuning)
add_subdirectory(shapes)
tkit_register_for_reflection(shapes SOURCES
    shapes/shape_kit_params.hpp
    shapes/walk_mass_params.hpp)
add_subdirectory(lost-in-memory)
