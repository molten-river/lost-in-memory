if(NOT CMAKE_HOST_WIN32)
  return()
endif()

set(SOURCE "${ONYX_SOURCE_DIR}/onyx/source/overlay.cpp")

if(NOT EXISTS "${SOURCE}")
  message(FATAL_ERROR "LOST-IN-MEMORY - Could not find ${SOURCE} to patch")
endif()

file(READ "${SOURCE}" CONTENTS)

if(CONTENTS MATCHES "m_SerializationPath\.string\(\)")
  message(STATUS "LOST-IN-MEMORY - onyx overlay.cpp already patched")
else()
  string(REPLACE "TKit::Yaml::FromFile(m_SerializationPath)" "TKit::Yaml::FromFile(m_SerializationPath.string())"
                 PATCHED "${CONTENTS}")
  string(REPLACE "TKit::Yaml::ToFile(m_SerializationPath, root)" "TKit::Yaml::ToFile(m_SerializationPath.string(), root)"
                 PATCHED "${PATCHED}")

  if(PATCHED STREQUAL CONTENTS)
    message(FATAL_ERROR "LOST-IN-MEMORY - Could not patch the std::filesystem::path conversions in onyx overlay.cpp")
  endif()

  file(WRITE "${SOURCE}" "${PATCHED}")
  message(STATUS "LOST-IN-MEMORY - Patched the std::filesystem::path conversions in onyx overlay.cpp")
endif()
