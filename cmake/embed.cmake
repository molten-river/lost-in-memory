function(_embed_symbol_name file out_var)
  get_filename_component(stem "${file}" NAME_WE)
  string(REGEX REPLACE "[^A-Za-z0-9]" "" symbol "${stem}")
  if(symbol MATCHES "^[0-9]")
    set(symbol "Asset${symbol}")
  endif()
  string(SUBSTRING "${symbol}" 0 1 initial)
  string(SUBSTRING "${symbol}" 1 -1 rest)
  string(TOUPPER "${initial}" initial)
  set(symbol "${initial}${rest}")
  set(${out_var} "${symbol}" PARENT_SCOPE)
endfunction()

function(_embed_write_if_changed path content)
  set(staging "${path}.in")
  file(WRITE "${staging}" "${content}")
  configure_file("${staging}" "${path}" COPYONLY)
  file(REMOVE "${staging}")
endfunction()

function(_embed_hex_to_initializer hex out_var)
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
  string(REGEX REPLACE "((0x[0-9a-f][0-9a-f],){20})" "\\1\n    " bytes "${bytes}")
  set(${out_var} "${bytes}" PARENT_SCOPE)
endfunction()

function(embed_assets target)
  set(one_value_args NAMESPACE OUTPUT_HEADER)
  set(multi_value_args FILES)
  cmake_parse_arguments(EMBED "" "${one_value_args}" "${multi_value_args}" ${ARGN})

  if(NOT EMBED_NAMESPACE OR NOT EMBED_OUTPUT_HEADER OR NOT EMBED_FILES)
    message(FATAL_ERROR "embed_assets needs NAMESPACE, OUTPUT_HEADER and FILES")
  endif()

  set(root "${CMAKE_CURRENT_BINARY_DIR}/embedded")
  set(include_dir "${root}/include")
  set(header_path "${include_dir}/${EMBED_OUTPUT_HEADER}")

  set(declarations "")
  set(generated_sources "")

  foreach(file IN LISTS EMBED_FILES)
    get_filename_component(absolute "${file}" ABSOLUTE)
    if(NOT EXISTS "${absolute}")
      message(FATAL_ERROR "embed_assets: missing asset ${absolute}")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${absolute}")

    _embed_symbol_name("${absolute}" symbol)
    get_filename_component(file_name "${absolute}" NAME)

    file(READ "${absolute}" hex HEX)
    _embed_hex_to_initializer("${hex}" initializer)

    set(source_path "${root}/${symbol}.cpp")
    set(source_content
        "#include \"${EMBED_OUTPUT_HEADER}\"

namespace ${EMBED_NAMESPACE}
{
namespace
{
constexpr unsigned char DATA[] = {
    ${initializer}
};
} // namespace

const EmbeddedAsset &${symbol}()
{
    static const EmbeddedAsset asset{TKit::Span<const std::byte>{reinterpret_cast<const std::byte *>(DATA), sizeof(DATA)}, \"${file_name}\"};
    return asset;
}
} // namespace ${EMBED_NAMESPACE}
")
    _embed_write_if_changed("${source_path}" "${source_content}")
    list(APPEND generated_sources "${source_path}")
    string(APPEND declarations "/// ${file_name}\nconst EmbeddedAsset &${symbol}();\n")
  endforeach()

  set(header_content
      "#pragma once

#include \"tkit/container/span.hpp\"
#include <cstddef>

namespace ${EMBED_NAMESPACE}
{
/// A file bundled into the executable at build time.
struct EmbeddedAsset
{
    TKit::Span<const std::byte> Bytes;
    TKit::StringView Name;
};

${declarations}} // namespace ${EMBED_NAMESPACE}
")
  _embed_write_if_changed("${header_path}" "${header_content}")

  target_sources(${target} PRIVATE ${generated_sources} "${header_path}")
  target_include_directories(${target} PRIVATE "${include_dir}")
endfunction()
