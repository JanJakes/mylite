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
    COMPILE_OPTIONS "-Wno-unused-parameter;-Wno-unused-variable;-Wno-type-limits;-Wno-sign-conversion"
  )
endif()

add_library(mylite STATIC
  "${MYLITE_SQL_PARSE_C}"
  src/runtime/mylite_bitwise_aggregate.c
  src/runtime/mylite_catalog.c
  src/runtime/mylite_connection.c
  src/runtime/mylite_date_format.c
  src/runtime/mylite_date_interval_second.c
  src/runtime/mylite_datediff.c
  src/runtime/mylite_diagnostics.c
  src/runtime/mylite_execution.c
  src/runtime/mylite_group_concat_aggregate.c
  src/runtime/mylite_integer_arithmetic.c
  src/runtime/mylite_json.c
  src/runtime/mylite_json_functions.c
  src/runtime/mylite_regexp.c
  src/runtime/mylite_result.c
  src/runtime/mylite_result_metadata.c
  src/runtime/mylite_sqlite_bootstrap.c
  src/runtime/mylite_sqlite_registration.c
  src/runtime/mylite_statement_context.c
  src/runtime/mylite_string_case.c
  src/runtime/mylite_string_char.c
  src/runtime/mylite_string_codepoint.c
  src/runtime/mylite_string_concat.c
  src/runtime/mylite_string_padding.c
  src/runtime/mylite_string_quote.c
  src/runtime/mylite_string_replace.c
  src/runtime/mylite_string_reverse.c
  src/runtime/mylite_string_search.c
  src/runtime/mylite_string_substring_index.c
  src/runtime/mylite_string_trim.c
  src/runtime/mylite_string_unhex.c
  src/runtime/mylite_temporal_extract.c
  src/runtime/mylite_temporary_catalog.c
  src/runtime/mylite_timestampdiff.c
  src/runtime/mylite_unix_timestamp.c
  src/runtime/mylite_uuid.c
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
if(UNIX AND NOT APPLE)
  target_link_libraries(mylite PRIVATE m)
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

  add_executable(mylite_runtime_if_function_test
    tests/runtime_if_function_test.c
  )
  target_link_libraries(mylite_runtime_if_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_if_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_if_function_test)

  add_executable(mylite_runtime_ifnull_function_test
    tests/runtime_ifnull_function_test.c
  )
  target_link_libraries(mylite_runtime_ifnull_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_ifnull_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_ifnull_function_test)

  add_executable(mylite_runtime_coalesce_function_test
    tests/runtime_coalesce_function_test.c
  )
  target_link_libraries(mylite_runtime_coalesce_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_coalesce_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_coalesce_function_test)

  add_executable(mylite_runtime_nullif_function_test
    tests/runtime_nullif_function_test.c
  )
  target_link_libraries(mylite_runtime_nullif_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_nullif_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_nullif_function_test)

  add_executable(mylite_runtime_isnull_function_test
    tests/runtime_isnull_function_test.c
  )
  target_link_libraries(mylite_runtime_isnull_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_isnull_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_isnull_function_test)

  add_executable(mylite_runtime_case_operator_test
    tests/runtime_case_operator_test.c
  )
  target_link_libraries(mylite_runtime_case_operator_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_case_operator_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_case_operator_test)

  add_executable(mylite_runtime_do_statement_test
    tests/runtime_do_statement_test.c
  )
  target_link_libraries(mylite_runtime_do_statement_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_do_statement_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_do_statement_test)

  add_executable(mylite_runtime_scalar_expression_projection_test
    tests/runtime_scalar_expression_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_expression_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_expression_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_expression_projection_test)

  add_executable(mylite_runtime_scalar_arithmetic_projection_test
    tests/runtime_scalar_arithmetic_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_arithmetic_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_arithmetic_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_arithmetic_projection_test)

  add_executable(mylite_runtime_scalar_modulo_projection_test
    tests/runtime_scalar_modulo_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_modulo_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_modulo_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_modulo_projection_test)

  add_executable(mylite_runtime_scalar_div_projection_test
    tests/runtime_scalar_div_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_div_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_div_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_div_projection_test)

  add_executable(mylite_runtime_scalar_division_projection_test
    tests/runtime_scalar_division_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_division_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_division_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_division_projection_test)

  add_executable(mylite_runtime_scalar_bitwise_projection_test
    tests/runtime_scalar_bitwise_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_bitwise_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_bitwise_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_bitwise_projection_test)

  add_executable(mylite_runtime_abs_function_test
    tests/runtime_abs_function_test.c
  )
  target_link_libraries(mylite_runtime_abs_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_abs_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_abs_function_test)

  add_executable(mylite_runtime_sign_function_test
    tests/runtime_sign_function_test.c
  )
  target_link_libraries(mylite_runtime_sign_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sign_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sign_function_test)

  add_executable(mylite_runtime_ceil_floor_functions_test
    tests/runtime_ceil_floor_functions_test.c
  )
  target_link_libraries(mylite_runtime_ceil_floor_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_ceil_floor_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_ceil_floor_functions_test)

  add_executable(mylite_runtime_round_function_test
    tests/runtime_round_function_test.c
  )
  target_link_libraries(mylite_runtime_round_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_round_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_round_function_test)

  add_executable(mylite_runtime_bin_oct_functions_test
    tests/runtime_bin_oct_functions_test.c
  )
  target_link_libraries(mylite_runtime_bin_oct_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_bin_oct_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_bin_oct_functions_test)

  add_executable(mylite_runtime_conv_function_test
    tests/runtime_conv_function_test.c
  )
  target_link_libraries(mylite_runtime_conv_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_conv_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_conv_function_test)

  add_executable(mylite_runtime_pi_function_test
    tests/runtime_pi_function_test.c
  )
  target_link_libraries(mylite_runtime_pi_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_pi_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_pi_function_test)

  add_executable(mylite_runtime_rand_function_test
    tests/runtime_rand_function_test.c
  )
  target_link_libraries(mylite_runtime_rand_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_rand_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_rand_function_test)

  add_executable(mylite_runtime_sqrt_function_test
    tests/runtime_sqrt_function_test.c
  )
  target_link_libraries(mylite_runtime_sqrt_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sqrt_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sqrt_function_test)

  add_executable(mylite_runtime_degrees_radians_functions_test
    tests/runtime_degrees_radians_functions_test.c
  )
  target_link_libraries(mylite_runtime_degrees_radians_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_degrees_radians_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_degrees_radians_functions_test)

  add_executable(mylite_runtime_acos_asin_functions_test
    tests/runtime_acos_asin_functions_test.c
  )
  target_link_libraries(mylite_runtime_acos_asin_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_acos_asin_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_acos_asin_functions_test)

  add_executable(mylite_runtime_atan_functions_test
    tests/runtime_atan_functions_test.c
  )
  target_link_libraries(mylite_runtime_atan_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_atan_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_atan_functions_test)

  add_executable(mylite_runtime_trigonometric_functions_test
    tests/runtime_trigonometric_functions_test.c
  )
  target_link_libraries(mylite_runtime_trigonometric_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_trigonometric_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_trigonometric_functions_test)

  add_executable(mylite_runtime_exp_log_power_functions_test
    tests/runtime_exp_log_power_functions_test.c
  )
  target_link_libraries(mylite_runtime_exp_log_power_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_exp_log_power_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_exp_log_power_functions_test)

  add_executable(mylite_runtime_bit_count_function_test
    tests/runtime_bit_count_function_test.c
  )
  target_link_libraries(mylite_runtime_bit_count_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_bit_count_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_bit_count_function_test)

  add_executable(mylite_runtime_numeric_format_truncate_crc32_test
    tests/runtime_numeric_format_truncate_crc32_test.c
  )
  target_link_libraries(mylite_runtime_numeric_format_truncate_crc32_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_numeric_format_truncate_crc32_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_numeric_format_truncate_crc32_test)

  add_executable(mylite_runtime_hex_function_test
    tests/runtime_hex_function_test.c
  )
  target_link_libraries(mylite_runtime_hex_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_hex_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_hex_function_test)

  add_executable(mylite_runtime_unhex_function_test
    tests/runtime_unhex_function_test.c
  )
  target_link_libraries(mylite_runtime_unhex_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_unhex_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_unhex_function_test)

  add_executable(mylite_runtime_uuid_conversion_functions_test
    tests/runtime_uuid_conversion_functions_test.c
  )
  target_link_libraries(mylite_runtime_uuid_conversion_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_uuid_conversion_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_uuid_conversion_functions_test)

  add_executable(mylite_runtime_char_function_test
    tests/runtime_char_function_test.c
  )
  target_link_libraries(mylite_runtime_char_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_char_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_char_function_test)

  add_executable(mylite_runtime_default_function_test
    tests/runtime_default_function_test.c
  )
  target_link_libraries(mylite_runtime_default_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_default_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_default_function_test)

  add_executable(mylite_runtime_charset_collation_functions_test
    tests/runtime_charset_collation_functions_test.c
  )
  target_link_libraries(mylite_runtime_charset_collation_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_charset_collation_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_charset_collation_functions_test)

  add_executable(mylite_runtime_coercibility_function_test
    tests/runtime_coercibility_function_test.c
  )
  target_link_libraries(mylite_runtime_coercibility_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_coercibility_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_coercibility_function_test)

  add_executable(mylite_runtime_string_length_functions_test
    tests/runtime_string_length_functions_test.c
  )
  target_link_libraries(mylite_runtime_string_length_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_string_length_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_length_functions_test)

  add_executable(mylite_runtime_ascii_ord_functions_test
    tests/runtime_ascii_ord_functions_test.c
  )
  target_link_libraries(mylite_runtime_ascii_ord_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_ascii_ord_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_ascii_ord_functions_test)

  add_executable(mylite_runtime_string_case_functions_test
    tests/runtime_string_case_functions_test.c
  )
  target_link_libraries(mylite_runtime_string_case_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_string_case_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_case_functions_test)

  add_executable(mylite_runtime_concat_ws_function_test
    tests/runtime_concat_ws_function_test.c
  )
  target_link_libraries(mylite_runtime_concat_ws_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_concat_ws_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_concat_ws_function_test)

  add_executable(mylite_runtime_pipes_as_concat_test
    tests/runtime_pipes_as_concat_test.c
  )
  target_link_libraries(mylite_runtime_pipes_as_concat_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_pipes_as_concat_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_pipes_as_concat_test)

  add_executable(mylite_runtime_replace_string_function_test
    tests/runtime_replace_string_function_test.c
  )
  target_link_libraries(mylite_runtime_replace_string_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_replace_string_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_replace_string_function_test)

  add_executable(mylite_runtime_reverse_function_test
    tests/runtime_reverse_function_test.c
  )
  target_link_libraries(mylite_runtime_reverse_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_reverse_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_reverse_function_test)

  add_executable(mylite_runtime_quote_function_test
    tests/runtime_quote_function_test.c
  )
  target_link_libraries(mylite_runtime_quote_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_quote_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_quote_function_test)

  add_executable(mylite_runtime_trim_string_functions_test
    tests/runtime_trim_string_functions_test.c
  )
  target_link_libraries(mylite_runtime_trim_string_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_trim_string_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_trim_string_functions_test)

  add_executable(mylite_runtime_string_slice_functions_test
    tests/runtime_string_slice_functions_test.c
  )
  target_link_libraries(mylite_runtime_string_slice_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_string_slice_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_slice_functions_test)

  add_executable(mylite_runtime_string_padding_functions_test
    tests/runtime_string_padding_functions_test.c
  )
  target_link_libraries(mylite_runtime_string_padding_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_string_padding_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_padding_functions_test)

  add_executable(mylite_runtime_string_search_functions_test
    tests/runtime_string_search_functions_test.c
  )
  target_link_libraries(mylite_runtime_string_search_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_string_search_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_search_functions_test)

  add_executable(mylite_runtime_date_add_second_test
    tests/runtime_date_add_second_test.c
  )
  target_link_libraries(mylite_runtime_date_add_second_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_date_add_second_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_date_add_second_test)

  add_executable(mylite_runtime_addtime_subtime_functions_test
    tests/runtime_addtime_subtime_functions_test.c
  )
  target_link_libraries(mylite_runtime_addtime_subtime_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_addtime_subtime_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_addtime_subtime_functions_test)

  add_executable(mylite_runtime_row_temporal_interval_second_projection_test
    tests/runtime_row_temporal_interval_second_projection_test.c
  )
  target_link_libraries(mylite_runtime_row_temporal_interval_second_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_row_temporal_interval_second_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_row_temporal_interval_second_projection_test)

  add_executable(mylite_runtime_date_format_function_test
    tests/runtime_date_format_function_test.c
  )
  target_link_libraries(mylite_runtime_date_format_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_date_format_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_date_format_function_test)

  add_executable(mylite_runtime_time_format_function_test
    tests/runtime_time_format_function_test.c
  )
  target_link_libraries(mylite_runtime_time_format_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_time_format_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_time_format_function_test)

  add_executable(mylite_runtime_datediff_function_test
    tests/runtime_datediff_function_test.c
  )
  target_link_libraries(mylite_runtime_datediff_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_datediff_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_datediff_function_test)

  add_executable(mylite_runtime_timestampdiff_function_test
    tests/runtime_timestampdiff_function_test.c
  )
  target_link_libraries(mylite_runtime_timestampdiff_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_timestampdiff_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_timestampdiff_function_test)

  add_executable(mylite_runtime_calendar_date_functions_test
    tests/runtime_calendar_date_functions_test.c
  )
  target_link_libraries(mylite_runtime_calendar_date_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_calendar_date_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_calendar_date_functions_test)

  add_executable(mylite_runtime_week_temporal_functions_test
    tests/runtime_week_temporal_functions_test.c
  )
  target_link_libraries(mylite_runtime_week_temporal_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_week_temporal_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_week_temporal_functions_test)

  add_executable(mylite_runtime_unix_timestamp_function_test
    tests/runtime_unix_timestamp_function_test.c
  )
  target_link_libraries(mylite_runtime_unix_timestamp_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_unix_timestamp_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_unix_timestamp_function_test)

  add_executable(mylite_runtime_from_unixtime_function_test
    tests/runtime_from_unixtime_function_test.c
  )
  target_link_libraries(mylite_runtime_from_unixtime_function_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_from_unixtime_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_from_unixtime_function_test)

  add_executable(mylite_runtime_time_second_conversion_functions_test
    tests/runtime_time_second_conversion_functions_test.c
  )
  target_link_libraries(mylite_runtime_time_second_conversion_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_time_second_conversion_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_time_second_conversion_functions_test)

  add_executable(mylite_runtime_temporal_extract_functions_test
    tests/runtime_temporal_extract_functions_test.c
  )
  target_link_libraries(mylite_runtime_temporal_extract_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_temporal_extract_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_temporal_extract_functions_test)

  add_executable(mylite_runtime_field_function_test
    tests/runtime_field_function_test.c
  )
  target_link_libraries(mylite_runtime_field_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_field_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_field_function_test)

  add_executable(mylite_runtime_order_by_field_function_test
    tests/runtime_order_by_field_function_test.c
  )
  target_link_libraries(mylite_runtime_order_by_field_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_order_by_field_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_order_by_field_function_test)

  add_executable(mylite_runtime_greatest_least_functions_test
    tests/runtime_greatest_least_functions_test.c
  )
  target_link_libraries(mylite_runtime_greatest_least_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_greatest_least_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_greatest_least_functions_test)

  add_executable(mylite_runtime_json_valid_function_test
    tests/runtime_json_valid_function_test.c
  )
  target_link_libraries(mylite_runtime_json_valid_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_valid_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_valid_function_test)

  add_executable(mylite_runtime_json_extract_functions_test
    tests/runtime_json_extract_functions_test.c
  )
  target_link_libraries(mylite_runtime_json_extract_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_extract_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_extract_functions_test)

  add_executable(mylite_runtime_json_quote_function_test
    tests/runtime_json_quote_function_test.c
  )
  target_link_libraries(mylite_runtime_json_quote_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_quote_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_quote_function_test)

  add_executable(mylite_runtime_json_contains_functions_test
    tests/runtime_json_contains_functions_test.c
  )
  target_link_libraries(mylite_runtime_json_contains_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_contains_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_contains_functions_test)

  add_executable(mylite_runtime_json_construction_functions_test
    tests/runtime_json_construction_functions_test.c
  )
  target_link_libraries(mylite_runtime_json_construction_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_construction_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_construction_functions_test)

  add_executable(mylite_runtime_json_introspection_functions_test
    tests/runtime_json_introspection_functions_test.c
  )
  target_link_libraries(mylite_runtime_json_introspection_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_introspection_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_introspection_functions_test)

  add_executable(mylite_runtime_scalar_comparison_projection_test
    tests/runtime_scalar_comparison_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_comparison_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_comparison_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_comparison_projection_test)

  add_executable(mylite_runtime_scalar_logical_projection_test
    tests/runtime_scalar_logical_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_logical_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_logical_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_logical_projection_test)

  add_executable(mylite_runtime_scalar_is_projection_test
    tests/runtime_scalar_is_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_is_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_is_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_is_projection_test)

  add_executable(mylite_runtime_session_value_scalar_projection_test
    tests/runtime_session_value_scalar_projection_test.c
  )
  target_link_libraries(mylite_runtime_session_value_scalar_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_session_value_scalar_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_session_value_scalar_projection_test)

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

  add_executable(mylite_runtime_found_rows_function_test
    tests/runtime_found_rows_function_test.c
  )
  target_link_libraries(mylite_runtime_found_rows_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_found_rows_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_found_rows_function_test)

  add_executable(mylite_runtime_select_noop_modifiers_test
    tests/runtime_select_noop_modifiers_test.c
  )
  target_link_libraries(mylite_runtime_select_noop_modifiers_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_select_noop_modifiers_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_select_noop_modifiers_test)

  add_executable(mylite_runtime_select_locking_clauses_test
    tests/runtime_select_locking_clauses_test.c
  )
  target_link_libraries(mylite_runtime_select_locking_clauses_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_select_locking_clauses_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_select_locking_clauses_test)

  add_executable(mylite_runtime_last_insert_id_function_test
    tests/runtime_last_insert_id_function_test.c
  )
  target_link_libraries(mylite_runtime_last_insert_id_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_last_insert_id_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_last_insert_id_function_test)

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

  add_executable(mylite_runtime_min_max_aggregate_test
    tests/runtime_min_max_aggregate_test.c
  )
  target_link_libraries(mylite_runtime_min_max_aggregate_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_min_max_aggregate_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_min_max_aggregate_test)

  add_executable(mylite_runtime_sum_aggregate_test
    tests/runtime_sum_aggregate_test.c
  )
  target_link_libraries(mylite_runtime_sum_aggregate_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_sum_aggregate_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sum_aggregate_test)

  add_executable(mylite_runtime_avg_aggregate_test
    tests/runtime_avg_aggregate_test.c
  )
  target_link_libraries(mylite_runtime_avg_aggregate_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_avg_aggregate_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_avg_aggregate_test)

  add_executable(mylite_runtime_bitwise_aggregates_test
    tests/runtime_bitwise_aggregates_test.c
  )
  target_link_libraries(mylite_runtime_bitwise_aggregates_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_bitwise_aggregates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_bitwise_aggregates_test)

  add_executable(mylite_runtime_group_concat_aggregate_test
    tests/runtime_group_concat_aggregate_test.c
  )
  target_link_libraries(mylite_runtime_group_concat_aggregate_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_group_concat_aggregate_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_group_concat_aggregate_test)

  add_executable(mylite_runtime_group_concat_max_len_system_variable_test
    tests/runtime_group_concat_max_len_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_group_concat_max_len_system_variable_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_group_concat_max_len_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_group_concat_max_len_system_variable_test)

  add_executable(mylite_runtime_group_by_single_column_aggregate_test
    tests/runtime_group_by_single_column_aggregate_test.c
  )
  target_link_libraries(mylite_runtime_group_by_single_column_aggregate_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_group_by_single_column_aggregate_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_group_by_single_column_aggregate_test)

  add_executable(mylite_runtime_group_by_primary_key_projection_test
    tests/runtime_group_by_primary_key_projection_test.c
  )
  target_link_libraries(mylite_runtime_group_by_primary_key_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_group_by_primary_key_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_group_by_primary_key_projection_test)

  add_executable(mylite_runtime_joined_aggregate_select_test
    tests/runtime_joined_aggregate_select_test.c
  )
  target_link_libraries(mylite_runtime_joined_aggregate_select_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_joined_aggregate_select_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_joined_aggregate_select_test)

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

  add_executable(mylite_runtime_show_full_columns_introspection_test
    tests/runtime_show_full_columns_introspection_test.c
  )
  target_link_libraries(mylite_runtime_show_full_columns_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_full_columns_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_full_columns_introspection_test)

  add_executable(mylite_runtime_information_schema_core_test
    tests/runtime_information_schema_core_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_core_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_core_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_core_test)

  add_executable(mylite_runtime_information_schema_predicates_test
    tests/runtime_information_schema_predicates_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_predicates_test)

  add_executable(mylite_runtime_information_schema_write_access_test
    tests/runtime_information_schema_write_access_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_write_access_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_write_access_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_write_access_test)

  add_executable(mylite_runtime_information_schema_constraints_test
    tests/runtime_information_schema_constraints_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_constraints_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_constraints_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_constraints_test)

  add_executable(mylite_runtime_information_schema_check_constraints_test
    tests/runtime_information_schema_check_constraints_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_check_constraints_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_check_constraints_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_check_constraints_test)

  add_executable(mylite_runtime_information_schema_events_test
    tests/runtime_information_schema_events_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_events_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_events_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_events_test)

  add_executable(mylite_runtime_information_schema_parameters_test
    tests/runtime_information_schema_parameters_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_parameters_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_parameters_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_parameters_test)

  add_executable(mylite_runtime_information_schema_partitions_test
    tests/runtime_information_schema_partitions_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_partitions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_partitions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_partitions_test)

  add_executable(mylite_runtime_information_schema_processlist_test
    tests/runtime_information_schema_processlist_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_processlist_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_processlist_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_processlist_test)

  add_executable(mylite_runtime_information_schema_privileges_test
    tests/runtime_information_schema_privileges_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_privileges_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_privileges_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_privileges_test)

  add_executable(mylite_runtime_information_schema_routines_test
    tests/runtime_information_schema_routines_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_routines_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_routines_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_routines_test)

  add_executable(mylite_runtime_information_schema_triggers_test
    tests/runtime_information_schema_triggers_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_triggers_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_triggers_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_triggers_test)

  add_executable(mylite_runtime_information_schema_views_test
    tests/runtime_information_schema_views_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_views_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_views_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_views_test)

  add_executable(mylite_runtime_information_schema_view_table_usage_test
    tests/runtime_information_schema_view_table_usage_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_view_table_usage_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_view_table_usage_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_view_table_usage_test)

  add_executable(mylite_runtime_information_schema_static_catalogs_test
    tests/runtime_information_schema_static_catalogs_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_static_catalogs_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_static_catalogs_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_static_catalogs_test)

  add_executable(mylite_runtime_information_schema_keywords_test
    tests/runtime_information_schema_keywords_test.c
  )
  target_link_libraries(mylite_runtime_information_schema_keywords_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_information_schema_keywords_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_information_schema_keywords_test)

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

  add_executable(mylite_runtime_show_grants_test
    tests/runtime_show_grants_test.c
  )
  target_link_libraries(mylite_runtime_show_grants_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_grants_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_grants_test)

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

  add_executable(mylite_runtime_explicit_defaults_for_timestamp_system_variable_test
    tests/runtime_explicit_defaults_for_timestamp_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_explicit_defaults_for_timestamp_system_variable_test
    PRIVATE
    MyLite::mylite
  )
  target_include_directories(
    mylite_runtime_explicit_defaults_for_timestamp_system_variable_test
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(
    mylite_runtime_explicit_defaults_for_timestamp_system_variable_test
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

  add_executable(mylite_runtime_time_zone_system_variable_test
    tests/runtime_time_zone_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_time_zone_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_time_zone_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_time_zone_system_variable_test)

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

  add_executable(mylite_runtime_max_allowed_packet_system_variable_test
    tests/runtime_max_allowed_packet_system_variable_test.c
  )
  target_link_libraries(mylite_runtime_max_allowed_packet_system_variable_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_max_allowed_packet_system_variable_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_max_allowed_packet_system_variable_test)

  add_executable(mylite_runtime_timeout_system_variables_test
    tests/runtime_timeout_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_timeout_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_timeout_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_timeout_system_variables_test)

  add_executable(mylite_runtime_auto_increment_step_system_variables_test
    tests/runtime_auto_increment_step_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_auto_increment_step_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_auto_increment_step_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_auto_increment_step_system_variables_test)

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

  add_executable(mylite_runtime_set_connection_character_set_test
    tests/runtime_set_connection_character_set_test.c
  )
  target_link_libraries(mylite_runtime_set_connection_character_set_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_set_connection_character_set_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_set_connection_character_set_test)

  add_executable(mylite_runtime_set_fixed_system_variables_test
    tests/runtime_set_fixed_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_set_fixed_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_set_fixed_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_set_fixed_system_variables_test)

  add_executable(mylite_runtime_read_only_system_variables_test
    tests/runtime_read_only_system_variables_test.c
  )
  target_link_libraries(mylite_runtime_read_only_system_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_read_only_system_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_read_only_system_variables_test)

  add_executable(mylite_runtime_user_variables_test
    tests/runtime_user_variables_test.c
  )
  target_link_libraries(mylite_runtime_user_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_user_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_user_variables_test)

  add_executable(mylite_runtime_sql_prepared_statements_test
    tests/runtime_sql_prepared_statements_test.c
  )
  target_link_libraries(mylite_runtime_sql_prepared_statements_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_sql_prepared_statements_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_sql_prepared_statements_test)

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

  add_executable(mylite_runtime_create_table_comment_option_test
    tests/runtime_create_table_comment_option_test.c
  )
  target_link_libraries(mylite_runtime_create_table_comment_option_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_create_table_comment_option_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_create_table_comment_option_test)

  add_executable(mylite_runtime_table_storage_statistics_options_test
    tests/runtime_table_storage_statistics_options_test.c
  )
  target_link_libraries(mylite_runtime_table_storage_statistics_options_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_table_storage_statistics_options_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_table_storage_statistics_options_test)

  add_executable(mylite_runtime_column_comments_test
    tests/runtime_column_comments_test.c
  )
  target_link_libraries(mylite_runtime_column_comments_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_column_comments_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_column_comments_test)

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

  add_executable(mylite_runtime_schema_default_charset_collation_test
    tests/runtime_schema_default_charset_collation_test.c
  )
  target_link_libraries(mylite_runtime_schema_default_charset_collation_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_schema_default_charset_collation_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_schema_default_charset_collation_test)

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

  add_executable(mylite_runtime_show_variables_test
    tests/runtime_show_variables_test.c
  )
  target_link_libraries(mylite_runtime_show_variables_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_variables_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_variables_test)

  add_executable(mylite_runtime_show_status_test
    tests/runtime_show_status_test.c
  )
  target_link_libraries(mylite_runtime_show_status_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_show_status_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_show_status_test)

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

  add_executable(mylite_runtime_wordpress_core_ddl_fixtures_test
    tests/runtime_wordpress_core_ddl_fixtures_test.c
  )
  target_link_libraries(mylite_runtime_wordpress_core_ddl_fixtures_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_wordpress_core_ddl_fixtures_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_wordpress_core_ddl_fixtures_test)

  add_executable(mylite_runtime_wordpress_dbdelta_introspection_test
    tests/runtime_wordpress_dbdelta_introspection_test.c
  )
  target_link_libraries(mylite_runtime_wordpress_dbdelta_introspection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_wordpress_dbdelta_introspection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_wordpress_dbdelta_introspection_test)

  add_executable(mylite_runtime_column_charset_collation_attributes_test
    tests/runtime_column_charset_collation_attributes_test.c
  )
  target_link_libraries(mylite_runtime_column_charset_collation_attributes_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_column_charset_collation_attributes_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_column_charset_collation_attributes_test)

  add_executable(mylite_runtime_alter_table_default_charset_collation_test
    tests/runtime_alter_table_default_charset_collation_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_default_charset_collation_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_alter_table_default_charset_collation_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_default_charset_collation_test)

  add_executable(mylite_runtime_alter_table_convert_character_set_test
    tests/runtime_alter_table_convert_character_set_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_convert_character_set_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_alter_table_convert_character_set_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_convert_character_set_test)

  add_executable(mylite_runtime_alter_table_order_by_test
    tests/runtime_alter_table_order_by_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_order_by_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_order_by_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_order_by_test)

  add_executable(mylite_runtime_alter_table_force_test
    tests/runtime_alter_table_force_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_force_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_force_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_force_test)

  add_executable(mylite_runtime_alter_table_algorithm_lock_clauses_test
    tests/runtime_alter_table_algorithm_lock_clauses_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_algorithm_lock_clauses_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_alter_table_algorithm_lock_clauses_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_algorithm_lock_clauses_test)

  add_executable(mylite_runtime_alter_table_disable_enable_keys_test
    tests/runtime_alter_table_disable_enable_keys_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_disable_enable_keys_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_alter_table_disable_enable_keys_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_disable_enable_keys_test)

  add_executable(mylite_runtime_table_maintenance_test
    tests/runtime_table_maintenance_test.c
  )
  target_link_libraries(mylite_runtime_table_maintenance_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_table_maintenance_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_table_maintenance_test)

  add_executable(mylite_runtime_create_table_like_lifecycle_test
    tests/runtime_create_table_like_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_create_table_like_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_create_table_like_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_create_table_like_lifecycle_test)

  add_executable(mylite_runtime_temporary_table_like_test
    tests/runtime_temporary_table_like_test.c
  )
  target_link_libraries(mylite_runtime_temporary_table_like_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_temporary_table_like_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_temporary_table_like_test)

  add_executable(mylite_runtime_temporary_create_table_select_test
    tests/runtime_temporary_create_table_select_test.c
  )
  target_link_libraries(mylite_runtime_temporary_create_table_select_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_temporary_create_table_select_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_temporary_create_table_select_test)

  add_executable(mylite_runtime_temporary_auto_increment_test
    tests/runtime_temporary_auto_increment_test.c
  )
  target_link_libraries(mylite_runtime_temporary_auto_increment_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_temporary_auto_increment_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_temporary_auto_increment_test)

  add_executable(mylite_runtime_secondary_index_lifecycle_test
    tests/runtime_secondary_index_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_secondary_index_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_secondary_index_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_secondary_index_lifecycle_test)

  add_executable(mylite_runtime_fulltext_index_metadata_test
    tests/runtime_fulltext_index_metadata_test.c
  )
  target_link_libraries(mylite_runtime_fulltext_index_metadata_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_fulltext_index_metadata_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_fulltext_index_metadata_test)

  add_executable(mylite_runtime_spatial_index_metadata_test
    tests/runtime_spatial_index_metadata_test.c
  )
  target_link_libraries(mylite_runtime_spatial_index_metadata_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_spatial_index_metadata_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_spatial_index_metadata_test)

  add_executable(mylite_runtime_index_options_metadata_test
    tests/runtime_index_options_metadata_test.c
  )
  target_link_libraries(mylite_runtime_index_options_metadata_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_index_options_metadata_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_index_options_metadata_test)

  add_executable(mylite_runtime_alter_table_add_index_test
    tests/runtime_alter_table_add_index_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_add_index_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_add_index_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_add_index_test)

  add_executable(mylite_runtime_alter_table_add_unique_test
    tests/runtime_alter_table_add_unique_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_add_unique_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_add_unique_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_add_unique_test)

  add_executable(mylite_runtime_create_index_lifecycle_test
    tests/runtime_create_index_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_create_index_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_create_index_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_create_index_lifecycle_test)

  add_executable(mylite_runtime_alter_table_drop_index_test
    tests/runtime_alter_table_drop_index_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_drop_index_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_drop_index_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_drop_index_test)

  add_executable(mylite_runtime_alter_table_multi_action_test
    tests/runtime_alter_table_multi_action_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_multi_action_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_alter_table_multi_action_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_multi_action_test)

  add_executable(mylite_runtime_drop_index_lifecycle_test
    tests/runtime_drop_index_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_drop_index_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_drop_index_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_drop_index_lifecycle_test)

  add_executable(mylite_runtime_drop_constraint_lifecycle_test
    tests/runtime_drop_constraint_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_drop_constraint_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_drop_constraint_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_drop_constraint_lifecycle_test)

  add_executable(mylite_runtime_rename_index_lifecycle_test
    tests/runtime_rename_index_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_rename_index_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_rename_index_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_rename_index_lifecycle_test)

  add_executable(mylite_runtime_alter_index_visibility_test
    tests/runtime_alter_index_visibility_test.c
  )
  target_link_libraries(mylite_runtime_alter_index_visibility_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_index_visibility_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_index_visibility_test)

  add_executable(mylite_runtime_unique_index_lifecycle_test
    tests/runtime_unique_index_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_unique_index_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_unique_index_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_unique_index_lifecycle_test)

  add_executable(mylite_runtime_named_unique_constraint_test
    tests/runtime_named_unique_constraint_test.c
  )
  target_link_libraries(mylite_runtime_named_unique_constraint_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_named_unique_constraint_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_named_unique_constraint_test)

  add_executable(mylite_runtime_unique_prefix_index_lifecycle_test
    tests/runtime_unique_prefix_index_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_unique_prefix_index_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_unique_prefix_index_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_unique_prefix_index_lifecycle_test)

  add_executable(mylite_runtime_binary_full_column_index_test
    tests/runtime_binary_full_column_index_test.c
  )
  target_link_libraries(mylite_runtime_binary_full_column_index_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_binary_full_column_index_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_binary_full_column_index_test)

  add_executable(mylite_runtime_descending_index_key_parts_test
    tests/runtime_descending_index_key_parts_test.c
  )
  target_link_libraries(mylite_runtime_descending_index_key_parts_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_descending_index_key_parts_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_descending_index_key_parts_test)

  add_executable(mylite_runtime_foreign_key_constraints_test
    tests/runtime_foreign_key_constraints_test.c
  )
  target_link_libraries(mylite_runtime_foreign_key_constraints_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_foreign_key_constraints_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_foreign_key_constraints_test)

  add_executable(mylite_runtime_check_constraint_lifecycle_test
    tests/runtime_check_constraint_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_check_constraint_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_check_constraint_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_check_constraint_lifecycle_test)

  add_executable(mylite_runtime_alter_check_constraint_lifecycle_test
    tests/runtime_alter_check_constraint_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_alter_check_constraint_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_alter_check_constraint_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_check_constraint_lifecycle_test)

  add_executable(mylite_runtime_char_varchar_key_lifecycle_test
    tests/runtime_char_varchar_key_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_char_varchar_key_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_char_varchar_key_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_char_varchar_key_lifecycle_test)

  add_executable(mylite_runtime_composite_string_primary_key_test
    tests/runtime_composite_string_primary_key_test.c
  )
  target_link_libraries(mylite_runtime_composite_string_primary_key_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_composite_string_primary_key_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_composite_string_primary_key_test)

  add_executable(mylite_runtime_create_table_select_lifecycle_test
    tests/runtime_create_table_select_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_create_table_select_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_create_table_select_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_create_table_select_lifecycle_test)

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

  add_executable(mylite_runtime_temporary_table_lifecycle_test
    tests/runtime_temporary_table_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_temporary_table_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_temporary_table_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_temporary_table_lifecycle_test)

  add_executable(mylite_runtime_temporary_index_lifecycle_test
    tests/runtime_temporary_index_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_temporary_index_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_temporary_index_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_temporary_index_lifecycle_test)

  add_executable(mylite_runtime_lock_tables_lifecycle_test
    tests/runtime_lock_tables_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_lock_tables_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_lock_tables_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_lock_tables_lifecycle_test)

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

  add_executable(mylite_runtime_replace_values_lifecycle_test
    tests/runtime_replace_values_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_replace_values_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_replace_values_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_replace_values_lifecycle_test)

  add_executable(mylite_runtime_replace_set_lifecycle_test
    tests/runtime_replace_set_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_replace_set_lifecycle_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_replace_set_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_replace_set_lifecycle_test)

  add_executable(mylite_runtime_replace_key_lifecycle_test
    tests/runtime_replace_key_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_replace_key_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_replace_key_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_replace_key_lifecycle_test)

  add_executable(mylite_runtime_replace_select_lifecycle_test
    tests/runtime_replace_select_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_replace_select_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_replace_select_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_replace_select_lifecycle_test)

  add_executable(mylite_runtime_replace_modifier_lifecycle_test
    tests/runtime_replace_modifier_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_replace_modifier_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_replace_modifier_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_replace_modifier_lifecycle_test)

  add_executable(mylite_runtime_insert_modifier_lifecycle_test
    tests/runtime_insert_modifier_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_insert_modifier_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_insert_modifier_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_insert_modifier_lifecycle_test)

  add_executable(mylite_runtime_insert_ignore_lifecycle_test
    tests/runtime_insert_ignore_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_insert_ignore_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_insert_ignore_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_insert_ignore_lifecycle_test)

  add_executable(mylite_runtime_nonstrict_dml_coercion_test
    tests/runtime_nonstrict_dml_coercion_test.c
  )
  target_link_libraries(mylite_runtime_nonstrict_dml_coercion_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_nonstrict_dml_coercion_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_nonstrict_dml_coercion_test)

  add_executable(mylite_runtime_dml_string_numeric_coercion_test
    tests/runtime_dml_string_numeric_coercion_test.c
  )
  target_link_libraries(mylite_runtime_dml_string_numeric_coercion_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_dml_string_numeric_coercion_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_dml_string_numeric_coercion_test)

  add_executable(mylite_runtime_insert_on_duplicate_key_update_test
    tests/runtime_insert_on_duplicate_key_update_test.c
  )
  target_link_libraries(mylite_runtime_insert_on_duplicate_key_update_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_insert_on_duplicate_key_update_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_insert_on_duplicate_key_update_test)

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

  add_executable(mylite_runtime_exists_subquery_predicates_test
    tests/runtime_exists_subquery_predicates_test.c
  )
  target_link_libraries(mylite_runtime_exists_subquery_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_exists_subquery_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_exists_subquery_predicates_test)

  add_executable(mylite_runtime_in_subquery_predicates_test
    tests/runtime_in_subquery_predicates_test.c
  )
  target_link_libraries(mylite_runtime_in_subquery_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_in_subquery_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_in_subquery_predicates_test)

  add_executable(mylite_runtime_where_and_predicates_test
    tests/runtime_where_and_predicates_test.c
  )
  target_link_libraries(mylite_runtime_where_and_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_where_and_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_where_and_predicates_test)

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

  add_executable(mylite_runtime_select_distinct_rowsets_test
    tests/runtime_select_distinct_rowsets_test.c
  )
  target_link_libraries(mylite_runtime_select_distinct_rowsets_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_select_distinct_rowsets_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_select_distinct_rowsets_test)

  add_executable(mylite_runtime_table_statement_test
    tests/runtime_table_statement_test.c
  )
  target_link_libraries(mylite_runtime_table_statement_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_table_statement_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_table_statement_test)

  add_executable(mylite_runtime_select_qualified_columns_test
    tests/runtime_select_qualified_columns_test.c
  )
  target_link_libraries(mylite_runtime_select_qualified_columns_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_select_qualified_columns_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_select_qualified_columns_test)

  add_executable(mylite_runtime_union_select_lifecycle_test
    tests/runtime_union_select_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_union_select_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_union_select_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_union_select_lifecycle_test)

  add_executable(mylite_runtime_inner_join_select_test
    tests/runtime_inner_join_select_test.c
  )
  target_link_libraries(mylite_runtime_inner_join_select_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_inner_join_select_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_inner_join_select_test)

  add_executable(mylite_runtime_left_join_select_test
    tests/runtime_left_join_select_test.c
  )
  target_link_libraries(mylite_runtime_left_join_select_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_left_join_select_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_left_join_select_test)

  add_executable(mylite_runtime_right_join_select_test
    tests/runtime_right_join_select_test.c
  )
  target_link_libraries(mylite_runtime_right_join_select_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_right_join_select_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_right_join_select_test)

  add_executable(mylite_runtime_select_item_alias_test
    tests/runtime_select_item_alias_test.c
  )
  target_link_libraries(mylite_runtime_select_item_alias_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_select_item_alias_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_select_item_alias_test)

  add_executable(mylite_runtime_select_literal_projection_test
    tests/runtime_select_literal_projection_test.c
  )
  target_link_libraries(mylite_runtime_select_literal_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_select_literal_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_select_literal_projection_test)

  add_executable(mylite_runtime_row_scalar_expressions_test
    tests/runtime_row_scalar_expressions_test.c
  )
  target_link_libraries(mylite_runtime_row_scalar_expressions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_row_scalar_expressions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_row_scalar_expressions_test)

  add_executable(mylite_runtime_scalar_subquery_projection_test
    tests/runtime_scalar_subquery_projection_test.c
  )
  target_link_libraries(mylite_runtime_scalar_subquery_projection_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_scalar_subquery_projection_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_scalar_subquery_projection_test)

  add_executable(mylite_runtime_string_equality_predicates_test
    tests/runtime_string_equality_predicates_test.c
  )
  target_link_libraries(mylite_runtime_string_equality_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_string_equality_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_equality_predicates_test)

  add_executable(mylite_runtime_string_order_lifecycle_test
    tests/runtime_string_order_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_string_order_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_string_order_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_order_lifecycle_test)

  add_executable(mylite_runtime_string_range_predicates_test
    tests/runtime_string_range_predicates_test.c
  )
  target_link_libraries(mylite_runtime_string_range_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_string_range_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_range_predicates_test)

  add_executable(mylite_runtime_like_predicates_test
    tests/runtime_like_predicates_test.c
  )
  target_link_libraries(mylite_runtime_like_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_like_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_like_predicates_test)

  add_executable(mylite_runtime_regexp_rlike_predicates_test
    tests/runtime_regexp_rlike_predicates_test.c
  )
  target_link_libraries(mylite_runtime_regexp_rlike_predicates_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_regexp_rlike_predicates_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_regexp_rlike_predicates_test)

  add_executable(mylite_runtime_regexp_like_function_test
    tests/runtime_regexp_like_function_test.c
  )
  target_link_libraries(mylite_runtime_regexp_like_function_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_regexp_like_function_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_regexp_like_function_test)

  add_executable(mylite_runtime_view_lifecycle_test
    tests/runtime_view_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_view_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_view_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_view_lifecycle_test)

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

  add_executable(mylite_runtime_joined_delete_lifecycle_test
    tests/runtime_joined_delete_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_joined_delete_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_joined_delete_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_joined_delete_lifecycle_test)

  add_executable(mylite_runtime_joined_update_lifecycle_test
    tests/runtime_joined_update_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_joined_update_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_joined_update_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_joined_update_lifecycle_test)

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

  add_executable(mylite_runtime_update_index_hints_noop_test
    tests/runtime_update_index_hints_noop_test.c
  )
  target_link_libraries(mylite_runtime_update_index_hints_noop_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_update_index_hints_noop_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_update_index_hints_noop_test)

  add_executable(mylite_runtime_update_scalar_subquery_assignment_test
    tests/runtime_update_scalar_subquery_assignment_test.c
  )
  target_link_libraries(mylite_runtime_update_scalar_subquery_assignment_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_update_scalar_subquery_assignment_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_update_scalar_subquery_assignment_test)

  add_executable(mylite_runtime_update_arithmetic_assignment_test
    tests/runtime_update_arithmetic_assignment_test.c
  )
  target_link_libraries(mylite_runtime_update_arithmetic_assignment_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_update_arithmetic_assignment_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_update_arithmetic_assignment_test)

  add_executable(mylite_runtime_update_multiple_assignments_test
    tests/runtime_update_multiple_assignments_test.c
  )
  target_link_libraries(mylite_runtime_update_multiple_assignments_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_update_multiple_assignments_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_update_multiple_assignments_test)

  add_executable(mylite_runtime_transaction_lifecycle_test
    tests/runtime_transaction_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_transaction_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_transaction_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_transaction_lifecycle_test)

  add_executable(mylite_runtime_dml_default_keyword_values_test
    tests/runtime_dml_default_keyword_values_test.c
  )
  target_link_libraries(mylite_runtime_dml_default_keyword_values_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_dml_default_keyword_values_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_dml_default_keyword_values_test)

  add_executable(mylite_runtime_alter_column_visibility_test
    tests/runtime_alter_column_visibility_test.c
  )
  target_link_libraries(mylite_runtime_alter_column_visibility_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_column_visibility_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_column_visibility_test)

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

  add_executable(mylite_runtime_varchar_type_test
    tests/runtime_varchar_type_test.c
  )
  target_link_libraries(mylite_runtime_varchar_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_varchar_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_varchar_type_test)

  add_executable(mylite_runtime_enum_type_test
    tests/runtime_enum_type_test.c
  )
  target_link_libraries(mylite_runtime_enum_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_enum_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_enum_type_test)

  add_executable(mylite_runtime_set_type_test
    tests/runtime_set_type_test.c
  )
  target_link_libraries(mylite_runtime_set_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_set_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_set_type_test)

  add_executable(mylite_runtime_char_type_test
    tests/runtime_char_type_test.c
  )
  target_link_libraries(mylite_runtime_char_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_char_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_char_type_test)

  add_executable(mylite_runtime_character_alias_lifecycle_test
    tests/runtime_character_alias_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_character_alias_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_character_alias_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_character_alias_lifecycle_test)

  add_executable(mylite_runtime_text_type_test
    tests/runtime_text_type_test.c
  )
  target_link_libraries(mylite_runtime_text_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_text_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_text_type_test)

  add_executable(mylite_runtime_json_type_test
    tests/runtime_json_type_test.c
  )
  target_link_libraries(mylite_runtime_json_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_json_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_json_type_test)

  add_executable(mylite_runtime_binary_string_types_test
    tests/runtime_binary_string_types_test.c
  )
  target_link_libraries(mylite_runtime_binary_string_types_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_binary_string_types_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_binary_string_types_test)

  add_executable(mylite_runtime_long_character_binary_aliases_test
    tests/runtime_long_character_binary_aliases_test.c
  )
  target_link_libraries(mylite_runtime_long_character_binary_aliases_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_long_character_binary_aliases_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_long_character_binary_aliases_test)

  add_executable(mylite_runtime_bit_type_test
    tests/runtime_bit_type_test.c
  )
  target_link_libraries(mylite_runtime_bit_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_bit_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_bit_type_test)

  add_executable(mylite_runtime_string_defaults_test
    tests/runtime_string_defaults_test.c
  )
  target_link_libraries(mylite_runtime_string_defaults_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_string_defaults_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_string_defaults_test)

  add_executable(mylite_runtime_decimal_type_test
    tests/runtime_decimal_type_test.c
  )
  target_link_libraries(mylite_runtime_decimal_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_decimal_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_decimal_type_test)

  add_executable(mylite_runtime_float_double_type_test
    tests/runtime_float_double_type_test.c
  )
  target_link_libraries(mylite_runtime_float_double_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_float_double_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_float_double_type_test)

  add_executable(mylite_runtime_date_type_test
    tests/runtime_date_type_test.c
  )
  target_link_libraries(mylite_runtime_date_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_date_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_date_type_test)

  add_executable(mylite_runtime_year_type_test
    tests/runtime_year_type_test.c
  )
  target_link_libraries(mylite_runtime_year_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_year_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_year_type_test)

  add_executable(mylite_runtime_time_type_test
    tests/runtime_time_type_test.c
  )
  target_link_libraries(mylite_runtime_time_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_time_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_time_type_test)

  add_executable(mylite_runtime_datetime_type_test
    tests/runtime_datetime_type_test.c
  )
  target_link_libraries(mylite_runtime_datetime_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_datetime_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_datetime_type_test)

  add_executable(mylite_runtime_timestamp_type_test
    tests/runtime_timestamp_type_test.c
  )
  target_link_libraries(mylite_runtime_timestamp_type_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_timestamp_type_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_timestamp_type_test)

  add_executable(mylite_runtime_zero_temporal_sql_modes_test
    tests/runtime_zero_temporal_sql_modes_test.c
  )
  target_link_libraries(mylite_runtime_zero_temporal_sql_modes_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_zero_temporal_sql_modes_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_zero_temporal_sql_modes_test)

  add_executable(mylite_runtime_current_timestamp_defaults_test
    tests/runtime_current_timestamp_defaults_test.c
  )
  target_link_libraries(mylite_runtime_current_timestamp_defaults_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_current_timestamp_defaults_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_current_timestamp_defaults_test)

  add_executable(mylite_runtime_current_date_time_defaults_test
    tests/runtime_current_date_time_defaults_test.c
  )
  target_link_libraries(mylite_runtime_current_date_time_defaults_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_current_date_time_defaults_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_current_date_time_defaults_test)

  add_executable(mylite_runtime_current_date_time_functions_test
    tests/runtime_current_date_time_functions_test.c
  )
  target_link_libraries(mylite_runtime_current_date_time_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_current_date_time_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_current_date_time_functions_test)

  add_executable(mylite_runtime_utc_date_time_functions_test
    tests/runtime_utc_date_time_functions_test.c
  )
  target_link_libraries(mylite_runtime_utc_date_time_functions_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_utc_date_time_functions_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_utc_date_time_functions_test)

  add_executable(mylite_runtime_primary_key_lifecycle_test
    tests/runtime_primary_key_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_primary_key_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_primary_key_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_primary_key_lifecycle_test)

  add_executable(mylite_runtime_alter_table_add_primary_key_test
    tests/runtime_alter_table_add_primary_key_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_add_primary_key_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_add_primary_key_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_add_primary_key_test)

  add_executable(mylite_runtime_alter_table_drop_primary_key_test
    tests/runtime_alter_table_drop_primary_key_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_drop_primary_key_test PRIVATE
    MyLite::mylite
    MyLite::sqlite
  )
  target_include_directories(mylite_runtime_alter_table_drop_primary_key_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_drop_primary_key_test)

  add_executable(mylite_runtime_alter_table_auto_increment_option_test
    tests/runtime_alter_table_auto_increment_option_test.c
  )
  target_link_libraries(mylite_runtime_alter_table_auto_increment_option_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_alter_table_auto_increment_option_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_alter_table_auto_increment_option_test)

  add_executable(mylite_runtime_auto_increment_lifecycle_test
    tests/runtime_auto_increment_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_auto_increment_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_auto_increment_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_auto_increment_lifecycle_test)

  add_executable(mylite_runtime_serial_alias_lifecycle_test
    tests/runtime_serial_alias_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_serial_alias_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_serial_alias_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_serial_alias_lifecycle_test)

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

  add_executable(mylite_runtime_generated_column_lifecycle_test
    tests/runtime_generated_column_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_generated_column_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_generated_column_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_generated_column_lifecycle_test)

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

  add_executable(mylite_runtime_insert_select_lifecycle_test
    tests/runtime_insert_select_lifecycle_test.c
  )
  target_link_libraries(mylite_runtime_insert_select_lifecycle_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_insert_select_lifecycle_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_insert_select_lifecycle_test)

  add_executable(mylite_runtime_insert_select_union_source_test
    tests/runtime_insert_select_union_source_test.c
  )
  target_link_libraries(mylite_runtime_insert_select_union_source_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_insert_select_union_source_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_insert_select_union_source_test)

  add_executable(mylite_runtime_insert_select_on_duplicate_key_update_test
    tests/runtime_insert_select_on_duplicate_key_update_test.c
  )
  target_link_libraries(mylite_runtime_insert_select_on_duplicate_key_update_test PRIVATE
    MyLite::mylite
  )
  target_include_directories(mylite_runtime_insert_select_on_duplicate_key_update_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_insert_select_on_duplicate_key_update_test)

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

  add_executable(mylite_runtime_result_column_metadata_test
    tests/runtime_result_column_metadata_test.c
  )
  target_link_libraries(mylite_runtime_result_column_metadata_test PRIVATE MyLite::mylite)
  target_include_directories(mylite_runtime_result_column_metadata_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_runtime_result_column_metadata_test)

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
    NAME libmylite.runtime.if_function
    COMMAND mylite_runtime_if_function_test
  )
  add_test(
    NAME libmylite.runtime.ifnull_function
    COMMAND mylite_runtime_ifnull_function_test
  )
  add_test(
    NAME libmylite.runtime.coalesce_function
    COMMAND mylite_runtime_coalesce_function_test
  )
  add_test(
    NAME libmylite.runtime.nullif_function
    COMMAND mylite_runtime_nullif_function_test
  )
  add_test(
    NAME libmylite.runtime.isnull_function
    COMMAND mylite_runtime_isnull_function_test
  )
  add_test(
    NAME libmylite.runtime.case_operator
    COMMAND mylite_runtime_case_operator_test
  )
  add_test(
    NAME libmylite.runtime.do_statement
    COMMAND mylite_runtime_do_statement_test
  )
  add_test(
    NAME libmylite.runtime.scalar_expression_projection
    COMMAND mylite_runtime_scalar_expression_projection_test
  )
  add_test(
    NAME libmylite.runtime.scalar_arithmetic_projection
    COMMAND mylite_runtime_scalar_arithmetic_projection_test
  )
  add_test(
    NAME libmylite.runtime.scalar_modulo_projection
    COMMAND mylite_runtime_scalar_modulo_projection_test
  )
  add_test(
    NAME libmylite.runtime.scalar_div_projection
    COMMAND mylite_runtime_scalar_div_projection_test
  )
  add_test(
    NAME libmylite.runtime.scalar_division_projection
    COMMAND mylite_runtime_scalar_division_projection_test
  )
  add_test(
    NAME libmylite.runtime.scalar_bitwise_projection
    COMMAND mylite_runtime_scalar_bitwise_projection_test
  )
  add_test(
    NAME libmylite.runtime.abs_function
    COMMAND mylite_runtime_abs_function_test
  )
  add_test(
    NAME libmylite.runtime.sign_function
    COMMAND mylite_runtime_sign_function_test
  )
  add_test(
    NAME libmylite.runtime.ceil_floor_functions
    COMMAND mylite_runtime_ceil_floor_functions_test
  )
  add_test(
    NAME libmylite.runtime.round_function
    COMMAND mylite_runtime_round_function_test
  )
  add_test(
    NAME libmylite.runtime.bin_oct_functions
    COMMAND mylite_runtime_bin_oct_functions_test
  )
  add_test(
    NAME libmylite.runtime.conv_function
    COMMAND mylite_runtime_conv_function_test
  )
  add_test(
    NAME libmylite.runtime.pi_function
    COMMAND mylite_runtime_pi_function_test
  )
  add_test(
    NAME libmylite.runtime.rand_function
    COMMAND mylite_runtime_rand_function_test
  )
  add_test(
    NAME libmylite.runtime.sqrt_function
    COMMAND mylite_runtime_sqrt_function_test
  )
  add_test(
    NAME libmylite.runtime.degrees_radians_functions
    COMMAND mylite_runtime_degrees_radians_functions_test
  )
  add_test(
    NAME libmylite.runtime.acos_asin_functions
    COMMAND mylite_runtime_acos_asin_functions_test
  )
  add_test(
    NAME libmylite.runtime.atan_functions
    COMMAND mylite_runtime_atan_functions_test
  )
  add_test(
    NAME libmylite.runtime.trigonometric_functions
    COMMAND mylite_runtime_trigonometric_functions_test
  )
  add_test(
    NAME libmylite.runtime.exp_log_power_functions
    COMMAND mylite_runtime_exp_log_power_functions_test
  )
  add_test(
    NAME libmylite.runtime.bit_count_function
    COMMAND mylite_runtime_bit_count_function_test
  )
  add_test(
    NAME libmylite.runtime.numeric_format_truncate_crc32
    COMMAND mylite_runtime_numeric_format_truncate_crc32_test
  )
  add_test(
    NAME libmylite.runtime.hex_function
    COMMAND mylite_runtime_hex_function_test
  )
  add_test(
    NAME libmylite.runtime.unhex_function
    COMMAND mylite_runtime_unhex_function_test
  )
  add_test(
    NAME libmylite.runtime.uuid_conversion_functions
    COMMAND mylite_runtime_uuid_conversion_functions_test
  )
  add_test(
    NAME libmylite.runtime.char_function
    COMMAND mylite_runtime_char_function_test
  )
  add_test(
    NAME libmylite.runtime.default_function
    COMMAND mylite_runtime_default_function_test
  )
  add_test(
    NAME libmylite.runtime.charset_collation_functions
    COMMAND mylite_runtime_charset_collation_functions_test
  )
  add_test(
    NAME libmylite.runtime.coercibility_function
    COMMAND mylite_runtime_coercibility_function_test
  )
  add_test(
    NAME libmylite.runtime.string_length_functions
    COMMAND mylite_runtime_string_length_functions_test
  )
  add_test(
    NAME libmylite.runtime.ascii_ord_functions
    COMMAND mylite_runtime_ascii_ord_functions_test
  )
  add_test(
    NAME libmylite.runtime.string_case_functions
    COMMAND mylite_runtime_string_case_functions_test
  )
  add_test(
    NAME libmylite.runtime.concat_ws_function
    COMMAND mylite_runtime_concat_ws_function_test
  )
  add_test(
    NAME libmylite.runtime.pipes_as_concat
    COMMAND mylite_runtime_pipes_as_concat_test
  )
  add_test(
    NAME libmylite.runtime.replace_string_function
    COMMAND mylite_runtime_replace_string_function_test
  )
  add_test(
    NAME libmylite.runtime.reverse_function
    COMMAND mylite_runtime_reverse_function_test
  )
  add_test(
    NAME libmylite.runtime.quote_function
    COMMAND mylite_runtime_quote_function_test
  )
  add_test(
    NAME libmylite.runtime.trim_string_functions
    COMMAND mylite_runtime_trim_string_functions_test
  )
  add_test(
    NAME libmylite.runtime.string_slice_functions
    COMMAND mylite_runtime_string_slice_functions_test
  )
  add_test(
    NAME libmylite.runtime.string_padding_functions
    COMMAND mylite_runtime_string_padding_functions_test
  )
  add_test(
    NAME libmylite.runtime.string_search_functions
    COMMAND mylite_runtime_string_search_functions_test
  )
  add_test(
    NAME libmylite.runtime.date_add_second
    COMMAND mylite_runtime_date_add_second_test
  )
  add_test(
    NAME libmylite.runtime.addtime_subtime_functions
    COMMAND mylite_runtime_addtime_subtime_functions_test
  )
  add_test(
    NAME libmylite.runtime.row_temporal_interval_second_projection
    COMMAND mylite_runtime_row_temporal_interval_second_projection_test
  )
  add_test(
    NAME libmylite.runtime.date_format_function
    COMMAND mylite_runtime_date_format_function_test
  )
  add_test(
    NAME libmylite.runtime.time_format_function
    COMMAND mylite_runtime_time_format_function_test
  )
  add_test(
    NAME libmylite.runtime.datediff_function
    COMMAND mylite_runtime_datediff_function_test
  )
  add_test(
    NAME libmylite.runtime.timestampdiff_function
    COMMAND mylite_runtime_timestampdiff_function_test
  )
  add_test(
    NAME libmylite.runtime.calendar_date_functions
    COMMAND mylite_runtime_calendar_date_functions_test
  )
  add_test(
    NAME libmylite.runtime.week_temporal_functions
    COMMAND mylite_runtime_week_temporal_functions_test
  )
  add_test(
    NAME libmylite.runtime.unix_timestamp_function
    COMMAND mylite_runtime_unix_timestamp_function_test
  )
  add_test(
    NAME libmylite.runtime.from_unixtime_function
    COMMAND mylite_runtime_from_unixtime_function_test
  )
  add_test(
    NAME libmylite.runtime.time_second_conversion_functions
    COMMAND mylite_runtime_time_second_conversion_functions_test
  )

  add_test(
    NAME libmylite.runtime.temporal_extract_functions
    COMMAND mylite_runtime_temporal_extract_functions_test
  )
  add_test(
    NAME libmylite.runtime.field_function
    COMMAND mylite_runtime_field_function_test
  )
  add_test(
    NAME libmylite.runtime.order_by_field_function
    COMMAND mylite_runtime_order_by_field_function_test
  )
  add_test(
    NAME libmylite.runtime.greatest_least_functions
    COMMAND mylite_runtime_greatest_least_functions_test
  )
  add_test(
    NAME libmylite.runtime.json_valid_function
    COMMAND mylite_runtime_json_valid_function_test
  )
  add_test(
    NAME libmylite.runtime.json_extract_functions
    COMMAND mylite_runtime_json_extract_functions_test
  )
  add_test(
    NAME libmylite.runtime.json_quote_function
    COMMAND mylite_runtime_json_quote_function_test
  )
  add_test(
    NAME libmylite.runtime.json_contains_functions
    COMMAND mylite_runtime_json_contains_functions_test
  )
  add_test(
    NAME libmylite.runtime.json_construction_functions
    COMMAND mylite_runtime_json_construction_functions_test
  )
  add_test(
    NAME libmylite.runtime.json_introspection_functions
    COMMAND mylite_runtime_json_introspection_functions_test
  )
  add_test(
    NAME libmylite.runtime.scalar_comparison_projection
    COMMAND mylite_runtime_scalar_comparison_projection_test
  )
  add_test(
    NAME libmylite.runtime.scalar_logical_projection
    COMMAND mylite_runtime_scalar_logical_projection_test
  )
  add_test(
    NAME libmylite.runtime.scalar_is_projection
    COMMAND mylite_runtime_scalar_is_projection_test
  )
  add_test(
    NAME libmylite.runtime.session_value_scalar_projection
    COMMAND mylite_runtime_session_value_scalar_projection_test
  )
  add_test(
    NAME libmylite.runtime.row_count_function
    COMMAND mylite_runtime_row_count_function_test
  )
  add_test(
    NAME libmylite.runtime.found_rows_function
    COMMAND mylite_runtime_found_rows_function_test
  )
  add_test(
    NAME libmylite.runtime.select_noop_modifiers
    COMMAND mylite_runtime_select_noop_modifiers_test
  )
  add_test(
    NAME libmylite.runtime.select_locking_clauses
    COMMAND mylite_runtime_select_locking_clauses_test
  )
  add_test(
    NAME libmylite.runtime.last_insert_id_function
    COMMAND mylite_runtime_last_insert_id_function_test
  )
  add_test(
    NAME libmylite.runtime.count_aggregate
    COMMAND mylite_runtime_count_aggregate_test
  )
  add_test(
    NAME libmylite.runtime.min_max_aggregate
    COMMAND mylite_runtime_min_max_aggregate_test
  )
  add_test(
    NAME libmylite.runtime.sum_aggregate
    COMMAND mylite_runtime_sum_aggregate_test
  )
  add_test(
    NAME libmylite.runtime.avg_aggregate
    COMMAND mylite_runtime_avg_aggregate_test
  )
  add_test(
    NAME libmylite.runtime.bitwise_aggregates
    COMMAND mylite_runtime_bitwise_aggregates_test
  )
  add_test(
    NAME libmylite.runtime.group_concat_aggregate
    COMMAND mylite_runtime_group_concat_aggregate_test
  )
  add_test(
    NAME libmylite.runtime.group_concat_max_len_system_variable
    COMMAND mylite_runtime_group_concat_max_len_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.group_by_single_column_aggregate
    COMMAND mylite_runtime_group_by_single_column_aggregate_test
  )
  add_test(
    NAME libmylite.runtime.group_by_primary_key_projection
    COMMAND mylite_runtime_group_by_primary_key_projection_test
  )
  add_test(
    NAME libmylite.runtime.joined_aggregate_select
    COMMAND mylite_runtime_joined_aggregate_select_test
  )
  add_test(
    NAME libmylite.runtime.show_columns_introspection
    COMMAND mylite_runtime_show_columns_introspection_test
  )
  add_test(
    NAME libmylite.runtime.show_full_columns_introspection
    COMMAND mylite_runtime_show_full_columns_introspection_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_core
    COMMAND mylite_runtime_information_schema_core_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_predicates
    COMMAND mylite_runtime_information_schema_predicates_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_write_access
    COMMAND mylite_runtime_information_schema_write_access_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_constraints
    COMMAND mylite_runtime_information_schema_constraints_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_check_constraints
    COMMAND mylite_runtime_information_schema_check_constraints_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_events
    COMMAND mylite_runtime_information_schema_events_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_parameters
    COMMAND mylite_runtime_information_schema_parameters_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_partitions
    COMMAND mylite_runtime_information_schema_partitions_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_processlist
    COMMAND mylite_runtime_information_schema_processlist_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_privileges
    COMMAND mylite_runtime_information_schema_privileges_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_routines
    COMMAND mylite_runtime_information_schema_routines_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_triggers
    COMMAND mylite_runtime_information_schema_triggers_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_views
    COMMAND mylite_runtime_information_schema_views_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_view_table_usage
    COMMAND mylite_runtime_information_schema_view_table_usage_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_static_catalogs
    COMMAND mylite_runtime_information_schema_static_catalogs_test
  )
  add_test(
    NAME libmylite.runtime.information_schema_keywords
    COMMAND mylite_runtime_information_schema_keywords_test
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
    NAME libmylite.runtime.show_grants
    COMMAND mylite_runtime_show_grants_test
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
    NAME libmylite.runtime.explicit_defaults_for_timestamp_system_variable
    COMMAND mylite_runtime_explicit_defaults_for_timestamp_system_variable_test
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
    NAME libmylite.runtime.time_zone_system_variable
    COMMAND mylite_runtime_time_zone_system_variable_test
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
    NAME libmylite.runtime.max_allowed_packet_system_variable
    COMMAND mylite_runtime_max_allowed_packet_system_variable_test
  )
  add_test(
    NAME libmylite.runtime.timeout_system_variables
    COMMAND mylite_runtime_timeout_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.auto_increment_step_system_variables
    COMMAND mylite_runtime_auto_increment_step_system_variables_test
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
    NAME libmylite.runtime.set_connection_character_set
    COMMAND mylite_runtime_set_connection_character_set_test
  )
  add_test(
    NAME libmylite.runtime.set_fixed_system_variables
    COMMAND mylite_runtime_set_fixed_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.read_only_system_variables
    COMMAND mylite_runtime_read_only_system_variables_test
  )
  add_test(
    NAME libmylite.runtime.user_variables
    COMMAND mylite_runtime_user_variables_test
  )
  add_test(
    NAME libmylite.runtime.sql_prepared_statements
    COMMAND mylite_runtime_sql_prepared_statements_test
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
    NAME libmylite.runtime.create_table_comment_option
    COMMAND mylite_runtime_create_table_comment_option_test
  )
  add_test(
    NAME libmylite.runtime.table_storage_statistics_options
    COMMAND mylite_runtime_table_storage_statistics_options_test
  )
  add_test(
    NAME libmylite.runtime.column_comments
    COMMAND mylite_runtime_column_comments_test
  )
  add_test(
    NAME libmylite.runtime.show_create_database
    COMMAND mylite_runtime_show_create_database_test
  )
  add_test(
    NAME libmylite.runtime.schema_default_charset_collation
    COMMAND mylite_runtime_schema_default_charset_collation_test
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
    NAME libmylite.runtime.show_variables
    COMMAND mylite_runtime_show_variables_test
  )
  add_test(
    NAME libmylite.runtime.show_status
    COMMAND mylite_runtime_show_status_test
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
    NAME libmylite.runtime.wordpress_core_ddl_fixtures
    COMMAND mylite_runtime_wordpress_core_ddl_fixtures_test
  )
  add_test(
    NAME libmylite.runtime.wordpress_dbdelta_introspection
    COMMAND mylite_runtime_wordpress_dbdelta_introspection_test
  )
  add_test(
    NAME libmylite.runtime.column_charset_collation_attributes
    COMMAND mylite_runtime_column_charset_collation_attributes_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_default_charset_collation
    COMMAND mylite_runtime_alter_table_default_charset_collation_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_convert_character_set
    COMMAND mylite_runtime_alter_table_convert_character_set_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_order_by
    COMMAND mylite_runtime_alter_table_order_by_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_force
    COMMAND mylite_runtime_alter_table_force_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_algorithm_lock_clauses
    COMMAND mylite_runtime_alter_table_algorithm_lock_clauses_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_disable_enable_keys
    COMMAND mylite_runtime_alter_table_disable_enable_keys_test
  )
  add_test(
    NAME libmylite.runtime.table_maintenance
    COMMAND mylite_runtime_table_maintenance_test
  )
  add_test(
    NAME libmylite.runtime.create_table_like_lifecycle
    COMMAND mylite_runtime_create_table_like_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.temporary_table_like
    COMMAND mylite_runtime_temporary_table_like_test
  )
  add_test(
    NAME libmylite.runtime.temporary_create_table_select
    COMMAND mylite_runtime_temporary_create_table_select_test
  )
  add_test(
    NAME libmylite.runtime.temporary_auto_increment
    COMMAND mylite_runtime_temporary_auto_increment_test
  )
  add_test(
    NAME libmylite.runtime.secondary_index_lifecycle
    COMMAND mylite_runtime_secondary_index_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.fulltext_index_metadata
    COMMAND mylite_runtime_fulltext_index_metadata_test
  )
  add_test(
    NAME libmylite.runtime.spatial_index_metadata
    COMMAND mylite_runtime_spatial_index_metadata_test
  )
  add_test(
    NAME libmylite.runtime.index_options_metadata
    COMMAND mylite_runtime_index_options_metadata_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_add_index
    COMMAND mylite_runtime_alter_table_add_index_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_add_unique
    COMMAND mylite_runtime_alter_table_add_unique_test
  )
  add_test(
    NAME libmylite.runtime.create_index_lifecycle
    COMMAND mylite_runtime_create_index_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_drop_index
    COMMAND mylite_runtime_alter_table_drop_index_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_multi_action
    COMMAND mylite_runtime_alter_table_multi_action_test
  )
  add_test(
    NAME libmylite.runtime.drop_index_lifecycle
    COMMAND mylite_runtime_drop_index_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.drop_constraint_lifecycle
    COMMAND mylite_runtime_drop_constraint_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.rename_index_lifecycle
    COMMAND mylite_runtime_rename_index_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.alter_index_visibility
    COMMAND mylite_runtime_alter_index_visibility_test
  )
  add_test(
    NAME libmylite.runtime.unique_index_lifecycle
    COMMAND mylite_runtime_unique_index_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.named_unique_constraint
    COMMAND mylite_runtime_named_unique_constraint_test
  )
  add_test(
    NAME libmylite.runtime.unique_prefix_index_lifecycle
    COMMAND mylite_runtime_unique_prefix_index_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.binary_full_column_index
    COMMAND mylite_runtime_binary_full_column_index_test
  )
  add_test(
    NAME libmylite.runtime.descending_index_key_parts
    COMMAND mylite_runtime_descending_index_key_parts_test
  )
  add_test(
    NAME libmylite.runtime.foreign_key_constraints
    COMMAND mylite_runtime_foreign_key_constraints_test
  )
  add_test(
    NAME libmylite.runtime.check_constraint_lifecycle
    COMMAND mylite_runtime_check_constraint_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.alter_check_constraint_lifecycle
    COMMAND mylite_runtime_alter_check_constraint_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.char_varchar_key_lifecycle
    COMMAND mylite_runtime_char_varchar_key_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.composite_string_primary_key
    COMMAND mylite_runtime_composite_string_primary_key_test
  )
  add_test(
    NAME libmylite.runtime.create_table_select_lifecycle
    COMMAND mylite_runtime_create_table_select_lifecycle_test
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
    NAME libmylite.runtime.temporary_table_lifecycle
    COMMAND mylite_runtime_temporary_table_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.temporary_index_lifecycle
    COMMAND mylite_runtime_temporary_index_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.lock_tables_lifecycle
    COMMAND mylite_runtime_lock_tables_lifecycle_test
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
    NAME libmylite.runtime.replace_values_lifecycle
    COMMAND mylite_runtime_replace_values_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.replace_set_lifecycle
    COMMAND mylite_runtime_replace_set_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.replace_key_lifecycle
    COMMAND mylite_runtime_replace_key_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.replace_select_lifecycle
    COMMAND mylite_runtime_replace_select_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.replace_modifier_lifecycle
    COMMAND mylite_runtime_replace_modifier_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.insert_modifier_lifecycle
    COMMAND mylite_runtime_insert_modifier_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.insert_ignore_lifecycle
    COMMAND mylite_runtime_insert_ignore_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.nonstrict_dml_coercion
    COMMAND mylite_runtime_nonstrict_dml_coercion_test
  )
  add_test(
    NAME libmylite.runtime.dml_string_numeric_coercion
    COMMAND mylite_runtime_dml_string_numeric_coercion_test
  )
  add_test(
    NAME libmylite.runtime.insert_on_duplicate_key_update
    COMMAND mylite_runtime_insert_on_duplicate_key_update_test
  )
  add_test(
    NAME libmylite.runtime.select_where_lifecycle
    COMMAND mylite_runtime_select_where_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.exists_subquery_predicates
    COMMAND mylite_runtime_exists_subquery_predicates_test
  )
  add_test(
    NAME libmylite.runtime.in_subquery_predicates
    COMMAND mylite_runtime_in_subquery_predicates_test
  )
  add_test(
    NAME libmylite.runtime.where_and_predicates
    COMMAND mylite_runtime_where_and_predicates_test
  )
  add_test(
    NAME libmylite.runtime.select_order_limit_lifecycle
    COMMAND mylite_runtime_select_order_limit_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.select_distinct_rowsets
    COMMAND mylite_runtime_select_distinct_rowsets_test
  )
  add_test(
    NAME libmylite.runtime.table_statement
    COMMAND mylite_runtime_table_statement_test
  )
  add_test(
    NAME libmylite.runtime.select_qualified_columns
    COMMAND mylite_runtime_select_qualified_columns_test
  )
  add_test(
    NAME libmylite.runtime.union_select_lifecycle
    COMMAND mylite_runtime_union_select_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.inner_join_select
    COMMAND mylite_runtime_inner_join_select_test
  )
  add_test(
    NAME libmylite.runtime.left_join_select
    COMMAND mylite_runtime_left_join_select_test
  )
  add_test(
    NAME libmylite.runtime.right_join_select
    COMMAND mylite_runtime_right_join_select_test
  )
  add_test(
    NAME libmylite.runtime.select_item_alias
    COMMAND mylite_runtime_select_item_alias_test
  )
  add_test(
    NAME libmylite.runtime.select_literal_projection
    COMMAND mylite_runtime_select_literal_projection_test
  )
  add_test(
    NAME libmylite.runtime.row_scalar_expressions
    COMMAND mylite_runtime_row_scalar_expressions_test
  )
  add_test(
    NAME libmylite.runtime.scalar_subquery_projection
    COMMAND mylite_runtime_scalar_subquery_projection_test
  )
  add_test(
    NAME libmylite.runtime.string_equality_predicates
    COMMAND mylite_runtime_string_equality_predicates_test
  )
  add_test(
    NAME libmylite.runtime.string_order_lifecycle
    COMMAND mylite_runtime_string_order_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.string_range_predicates
    COMMAND mylite_runtime_string_range_predicates_test
  )
  add_test(
    NAME libmylite.runtime.like_predicates
    COMMAND mylite_runtime_like_predicates_test
  )
  add_test(
    NAME libmylite.runtime.regexp_rlike_predicates
    COMMAND mylite_runtime_regexp_rlike_predicates_test
  )
  add_test(
    NAME libmylite.runtime.regexp_like_function
    COMMAND mylite_runtime_regexp_like_function_test
  )
  add_test(
    NAME libmylite.runtime.view_lifecycle
    COMMAND mylite_runtime_view_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.delete_lifecycle
    COMMAND mylite_runtime_delete_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.joined_delete_lifecycle
    COMMAND mylite_runtime_joined_delete_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.joined_update_lifecycle
    COMMAND mylite_runtime_joined_update_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.update_lifecycle
    COMMAND mylite_runtime_update_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.update_index_hints_noop
    COMMAND mylite_runtime_update_index_hints_noop_test
  )
  add_test(
    NAME libmylite.runtime.update_scalar_subquery_assignment
    COMMAND mylite_runtime_update_scalar_subquery_assignment_test
  )
  add_test(
    NAME libmylite.runtime.update_arithmetic_assignment
    COMMAND mylite_runtime_update_arithmetic_assignment_test
  )
  add_test(
    NAME libmylite.runtime.update_multiple_assignments
    COMMAND mylite_runtime_update_multiple_assignments_test
  )
  add_test(
    NAME libmylite.runtime.transaction_lifecycle
    COMMAND mylite_runtime_transaction_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.dml_default_keyword_values
    COMMAND mylite_runtime_dml_default_keyword_values_test
  )
  add_test(
    NAME libmylite.runtime.alter_column_visibility
    COMMAND mylite_runtime_alter_column_visibility_test
  )
  add_test(
    NAME libmylite.runtime.small_integer_types
    COMMAND mylite_runtime_small_integer_types_test
  )
  add_test(
    NAME libmylite.runtime.varchar_type
    COMMAND mylite_runtime_varchar_type_test
  )
  add_test(
    NAME libmylite.runtime.enum_type
    COMMAND mylite_runtime_enum_type_test
  )
  add_test(
    NAME libmylite.runtime.set_type
    COMMAND mylite_runtime_set_type_test
  )
  add_test(
    NAME libmylite.runtime.char_type
    COMMAND mylite_runtime_char_type_test
  )
  add_test(
    NAME libmylite.runtime.character_alias_lifecycle
    COMMAND mylite_runtime_character_alias_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.text_type
    COMMAND mylite_runtime_text_type_test
  )
  add_test(
    NAME libmylite.runtime.json_type
    COMMAND mylite_runtime_json_type_test
  )
  add_test(
    NAME libmylite.runtime.binary_string_types
    COMMAND mylite_runtime_binary_string_types_test
  )
  add_test(
    NAME libmylite.runtime.long_character_binary_aliases
    COMMAND mylite_runtime_long_character_binary_aliases_test
  )
  add_test(
    NAME libmylite.runtime.bit_type
    COMMAND mylite_runtime_bit_type_test
  )
  add_test(
    NAME libmylite.runtime.string_defaults
    COMMAND mylite_runtime_string_defaults_test
  )
  add_test(
    NAME libmylite.runtime.decimal_type
    COMMAND mylite_runtime_decimal_type_test
  )
  add_test(
    NAME libmylite.runtime.float_double_type
    COMMAND mylite_runtime_float_double_type_test
  )
  add_test(
    NAME libmylite.runtime.date_type
    COMMAND mylite_runtime_date_type_test
  )
  add_test(
    NAME libmylite.runtime.year_type
    COMMAND mylite_runtime_year_type_test
  )
  add_test(
    NAME libmylite.runtime.time_type
    COMMAND mylite_runtime_time_type_test
  )
  add_test(
    NAME libmylite.runtime.datetime_type
    COMMAND mylite_runtime_datetime_type_test
  )
  add_test(
    NAME libmylite.runtime.timestamp_type
    COMMAND mylite_runtime_timestamp_type_test
  )
  add_test(
    NAME libmylite.runtime.zero_temporal_sql_modes
    COMMAND mylite_runtime_zero_temporal_sql_modes_test
  )
  add_test(
    NAME libmylite.runtime.current_timestamp_defaults
    COMMAND mylite_runtime_current_timestamp_defaults_test
  )
  add_test(
    NAME libmylite.runtime.current_date_time_defaults
    COMMAND mylite_runtime_current_date_time_defaults_test
  )
  add_test(
    NAME libmylite.runtime.current_date_time_functions
    COMMAND mylite_runtime_current_date_time_functions_test
  )
  add_test(
    NAME libmylite.runtime.utc_date_time_functions
    COMMAND mylite_runtime_utc_date_time_functions_test
  )
  add_test(
    NAME libmylite.runtime.primary_key_lifecycle
    COMMAND mylite_runtime_primary_key_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_add_primary_key
    COMMAND mylite_runtime_alter_table_add_primary_key_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_drop_primary_key
    COMMAND mylite_runtime_alter_table_drop_primary_key_test
  )
  add_test(
    NAME libmylite.runtime.alter_table_auto_increment_option
    COMMAND mylite_runtime_alter_table_auto_increment_option_test
  )
  add_test(
    NAME libmylite.runtime.auto_increment_lifecycle
    COMMAND mylite_runtime_auto_increment_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.serial_alias_lifecycle
    COMMAND mylite_runtime_serial_alias_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.integer_default_literals
    COMMAND mylite_runtime_integer_default_literals_test
  )
  add_test(
    NAME libmylite.runtime.generated_column_lifecycle
    COMMAND mylite_runtime_generated_column_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.insert_set_lifecycle
    COMMAND mylite_runtime_insert_set_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.insert_select_lifecycle
    COMMAND mylite_runtime_insert_select_lifecycle_test
  )
  add_test(
    NAME libmylite.runtime.insert_select_union_source
    COMMAND mylite_runtime_insert_select_union_source_test
  )
  add_test(
    NAME libmylite.runtime.insert_select_on_duplicate_key_update
    COMMAND mylite_runtime_insert_select_on_duplicate_key_update_test
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
    NAME libmylite.runtime.result_column_metadata
    COMMAND mylite_runtime_result_column_metadata_test
  )
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
