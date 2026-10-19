if(NOT CMAKE_HOST_WIN32)
  return()
endif()

set(SOURCE "${ONYX_SOURCE_DIR}/onyx/source/window.cpp")

if(NOT EXISTS "${SOURCE}")
  message(FATAL_ERROR "LOST-IN-MEMORY - Could not find ${SOURCE} to patch")
endif()

file(READ "${SOURCE}" CONTENTS)

string(REPLACE "nameSurface();" "" PATCHED "${CONTENTS}")

if(NOT PATCHED STREQUAL CONTENTS)
  file(WRITE "${SOURCE}" "${PATCHED}")
  message(STATUS "LOST-IN-MEMORY - Removed the surface naming from onyx window.cpp")
elseif(CONTENTS MATCHES "nameSurface")
  message(STATUS "LOST-IN-MEMORY - onyx window.cpp already patched")
else()
  message(FATAL_ERROR "LOST-IN-MEMORY - Could not find the surface naming in onyx window.cpp")
endif()
