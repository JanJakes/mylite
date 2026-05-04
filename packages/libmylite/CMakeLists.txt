configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/mylite/version.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/include/mylite/version.h"
  @ONLY
)

set(MYLITE_SQL_PARSE_DIR "${PROJECT_BINARY_DIR}/generated/libmylite/sql")
set(MYLITE_SQL_PARSE_C "${MYLITE_SQL_PARSE_DIR}/mylite_parse.c")
set(MYLITE_SQL_PARSE_H "${MYLITE_SQL_PARSE_DIR}/mylite_parse.h")

add_custom_command(
  OUTPUT
    "${MYLITE_SQL_PARSE_C}"
    "${MYLITE_SQL_PARSE_H}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${MYLITE_SQL_PARSE_DIR}"
  COMMAND "$<TARGET_FILE:mylite_lemon>"
    -q
    "-T$<TARGET_FILE_DIR:mylite_lemon>/lempar.c"
    "-d${MYLITE_SQL_PARSE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sql/mylite_parse.y"
  DEPENDS
    mylite_lemon
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sql/mylite_parse.y"
  COMMENT "Generating MyLite SQL parser"
  VERBATIM
)

if(MSVC)
  set_source_files_properties("${MYLITE_SQL_PARSE_C}" PROPERTIES
    COMPILE_OPTIONS "/wd4100;/wd4189"
  )
else()
  set_source_files_properties("${MYLITE_SQL_PARSE_C}" PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-parameter;-Wno-unused-variable"
  )
endif()

add_library(mylite STATIC
  "${MYLITE_SQL_PARSE_C}"
  src/mylite.c
  src/charset/mylite_charset.c
  src/runtime/mylite_connection.c
  src/runtime/mylite_connection_statement.c
  src/runtime/mylite_catalog.c
  src/runtime/mylite_diagnostics.c
  src/runtime/mylite_dml.c
  src/runtime/mylite_dml_insert_copy.c
  src/runtime/mylite_dml_update_delete_copy.c
  src/runtime/mylite_field_descriptor.c
  src/runtime/mylite_information_schema.c
  src/runtime/mylite_metadata.c
  src/runtime/mylite_metadata_constants.c
  src/runtime/mylite_schema.c
  src/runtime/mylite_select.c
  src/runtime/mylite_show.c
  src/runtime/mylite_show_create.c
  src/runtime/mylite_show_prepare.c
  src/runtime/mylite_show_tables.c
  src/runtime/mylite_show_targets.c
  src/runtime/mylite_span.c
  src/runtime/mylite_statement.c
  src/runtime/mylite_storage_engine.c
  src/runtime/mylite_table_ddl.c
  src/runtime/mylite_table_ddl_alter.c
  src/runtime/mylite_table_ddl_index.c
  src/runtime/mylite_transactions.c
  src/sql/mylite_ast.c
  src/sql/mylite_expression.c
  src/sql/mylite_lexer.c
  src/sql/mylite_parser.c
  src/sql/mylite_sqlite_translator.c
  src/storage/mylite_file_format.c
  src/storage/mylite_vfs.c
  src/types/mylite_column_type.c
  src/version.c
)
add_library(MyLite::mylite ALIAS mylite)

target_include_directories(mylite
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/charset"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sql"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/storage"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/types"
    "${MYLITE_SQL_PARSE_DIR}"
)
target_link_libraries(mylite PRIVATE MyLite::sqlite)

if(NOT WIN32)
  find_library(MYLITE_MATH_LIBRARY m)
  if(MYLITE_MATH_LIBRARY)
    target_link_libraries(mylite PRIVATE "${MYLITE_MATH_LIBRARY}")
  endif()
endif()

mylite_configure_c_target(mylite)

if(BUILD_TESTING)
  add_executable(mylite_lexer_test
    tests/lexer_test.c
  )
  target_link_libraries(mylite_lexer_test PRIVATE MyLite::mylite)
  target_include_directories(mylite_lexer_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_compile_definitions(mylite_lexer_test PRIVATE
    MYLITE_LEXER_CORPUS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/mysql_lexer_success.sql"
  )
  mylite_configure_c_target(mylite_lexer_test)

  add_executable(mylite_parser_test
    tests/parser_test.c
  )
  target_link_libraries(mylite_parser_test PRIVATE MyLite::mylite)
  target_include_directories(mylite_parser_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  mylite_configure_c_target(mylite_parser_test)

  add_executable(mylite_runtime_test
    tests/runtime_test.c
  )
  target_link_libraries(mylite_runtime_test PRIVATE MyLite::mylite MyLite::sqlite)
  target_include_directories(mylite_runtime_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/storage"
  )
  target_compile_definitions(mylite_runtime_test PRIVATE
    MYLITE_RUNTIME_TEST_FILE_PATH="${CMAKE_CURRENT_BINARY_DIR}/mylite-runtime-test.mylite"
  )
  mylite_configure_c_target(mylite_runtime_test)

  add_executable(mylite_column_type_test
    tests/column_type_test.c
  )
  target_link_libraries(mylite_column_type_test PRIVATE MyLite::mylite)
  target_include_directories(mylite_column_type_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  mylite_configure_c_target(mylite_column_type_test)

  add_executable(mylite_version_test
    tests/version_test.c
  )
  target_link_libraries(mylite_version_test PRIVATE MyLite::mylite)
  mylite_configure_c_target(mylite_version_test)

  add_test(NAME libmylite.lexer COMMAND mylite_lexer_test)
  add_test(NAME libmylite.parser COMMAND mylite_parser_test)
  add_test(NAME libmylite.runtime COMMAND mylite_runtime_test)
  add_test(NAME libmylite.column_type COMMAND mylite_column_type_test)
  add_test(NAME libmylite.version COMMAND mylite_version_test)
endif()
