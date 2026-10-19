cmake_minimum_required(VERSION 3.16)
project(lost-in-memory)

set(SOURCES
    lim/main.cpp
    lim/level_params.cpp)

include(FetchContent)

if(WIN32)
  set(ONYX_PATCH_STEPS
      PATCH_COMMAND
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

if(WIN32)
  target_compile_definitions(onyx PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_executable(lost-in-memory ${SOURCES})
target_include_directories(lost-in-memory PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(lost-in-memory PRIVATE -Wall -Wextra -Wpedantic)
tkit_register_for_reflection(lost-in-memory SOURCES lim/level_params.hpp)
tkit_configure_compile_link_options(lost-in-memory NO_EXCEPTIONS NO_RTTI)
target_compile_definitions(lost-in-memory PRIVATE LIM_DEFAULTS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/defaults.yaml")
target_link_libraries(lost-in-memory PRIVATE onyx)
