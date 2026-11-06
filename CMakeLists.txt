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

FetchContent_Declare(
  onyx
  GIT_REPOSITORY https://github.com/ismawno/onyx.git
  GIT_TAG v0.12.x
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(onyx)

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
add_subdirectory(lim-shapes)
