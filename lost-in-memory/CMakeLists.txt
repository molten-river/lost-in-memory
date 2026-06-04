cmake_minimum_required(VERSION 3.16)
project(lost-in-memory)

set(SOURCES lim/main.cpp)

include(FetchContent)
FetchContent_Declare(
  onyx
  GIT_REPOSITORY https://github.com/ismawno/onyx.git
  GIT_TAG main
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(onyx)

add_executable(lost-in-memory ${SOURCES})
target_link_libraries(lost-in-memory PRIVATE onyx)
