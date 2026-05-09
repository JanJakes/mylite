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
    COMPILE_OPTIONS "-Wno-unused-parameter;-Wno-unused-variable;-Wno-type-limits"
  )
endif()

add_library(mylite STATIC
  "${MYLITE_SQL_PARSE_C}"
  src/runtime/mylite_catalog.c
  src/runtime/mylite_connection.c
  src/runtime/mylite_diagnostics.c
  src/runtime/mylite_execution.c
  src/runtime/mylite_result.c
  src/runtime/mylite_result_metadata.c
  src/runtime/mylite_sqlite_bootstrap.c
  src/runtime/mylite_sqlite_registration.c
  src/runtime/mylite_statement_context.c
  src/sql/mylite_ast.c
  src/sql/mylite_lexer.c
  src/sql/mylite_parser.c
  src/storage/mylite_file_open.c
  src/storage/mylite_file_format.c
  src/storage/mylite_offset_vfs.c
  src/version.c
)
add_library(MyLite::mylite ALIAS mylite)

target_include_directories(mylite
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sql"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/storage"
    "${MYLITE_SQL_PARSE_DIR}"
)

target_link_libraries(mylite
  PRIVATE
    MyLite::sqlite
)

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

  add_executable(mylite_file_format_test
    tests/file_format_test.c
  )
  target_link_libraries(mylite_file_format_test PRIVATE MyLite::mylite)
  target_include_directories(mylite_file_format_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_file_format_test)

  add_executable(mylite_version_test
    tests/version_test.c
  )
  target_link_libraries(mylite_version_test PRIVATE MyLite::mylite)
  mylite_configure_c_target(mylite_version_test)

  add_executable(mylite_sqlite_test
    tests/sqlite_test.c
  )
  target_link_libraries(mylite_sqlite_test PRIVATE MyLite::sqlite)
  mylite_configure_c_target(mylite_sqlite_test)

  add_executable(mylite_runtime_open_memory_test
    tests/runtime_open_memory_test.c
  )
  target_link_libraries(mylite_runtime_open_memory_test PRIVATE MyLite::mylite)
  target_include_directories(mylite_runtime_open_memory_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_open_memory_test)

  add_executable(mylite_runtime_file_backed_open_test
    tests/runtime_file_backed_open_test.c
  )
  target_link_libraries(mylite_runtime_file_backed_open_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_file_backed_open_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_file_backed_open_test)

  add_executable(mylite_runtime_catalog_foundation_test
    tests/runtime_catalog_foundation_test.c
  )
  target_link_libraries(mylite_runtime_catalog_foundation_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_catalog_foundation_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_catalog_foundation_test)

  add_executable(mylite_runtime_basic_table_lifecycle_test
    tests/runtime_basic_table_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_basic_table_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_basic_table_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_basic_table_lifecycle_test)

  add_executable(mylite_runtime_table_if_exists_lifecycle_test
    tests/runtime_table_if_exists_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_table_if_exists_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_table_if_exists_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_table_if_exists_lifecycle_test)

  add_executable(mylite_runtime_schema_lifecycle_test
    tests/runtime_schema_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_schema_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_schema_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_schema_lifecycle_test)

  add_executable(mylite_runtime_current_database_function_test
    tests/runtime_current_database_function_test.c
  )
  target_link_libraries(mylite_runtime_current_database_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_current_database_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_current_database_function_test)

  add_executable(mylite_runtime_current_user_identity_test
    tests/runtime_current_user_identity_test.c
  )
  target_link_libraries(mylite_runtime_current_user_identity_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_current_user_identity_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_current_user_identity_test)

  add_executable(mylite_runtime_connection_id_function_test
    tests/runtime_connection_id_function_test.c
  )
  target_link_libraries(mylite_runtime_connection_id_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_connection_id_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_connection_id_function_test)

  add_executable(mylite_runtime_version_function_test
    tests/runtime_version_function_test.c
  )
  target_link_libraries(mylite_runtime_version_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_version_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_version_function_test)

  add_executable(mylite_runtime_row_count_function_test
    tests/runtime_row_count_function_test.c
  )
  target_link_libraries(mylite_runtime_row_count_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_row_count_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_row_count_function_test)

  add_executable(mylite_runtime_count_aggregate_test
    tests/runtime_count_aggregate_test.c
  )
  target_link_libraries(mylite_runtime_count_aggregate_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_count_aggregate_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_count_aggregate_test)

  add_executable(mylite_runtime_show_columns_introspection_test
    tests/runtime_show_columns_introspection_test.c
  )
  target_link_libraries(mylite_runtime_show_columns_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_columns_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_columns_introspection_test)

  add_executable(mylite_runtime_show_index_empty_introspection_test
    tests/runtime_show_index_empty_introspection_test.c
  )
  target_link_libraries(mylite_runtime_show_index_empty_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_index_empty_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_index_empty_introspection_test)

  add_executable(mylite_runtime_show_triggers_empty_introspection_test
    tests/runtime_show_triggers_empty_introspection_test.c
  )
  target_link_libraries(mylite_runtime_show_triggers_empty_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_triggers_empty_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_triggers_empty_introspection_test)

  add_executable(mylite_runtime_show_events_empty_introspection_test
    tests/runtime_show_events_empty_introspection_test.c
  )
  target_link_libraries(mylite_runtime_show_events_empty_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_events_empty_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_events_empty_introspection_test)

  add_executable(mylite_runtime_show_open_tables_empty_introspection_test
    tests/runtime_show_open_tables_empty_introspection_test.c
  )
  target_link_libraries(mylite_runtime_show_open_tables_empty_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_open_tables_empty_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_open_tables_empty_introspection_test)

  add_executable(mylite_runtime_show_routine_status_empty_introspection_test
    tests/runtime_show_routine_status_empty_introspection_test.c
  )
  target_link_libraries(mylite_runtime_show_routine_status_empty_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_routine_status_empty_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_routine_status_empty_introspection_test)

  add_executable(mylite_runtime_show_processlist_introspection_test
    tests/runtime_show_processlist_introspection_test.c
  )
  target_link_libraries(mylite_runtime_show_processlist_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_processlist_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_processlist_introspection_test)

  add_executable(mylite_runtime_show_warnings_diagnostics_test
    tests/runtime_show_warnings_diagnostics_test.c
  )
  target_link_libraries(mylite_runtime_show_warnings_diagnostics_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_warnings_diagnostics_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_warnings_diagnostics_test)

  add_executable(mylite_runtime_show_errors_diagnostics_test
    tests/runtime_show_errors_diagnostics_test.c
  )
  target_link_libraries(mylite_runtime_show_errors_diagnostics_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_errors_diagnostics_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_errors_diagnostics_test)

  add_executable(mylite_runtime_diagnostics_count_variables_test
    tests/runtime_diagnostics_count_variables_test.c
  )
  target_link_libraries(mylite_runtime_diagnostics_count_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_diagnostics_count_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_diagnostics_count_variables_test)

  add_executable(mylite_runtime_autocommit_system_variable_test
    tests/runtime_autocommit_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_autocommit_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_autocommit_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_autocommit_system_variable_test)

  add_executable(mylite_runtime_sql_quote_show_create_system_variable_test
    tests/runtime_sql_quote_show_create_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_quote_show_create_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_quote_show_create_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_quote_show_create_system_variable_test)

  add_executable(mylite_runtime_foreign_key_checks_system_variable_test
    tests/runtime_foreign_key_checks_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_foreign_key_checks_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_foreign_key_checks_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_foreign_key_checks_system_variable_test)

  add_executable(mylite_runtime_unique_checks_system_variable_test
    tests/runtime_unique_checks_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_unique_checks_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_unique_checks_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_unique_checks_system_variable_test)

  add_executable(mylite_runtime_updatable_views_with_limit_system_variable_test
    tests/runtime_updatable_views_with_limit_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_updatable_views_with_limit_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_updatable_views_with_limit_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_updatable_views_with_limit_system_variable_test)

  add_executable(mylite_runtime_sql_auto_is_null_system_variable_test
    tests/runtime_sql_auto_is_null_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_auto_is_null_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_auto_is_null_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_auto_is_null_system_variable_test)

  add_executable(mylite_runtime_sql_big_selects_system_variable_test
    tests/runtime_sql_big_selects_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_big_selects_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_big_selects_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_big_selects_system_variable_test)

  add_executable(mylite_runtime_sql_generate_invisible_primary_key_system_variable_test
    tests/runtime_sql_generate_invisible_primary_key_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_generate_invisible_primary_key_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_generate_invisible_primary_key_system_variable_test
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(
    mylite_runtime_sql_generate_invisible_primary_key_system_variable_test
  )

  add_executable(mylite_runtime_sql_buffer_result_system_variable_test
    tests/runtime_sql_buffer_result_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_buffer_result_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_buffer_result_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_buffer_result_system_variable_test)

  add_executable(mylite_runtime_sql_log_bin_system_variable_test
    tests/runtime_sql_log_bin_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_log_bin_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_log_bin_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_log_bin_system_variable_test)

  add_executable(mylite_runtime_sql_log_off_system_variable_test
    tests/runtime_sql_log_off_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_log_off_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_log_off_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_log_off_system_variable_test)

  add_executable(mylite_runtime_sql_mode_system_variable_test
    tests/runtime_sql_mode_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_mode_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_mode_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_mode_system_variable_test)

  add_executable(mylite_runtime_sql_require_primary_key_system_variable_test
    tests/runtime_sql_require_primary_key_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_require_primary_key_system_variable_test
    PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_require_primary_key_system_variable_test
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(
    mylite_runtime_sql_require_primary_key_system_variable_test
  )

  add_executable(mylite_runtime_sql_replica_skip_counter_system_variable_test
    tests/runtime_sql_replica_skip_counter_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_replica_skip_counter_system_variable_test
    PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_replica_skip_counter_system_variable_test
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(
    mylite_runtime_sql_replica_skip_counter_system_variable_test
  )

  add_executable(mylite_runtime_sql_slave_skip_counter_system_variable_test
    tests/runtime_sql_slave_skip_counter_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_slave_skip_counter_system_variable_test
    PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_slave_skip_counter_system_variable_test
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(
    mylite_runtime_sql_slave_skip_counter_system_variable_test
  )

  add_executable(mylite_runtime_sql_safe_updates_system_variable_test
    tests/runtime_sql_safe_updates_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_safe_updates_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_safe_updates_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_safe_updates_system_variable_test)

  add_executable(mylite_runtime_sql_select_limit_system_variable_test
    tests/runtime_sql_select_limit_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_select_limit_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_select_limit_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_select_limit_system_variable_test)

  add_executable(mylite_runtime_sql_notes_system_variable_test
    tests/runtime_sql_notes_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_notes_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_notes_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_notes_system_variable_test)

  add_executable(mylite_runtime_sql_warnings_system_variable_test
    tests/runtime_sql_warnings_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_sql_warnings_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_warnings_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_warnings_system_variable_test)

  add_executable(mylite_runtime_character_set_system_variables_test
    tests/runtime_character_set_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_character_set_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_character_set_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_character_set_system_variables_test)

  add_executable(mylite_runtime_show_like_filters_test
    tests/runtime_show_like_filters_test.c
  )
  target_link_libraries(mylite_runtime_show_like_filters_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_like_filters_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_like_filters_test)

  add_executable(mylite_runtime_show_create_table_test
    tests/runtime_show_create_table_test.c
  )
  target_link_libraries(mylite_runtime_show_create_table_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_show_create_table_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_create_table_test)

  add_executable(mylite_runtime_show_create_database_test
    tests/runtime_show_create_database_test.c
  )
  target_link_libraries(mylite_runtime_show_create_database_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_create_database_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_create_database_test)

  add_executable(mylite_runtime_show_table_status_introspection_test
    tests/runtime_show_table_status_introspection_test.c
  )
  target_link_libraries(mylite_runtime_show_table_status_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_table_status_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_table_status_introspection_test)

  add_executable(mylite_runtime_show_character_set_collation_test
    tests/runtime_show_character_set_collation_test.c
  )
  target_link_libraries(mylite_runtime_show_character_set_collation_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_character_set_collation_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_character_set_collation_test)

  add_executable(mylite_runtime_innodb_engine_surface_test
    tests/runtime_innodb_engine_surface_test.c
  )
  target_link_libraries(mylite_runtime_innodb_engine_surface_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_innodb_engine_surface_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_innodb_engine_surface_test)

  add_executable(mylite_runtime_table_charset_collation_surface_test
    tests/runtime_table_charset_collation_surface_test.c
  )
  target_link_libraries(mylite_runtime_table_charset_collation_surface_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_table_charset_collation_surface_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_table_charset_collation_surface_test)

  add_executable(mylite_runtime_explain_table_introspection_test
    tests/runtime_explain_table_introspection_test.c
  )
  target_link_libraries(mylite_runtime_explain_table_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_explain_table_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_explain_table_introspection_test)

  add_executable(mylite_runtime_table_rename_lifecycle_test
    tests/runtime_table_rename_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_table_rename_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_table_rename_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_table_rename_lifecycle_test)

  add_executable(mylite_runtime_row_values_lifecycle_test
    tests/runtime_row_values_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_row_values_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_row_values_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_row_values_lifecycle_test)

  add_executable(mylite_runtime_select_where_lifecycle_test
    tests/runtime_select_where_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_select_where_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_select_where_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_select_where_lifecycle_test)

  add_executable(mylite_runtime_select_order_limit_lifecycle_test
    tests/runtime_select_order_limit_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_select_order_limit_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_select_order_limit_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_select_order_limit_lifecycle_test)

  add_executable(mylite_runtime_delete_lifecycle_test
    tests/runtime_delete_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_delete_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_delete_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_delete_lifecycle_test)

  add_executable(mylite_runtime_update_lifecycle_test
    tests/runtime_update_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_update_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_update_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_update_lifecycle_test)

  add_executable(mylite_runtime_small_integer_types_test
    tests/runtime_small_integer_types_test.c
  )
  target_link_libraries(mylite_runtime_small_integer_types_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_small_integer_types_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_small_integer_types_test)

  add_executable(mylite_runtime_integer_default_literals_test
    tests/runtime_integer_default_literals_test.c
  )
  target_link_libraries(mylite_runtime_integer_default_literals_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_integer_default_literals_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_integer_default_literals_test)

  add_executable(mylite_runtime_insert_set_lifecycle_test
    tests/runtime_insert_set_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_insert_set_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_insert_set_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_insert_set_lifecycle_test)

  add_executable(mylite_runtime_truncate_table_lifecycle_test
    tests/runtime_truncate_table_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_truncate_table_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_truncate_table_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_truncate_table_lifecycle_test)

  add_executable(mylite_runtime_alter_table_add_column_lifecycle_test
    tests/runtime_alter_table_add_column_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_add_column_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_add_column_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_add_column_lifecycle_test)

  add_executable(mylite_runtime_alter_table_drop_column_lifecycle_test
    tests/runtime_alter_table_drop_column_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_drop_column_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_drop_column_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_drop_column_lifecycle_test)

  add_executable(mylite_runtime_alter_table_rename_column_lifecycle_test
    tests/runtime_alter_table_rename_column_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_rename_column_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_rename_column_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_rename_column_lifecycle_test)

  add_executable(mylite_runtime_alter_table_modify_column_lifecycle_test
    tests/runtime_alter_table_modify_column_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_modify_column_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_modify_column_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_modify_column_lifecycle_test)

  add_executable(mylite_runtime_alter_table_change_column_lifecycle_test
    tests/runtime_alter_table_change_column_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_change_column_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_change_column_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_change_column_lifecycle_test)

  add_executable(mylite_runtime_sqlite_owner_test
    tests/runtime_sqlite_owner_test.c
  )
  target_link_libraries(mylite_runtime_sqlite_owner_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_sqlite_owner_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sqlite_owner_test)

  add_executable(mylite_runtime_diagnostics_test
    tests/runtime_diagnostics_test.c
  )
  target_link_libraries(mylite_runtime_diagnostics_test PRIVATE MyLite::mylite)
  target_include_directories(mylite_runtime_diagnostics_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_diagnostics_test)

  add_executable(mylite_runtime_statement_context_test
    tests/runtime_statement_context_test.c
  )
  target_link_libraries(mylite_runtime_statement_context_test PRIVATE MyLite::mylite)
  target_include_directories(mylite_runtime_statement_context_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_statement_context_test)

  add_executable(mylite_runtime_result_metadata_test
    tests/runtime_result_metadata_test.c
  )
  target_link_libraries(mylite_runtime_result_metadata_test PRIVATE MyLite::mylite)
  target_include_directories(mylite_runtime_result_metadata_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_result_metadata_test)

  add_executable(mylite_runtime_sqlite_bootstrap_test
    tests/runtime_sqlite_bootstrap_test.c
  )
  target_link_libraries(mylite_runtime_sqlite_bootstrap_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_sqlite_bootstrap_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sqlite_bootstrap_test)

  add_test(NAME libmylite.lexer COMMAND mylite_lexer_test)
  add_test(NAME libmylite.parser COMMAND mylite_parser_test)
  add_test(NAME libmylite.file_format COMMAND mylite_file_format_test)
  add_test(NAME libmylite.runtime.diagnostics COMMAND mylite_runtime_diagnostics_test)
  add_test(
    NAME libmylite.runtime.catalog_foundation
    COMMAND mylite_runtime_catalog_foundation_test
  )
  add_test(
    NAME libmylite.runtime.basic_table_lifecycle
    COMMAND mylite_runtime_basic_table_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.table_if_exists_lifecycle
    COMMAND mylite_runtime_table_if_exists_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.schema_lifecycle
    COMMAND mylite_runtime_schema_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.current_database_function
    COMMAND mylite_runtime_current_database_function_test
  )
  add_test(
    NAME libmylite.runtime.current_user_identity
    COMMAND mylite_runtime_current_user_identity_test
  )
  add_test(
    NAME libmylite.runtime.connection_id_function
    COMMAND mylite_runtime_connection_id_function_test
  )
  add_test(
    NAME libmylite.runtime.version_function
    COMMAND mylite_runtime_version_function_test
  )
  add_test(
    NAME libmylite.runtime.row_count_function
    COMMAND mylite_runtime_row_count_function_test
  )
  add_test(
    NAME libmylite.runtime.count_aggregate
    COMMAND mylite_runtime_count_aggregate_test
  )
  add_test(
    NAME libmylite.runtime.show_columns_introspection
    COMMAND mylite_runtime_show_columns_introspection_test
  )
  add_test(
    NAME libmylite.runtime.show_index_empty_introspection
    COMMAND mylite_runtime_show_index_empty_introspection_test
  )
  add_test(
    NAME libmylite.runtime.show_triggers_empty_introspection
    COMMAND mylite_runtime_show_triggers_empty_introspection_test
  )
  add_test(
    NAME libmylite.runtime.show_events_empty_introspection
    COMMAND mylite_runtime_show_events_empty_introspection_test
  )
  add_test(
    NAME libmylite.runtime.show_open_tables_empty_introspection
    COMMAND mylite_runtime_show_open_tables_empty_introspection_test
  )
  add_test(
    NAME libmylite.runtime.show_routine_status_empty_introspection
    COMMAND mylite_runtime_show_routine_status_empty_introspection_test
  )
  add_test(
    NAME libmylite.runtime.show_processlist_introspection
    COMMAND mylite_runtime_show_processlist_introspection_test
  )
  add_test(
    NAME libmylite.runtime.show_warnings_diagnostics
    COMMAND mylite_runtime_show_warnings_diagnostics_test
  )
  add_test(
    NAME libmylite.runtime.show_errors_diagnostics
    COMMAND mylite_runtime_show_errors_diagnostics_test
  )
  add_test(
    NAME libmylite.runtime.diagnostics_count_variables
    COMMAND mylite_runtime_diagnostics_count_variables_test
  )
  add_test(
    NAME libmylite.runtime.autocommit_system_variable
    COMMAND mylite_runtime_autocommit_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_quote_show_create_system_variable
    COMMAND mylite_runtime_sql_quote_show_create_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.foreign_key_checks_system_variable
    COMMAND mylite_runtime_foreign_key_checks_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.unique_checks_system_variable
    COMMAND mylite_runtime_unique_checks_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.updatable_views_with_limit_system_variable
    COMMAND mylite_runtime_updatable_views_with_limit_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_auto_is_null_system_variable
    COMMAND mylite_runtime_sql_auto_is_null_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_big_selects_system_variable
    COMMAND mylite_runtime_sql_big_selects_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_generate_invisible_primary_key_system_variable
    COMMAND mylite_runtime_sql_generate_invisible_primary_key_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_buffer_result_system_variable
    COMMAND mylite_runtime_sql_buffer_result_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_log_bin_system_variable
    COMMAND mylite_runtime_sql_log_bin_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_log_off_system_variable
    COMMAND mylite_runtime_sql_log_off_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_mode_system_variable
    COMMAND mylite_runtime_sql_mode_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_require_primary_key_system_variable
    COMMAND mylite_runtime_sql_require_primary_key_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_replica_skip_counter_system_variable
    COMMAND mylite_runtime_sql_replica_skip_counter_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_slave_skip_counter_system_variable
    COMMAND mylite_runtime_sql_slave_skip_counter_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_safe_updates_system_variable
    COMMAND mylite_runtime_sql_safe_updates_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_select_limit_system_variable
    COMMAND mylite_runtime_sql_select_limit_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_notes_system_variable
    COMMAND mylite_runtime_sql_notes_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.sql_warnings_system_variable
    COMMAND mylite_runtime_sql_warnings_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.character_set_system_variables
    COMMAND mylite_runtime_character_set_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.show_like_filters
    COMMAND mylite_runtime_show_like_filters_test
  )
  add_test(
    NAME libmylite.runtime.show_create_table
    COMMAND mylite_runtime_show_create_table_test
  )
  add_test(
    NAME libmylite.runtime.show_create_database
    COMMAND mylite_runtime_show_create_database_test
  )
  add_test(
    NAME libmylite.runtime.show_table_status_introspection
    COMMAND mylite_runtime_show_table_status_introspection_test
  )
  add_test(
    NAME libmylite.runtime.show_character_set_collation
    COMMAND mylite_runtime_show_character_set_collation_test
  )
  add_test(
    NAME libmylite.runtime.innodb_engine_surface
    COMMAND mylite_runtime_innodb_engine_surface_test
  )
  add_test(
    NAME libmylite.runtime.table_charset_collation_surface
    COMMAND mylite_runtime_table_charset_collation_surface_test
  )
  add_test(
    NAME libmylite.runtime.explain_table_introspection
    COMMAND mylite_runtime_explain_table_introspection_test
  )
  add_test(
    NAME libmylite.runtime.table_rename_lifecycle
    COMMAND mylite_runtime_table_rename_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_rename_to
    COMMAND mylite_runtime_table_rename_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.row_values_lifecycle
    COMMAND mylite_runtime_row_values_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.select_where_lifecycle
    COMMAND mylite_runtime_select_where_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.select_order_limit_lifecycle
    COMMAND mylite_runtime_select_order_limit_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.delete_lifecycle
    COMMAND mylite_runtime_delete_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.update_lifecycle
    COMMAND mylite_runtime_update_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.small_integer_types
    COMMAND mylite_runtime_small_integer_types_test
  )
  add_test(
    NAME libmylite.runtime.integer_default_literals
    COMMAND mylite_runtime_integer_default_literals_test
  )
  add_test(
    NAME libmylite.runtime.insert_set_lifecycle
    COMMAND mylite_runtime_insert_set_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.truncate_table_lifecycle
    COMMAND mylite_runtime_truncate_table_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_add_column_lifecycle
    COMMAND mylite_runtime_alter_table_add_column_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_drop_column_lifecycle
    COMMAND mylite_runtime_alter_table_drop_column_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_rename_column_lifecycle
    COMMAND mylite_runtime_alter_table_rename_column_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_modify_column_lifecycle
    COMMAND mylite_runtime_alter_table_modify_column_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_change_column_lifecycle
    COMMAND mylite_runtime_alter_table_change_column_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.file_backed_open
    COMMAND mylite_runtime_file_backed_open_test
  )
  add_test(NAME libmylite.runtime.open_memory COMMAND mylite_runtime_open_memory_test)
  add_test(NAME libmylite.runtime.result_metadata COMMAND mylite_runtime_result_metadata_test)
  add_test(
    NAME libmylite.runtime.sqlite_bootstrap
    COMMAND mylite_runtime_sqlite_bootstrap_test
  )
  add_test(
    NAME libmylite.runtime.sqlite_owner
    COMMAND mylite_runtime_sqlite_owner_test
  )
  add_test(
    NAME libmylite.runtime.statement_context
    COMMAND mylite_runtime_statement_context_test
  )
  add_test(NAME libmylite.sqlite COMMAND mylite_sqlite_test)
  add_test(NAME libmylite.version COMMAND mylite_version_test)
endif()
