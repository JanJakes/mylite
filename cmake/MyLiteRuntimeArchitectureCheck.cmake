if(POLICY CMP0057)
  cmake_policy(SET CMP0057 NEW)
endif()

if(NOT DEFINED MYLITE_SOURCE_DIR)
  message(FATAL_ERROR "MYLITE_SOURCE_DIR is required")
endif()

set(mylite_removed_monolith
  "${MYLITE_SOURCE_DIR}/packages/libmylite/src/mylite.c")

if(EXISTS "${mylite_removed_monolith}")
  message(FATAL_ERROR
    "packages/libmylite/src/mylite.c has been removed. Do not reintroduce a "
    "top-level runtime monolith; add focused runtime modules instead."
  )
endif()

set(mylite_libmylite_cmake
  "${MYLITE_SOURCE_DIR}/packages/libmylite/CMakeLists.txt")

file(READ "${mylite_libmylite_cmake}" mylite_libmylite_cmake_content)
if(mylite_libmylite_cmake_content MATCHES "(^|\n)[ \t]*src/mylite\\.c([ \t\n]|$)")
  message(FATAL_ERROR
    "packages/libmylite/CMakeLists.txt references src/mylite.c. The old "
    "runtime monolith must stay removed; wire focused runtime modules instead."
  )
endif()

set(mylite_runtime_header
  "${MYLITE_SOURCE_DIR}/packages/libmylite/src/runtime/mylite_runtime.h")
set(mylite_runtime_header_max_lines 240)

file(READ "${mylite_runtime_header}" mylite_runtime_header_content)
string(REGEX MATCHALL "\n" mylite_runtime_header_newlines "${mylite_runtime_header_content}")
list(LENGTH mylite_runtime_header_newlines mylite_runtime_header_line_count)
math(EXPR mylite_runtime_header_line_count "${mylite_runtime_header_line_count} + 1")

if(mylite_runtime_header_line_count GREATER mylite_runtime_header_max_lines)
  message(FATAL_ERROR
    "mylite_runtime.h has ${mylite_runtime_header_line_count} lines; "
    "limit is ${mylite_runtime_header_max_lines}. Move feature-owned types "
    "or helpers into focused runtime headers."
  )
endif()

set(mylite_forbidden_runtime_type_prefixes
  mylite_alter_table
  mylite_cached_expression
  mylite_create_table
  mylite_delete
  mylite_drop_table
  mylite_field_descriptor
  mylite_insert
  mylite_index_ddl
  mylite_metadata
  mylite_pending_auto_increment
  mylite_rename_table
  mylite_result
  mylite_savepoint
  mylite_scalar_result
  mylite_schema
  mylite_select
  mylite_show
  mylite_table_select
  mylite_transaction
  mylite_truncate_table
  mylite_union
  mylite_update
)

foreach(mylite_forbidden_prefix IN LISTS mylite_forbidden_runtime_type_prefixes)
  if(mylite_runtime_header_content MATCHES "(^|\n)(struct|enum) ${mylite_forbidden_prefix}")
    message(FATAL_ERROR
      "mylite_runtime.h defines ${mylite_forbidden_prefix}*. "
      "Feature-owned type definitions belong in focused *_types.h headers."
    )
  endif()
endforeach()

set(mylite_runtime_header_include_allowlist
  mylite_diagnostics.h
  mylite_statement.h
)

file(GLOB mylite_runtime_headers
  "${MYLITE_SOURCE_DIR}/packages/libmylite/src/runtime/*.h")

foreach(mylite_header IN LISTS mylite_runtime_headers)
  get_filename_component(mylite_header_name "${mylite_header}" NAME)
  if(mylite_header_name STREQUAL "mylite_runtime.h")
    continue()
  endif()

  file(READ "${mylite_header}" mylite_header_content)
  if(NOT mylite_header_name IN_LIST mylite_runtime_header_include_allowlist
     AND mylite_header_content MATCHES "#[ \t]*include[ \t]+\"mylite_runtime\\.h\"")
    message(FATAL_ERROR
      "${mylite_header_name} includes mylite_runtime.h. Public runtime headers "
      "should include focused type headers unless they need full mylite_db or "
      "mylite_stmt layout and are added to the explicit allow-list."
    )
  endif()
endforeach()
