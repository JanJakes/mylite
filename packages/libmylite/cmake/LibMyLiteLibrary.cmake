configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/mylite/version.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/include/mylite/version.h"
  @ONLY
)

set(MYLITE_SQL_PARSE_DIR "${PROJECT_BINARY_DIR}/generated/libmylite/sql")
set(MYLITE_SQL_PARSE_C "${MYLITE_SQL_PARSE_DIR}/mylite_parse.c")
set(MYLITE_SQL_PARSE_H "${MYLITE_SQL_PARSE_DIR}/mylite_parse.h")
set(MYLITE_LEMON_TEMPLATE "${PROJECT_SOURCE_DIR}/third_party/sqlite/upstream/tool/lempar.c")

find_package(ZLIB REQUIRED)

add_custom_command(
  OUTPUT
    "${MYLITE_SQL_PARSE_C}"
    "${MYLITE_SQL_PARSE_H}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${MYLITE_SQL_PARSE_DIR}"
  COMMAND "$<TARGET_FILE:mylite_lemon>"
    -q
    "-T${MYLITE_LEMON_TEMPLATE}"
    "-d${MYLITE_SQL_PARSE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sql/mylite_parse.y"
  DEPENDS
    mylite_lemon
    "${MYLITE_LEMON_TEMPLATE}"
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

add_library(mylite_headers INTERFACE)
add_library(MyLite::headers ALIAS mylite_headers)
set_target_properties(mylite_headers PROPERTIES EXPORT_NAME headers)
target_include_directories(mylite_headers INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

add_library(mylite
  "${MYLITE_SQL_PARSE_C}"
  "${PROJECT_SOURCE_DIR}/third_party/sqlite/upstream/ext/fts3/fts3_unicode2.c"
  src/runtime/mylite_aes.c
  src/runtime/mylite_base_conversion.c
  src/runtime/mylite_bitwise.c
  src/runtime/mylite_bitwise_aggregate.c
  src/runtime/mylite_cast_convert.c
  src/runtime/mylite_collation.c
  src/runtime/mylite_catalog.c
  src/runtime/mylite_catalog_column_mutation.c
  src/runtime/mylite_catalog_column_values.c
  src/runtime/mylite_catalog_integrity.c
  src/runtime/mylite_catalog_key_constraints.c
  src/runtime/mylite_catalog_migrations.c
  src/runtime/mylite_catalog_object_lifecycle.c
  src/runtime/mylite_catalog_read.c
  src/runtime/mylite_catalog_read_column.c
  src/runtime/mylite_catalog_read_constraints.c
  src/runtime/mylite_catalog_read_index.c
  src/runtime/mylite_catalog_read_schema_table.c
  src/runtime/mylite_catalog_schema_table_mutation.c
  src/runtime/mylite_catalog_sqlite.c
  src/runtime/mylite_catalog_state.c
  src/runtime/mylite_catalog_table_validation.c
  src/runtime/mylite_catalog_validation.c
  src/runtime/mylite_connection.c
  src/runtime/mylite_convert_tz.c
  src/runtime/mylite_date_format.c
  src/runtime/mylite_date_interval_second.c
  src/runtime/mylite_datediff.c
  src/runtime/mylite_diagnostics.c
  src/runtime/mylite_digest.c
  src/runtime/mylite_dynamic_string.c
  src/runtime/mylite_execution.c
  src/runtime/mylite_execution_ast_internal.c
  src/runtime/mylite_execution_catalog_builtin.c
  src/runtime/mylite_execution_catalog_charsets.c
  src/runtime/mylite_execution_catalog_information_schema.c
  src/runtime/mylite_execution_catalog_information_schema_access_tables.c
  src/runtime/mylite_execution_catalog_information_schema_extension_tables.c
  src/runtime/mylite_execution_catalog_information_schema_innodb.c
  src/runtime/mylite_execution_catalog_information_schema_keywords.c
  src/runtime/mylite_execution_catalog_information_schema_metadata_tables.c
  src/runtime/mylite_execution_catalog_information_schema_routine_tables.c
  src/runtime/mylite_execution_catalog_information_schema_view_tables.c
  src/runtime/mylite_execution_catalog_mysql_auth_tables.c
  src/runtime/mylite_execution_catalog_mysql_service_tables.c
  src/runtime/mylite_execution_catalog_mysql_replication_tables.c
  src/runtime/mylite_execution_catalog_mysql_misc_tables.c
  src/runtime/mylite_execution_catalog_performance_schema_tables.c
  src/runtime/mylite_execution_catalog_sys_core_tables.c
  src/runtime/mylite_execution_catalog_sys_summary_host_tables.c
  src/runtime/mylite_execution_catalog_sys_summary_innodb_tables.c
  src/runtime/mylite_execution_catalog_sys_summary_instrumentation_tables.c
  src/runtime/mylite_execution_catalog_sys_summary_io_tables.c
  src/runtime/mylite_execution_catalog_sys_summary_memory_tables.c
  src/runtime/mylite_execution_catalog_sys_summary_statement_tables.c
  src/runtime/mylite_execution_catalog_sys_summary_tables.c
  src/runtime/mylite_execution_catalog_sys_summary_user_tables.c
  src/runtime/mylite_execution_catalog_sys_summary_wait_tables.c
  src/runtime/mylite_execution_catalog_sys_schema_tables.c
  src/runtime/mylite_execution_catalog_sys_views.c
  src/runtime/mylite_execution_catalog_system_tables.c
  src/runtime/mylite_execution_diagnostics.c
  src/runtime/mylite_execution_diagnostics_ddl.c
  src/runtime/mylite_execution_diagnostics_json.c
  src/runtime/mylite_execution_diagnostics_query.c
  src/runtime/mylite_execution_diagnostics_schema.c
  src/runtime/mylite_execution_diagnostics_system_variables.c
  src/runtime/mylite_execution_diagnostics_values.c
  src/runtime/mylite_execution_dml_numeric.c
  src/runtime/mylite_execution_loaded_catalog.c
  src/runtime/mylite_execution_scalar_base_conversion.c
  src/runtime/mylite_execution_scalar_binary_aes.c
  src/runtime/mylite_execution_scalar_binary.c
  src/runtime/mylite_execution_scalar_binary_base64.c
  src/runtime/mylite_execution_scalar_binary_char.c
  src/runtime/mylite_execution_scalar_binary_common.c
  src/runtime/mylite_execution_scalar_binary_compression.c
  src/runtime/mylite_execution_scalar_binary_digest.c
  src/runtime/mylite_execution_scalar_binary_random.c
  src/runtime/mylite_execution_scalar_binary_unhex.c
  src/runtime/mylite_execution_scalar_binary_uuid.c
  src/runtime/mylite_execution_scalar_charset_collation.c
  src/runtime/mylite_execution_scalar_ip_address.c
  src/runtime/mylite_execution_scalar_json.c
  src/runtime/mylite_execution_scalar_json_constructor.c
  src/runtime/mylite_execution_scalar_json_merge.c
  src/runtime/mylite_execution_scalar_json_mutation.c
  src/runtime/mylite_execution_scalar_numeric.c
  src/runtime/mylite_execution_scalar_numeric_decimal_format.c
  src/runtime/mylite_execution_scalar_numeric_format.c
  src/runtime/mylite_execution_scalar_numeric_math.c
  src/runtime/mylite_execution_scalar_regexp.c
  src/runtime/mylite_execution_scalar_string_bitmask.c
  src/runtime/mylite_execution_scalar_string.c
  src/runtime/mylite_execution_scalar_string_position.c
  src/runtime/mylite_execution_scalar_string_transform.c
  src/runtime/mylite_execution_show_filter.c
  src/runtime/mylite_execution_sqlite_internal.c
  src/runtime/mylite_execution_scalar_temporal.c
  src/runtime/mylite_execution_scalar_temporal_arithmetic.c
  src/runtime/mylite_execution_scalar_temporal_format.c
  src/runtime/mylite_execution_statement_transaction.c
  src/runtime/mylite_execution_sql_normalization.c
  src/runtime/mylite_execution_system_variables.c
  src/runtime/mylite_execution_text_internal.c
  src/runtime/mylite_group_concat_aggregate.c
  src/runtime/mylite_integer_arithmetic.c
  src/runtime/mylite_ip_address.c
  src/runtime/mylite_last_insert_id.c
  src/runtime/mylite_json.c
  src/runtime/mylite_json_aggregate.c
  src/runtime/mylite_json_contains.c
  src/runtime/mylite_json_dom.c
  src/runtime/mylite_json_functions.c
  src/runtime/mylite_like.c
  src/runtime/mylite_json_merge.c
  src/runtime/mylite_json_mutation.c
  src/runtime/mylite_json_parse.c
  src/runtime/mylite_json_path.c
  src/runtime/mylite_json_schema.c
  src/runtime/mylite_json_search.c
  src/runtime/mylite_json_validate.c
  src/runtime/mylite_named_locks.c
  src/runtime/mylite_numeric_extras.c
  src/runtime/mylite_numeric_functions.c
  src/runtime/mylite_numeric_locale.c
  src/runtime/mylite_period_functions.c
  src/runtime/mylite_rand.c
  src/runtime/mylite_random_bytes.c
  src/runtime/mylite_regexp.c
  src/runtime/mylite_result.c
  src/runtime/mylite_result_metadata.c
  src/runtime/mylite_sqlite_bootstrap.c
  src/runtime/mylite_sqlite_registration.c
  src/runtime/mylite_statement_completion.c
  src/runtime/mylite_statement_digest.c
  src/runtime/mylite_spatial.c
  src/runtime/mylite_spatial_collect_aggregate.c
  src/runtime/mylite_spatial_functions.c
  src/runtime/mylite_statement_context.c
  src/runtime/mylite_statistical_aggregate.c
  src/runtime/mylite_system_functions.c
  src/runtime/mylite_sys_functions.c
  src/runtime/mylite_string_base64.c
  src/runtime/mylite_string_bitmask.c
  src/runtime/mylite_string_case.c
  src/runtime/mylite_string_char.c
  src/runtime/mylite_string_codepoint.c
  src/runtime/mylite_string_compression.c
  src/runtime/mylite_string_concat.c
  src/runtime/mylite_string_insert.c
  src/runtime/mylite_string_padding.c
  src/runtime/mylite_string_quote.c
  src/runtime/mylite_string_replace.c
  src/runtime/mylite_string_reverse.c
  src/runtime/mylite_string_search.c
  src/runtime/mylite_string_soundex.c
  src/runtime/mylite_string_substring_index.c
  src/runtime/mylite_string_trim.c
  src/runtime/mylite_string_unhex.c
  src/runtime/mylite_temporal_arithmetic.c
  src/runtime/mylite_temporal_constructor.c
  src/runtime/mylite_temporal_extract.c
  src/runtime/mylite_temporary_catalog.c
  src/runtime/mylite_timediff.c
  src/runtime/mylite_timestamp_function.c
  src/runtime/mylite_timestampdiff.c
  src/runtime/mylite_unix_timestamp.c
  src/runtime/mylite_uuid.c
  src/runtime/mylite_weight_string.c
  src/sql/mylite_ast.c
  src/sql/mylite_lexer.c
  src/sql/mylite_parser.c
  src/sql/mylite_parser_ddl_alter_builders.c
  src/sql/mylite_parser_ddl_create_builders.c
  src/sql/mylite_parser_ddl_index_option_builders.c
  src/sql/mylite_parser_ddl_schema_show_builders.c
  src/sql/mylite_parser_dml_builders.c
  src/sql/mylite_parser_expression_builders.c
  src/sql/mylite_parser_helpers.c
  src/sql/mylite_parser_placeholders.c
  src/sql/mylite_parser_query_builders.c
  src/sql/mylite_parser_schema_builders.c
  src/sql/mylite_parser_statement_builders.c
  src/sql/mylite_parser_token_map.c
  src/sql/mylite_version_comment.c
  src/storage/mylite_file_open.c
  src/storage/mylite_file_format.c
  src/storage/mylite_offset_vfs.c
  src/version.c
)
add_library(MyLite::mylite ALIAS mylite)

set_target_properties(mylite PROPERTIES
  EXPORT_NAME mylite
  VERSION "${PROJECT_VERSION}"
  SOVERSION "${PROJECT_VERSION_MAJOR}"
)
if(BUILD_SHARED_LIBS)
  target_compile_definitions(mylite PRIVATE MYLITE_BUILDING_SHARED_LIBRARY=1)
  target_compile_definitions(mylite INTERFACE MYLITE_USING_SHARED_LIBRARY=1)
endif()

set_source_files_properties(
  "${PROJECT_SOURCE_DIR}/third_party/sqlite/upstream/ext/fts3/fts3_unicode2.c"
  PROPERTIES
    COMPILE_DEFINITIONS SQLITE_ENABLE_FTS3=1
)
if(MSVC)
  set_source_files_properties(
    "${PROJECT_SOURCE_DIR}/third_party/sqlite/upstream/ext/fts3/fts3_unicode2.c"
    PROPERTIES COMPILE_OPTIONS /w
  )
else()
  set_source_files_properties(
    "${PROJECT_SOURCE_DIR}/third_party/sqlite/upstream/ext/fts3/fts3_unicode2.c"
    PROPERTIES COMPILE_OPTIONS -w
  )
endif()

if(MYLITE_ENABLE_PROFILING)
  target_sources(mylite PRIVATE src/runtime/mylite_profile.c)
  target_compile_definitions(mylite PUBLIC MYLITE_ENABLE_PROFILING=1)
  set(mylite_profile_allocator
    "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/mylite_profile_allocator.h"
  )
  if(MSVC)
    target_compile_options(mylite PRIVATE "/FI${mylite_profile_allocator}")
  else()
    target_compile_options(mylite PRIVATE -include "${mylite_profile_allocator}")
  endif()
  set_source_files_properties(src/runtime/mylite_profile.c PROPERTIES
    COMPILE_DEFINITIONS MYLITE_PROFILE_ALLOCATOR_IMPLEMENTATION=1
  )
endif()

if(MYLITE_ENABLE_TEST_ALLOCATOR_FAILPOINTS)
  target_sources(mylite PRIVATE src/runtime/mylite_test_allocator.c)
  set(mylite_test_allocator
    "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/mylite_test_allocator.h"
  )
  if(MSVC)
    target_compile_options(mylite PRIVATE "/FI${mylite_test_allocator}")
  else()
    target_compile_options(mylite PRIVATE -include "${mylite_test_allocator}")
  endif()
  set_source_files_properties(src/runtime/mylite_test_allocator.c PROPERTIES
    COMPILE_DEFINITIONS MYLITE_TEST_ALLOCATOR_IMPLEMENTATION=1
  )
endif()

target_include_directories(mylite
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sql"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/storage"
    "${MYLITE_SQL_PARSE_DIR}"
)

target_link_libraries(mylite
  PRIVATE
    MyLite::sqlite
    ZLIB::ZLIB
)
if(UNIX AND NOT APPLE)
  target_link_libraries(mylite PRIVATE m)
endif()
if(WIN32)
  target_link_libraries(mylite PRIVATE bcrypt)
endif()

mylite_configure_c_target(mylite)

include(CMakePackageConfigHelpers)

set(MYLITE_CONFIG_DEPENDENCIES "")
if(NOT BUILD_SHARED_LIBS)
  set(MYLITE_CONFIG_DEPENDENCIES
    "find_dependency(Threads)\nfind_dependency(ZLIB)"
  )
endif()
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MyLiteConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/MyLiteConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MyLite"
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/MyLiteConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)

set(MYLITE_PKGCONFIG_PRIVATE_LIBS "")
if(NOT BUILD_SHARED_LIBS)
  set(MYLITE_PKGCONFIG_PRIVATE_LIBS "-lmylite_sqlite -lz")
  if(UNIX AND NOT APPLE)
    string(APPEND MYLITE_PKGCONFIG_PRIVATE_LIBS " -lm")
  endif()
  if(CMAKE_THREAD_LIBS_INIT)
    string(APPEND MYLITE_PKGCONFIG_PRIVATE_LIBS " ${CMAKE_THREAD_LIBS_INIT}")
  endif()
  foreach(mylite_dl_library IN LISTS CMAKE_DL_LIBS)
    if(IS_ABSOLUTE "${mylite_dl_library}" OR mylite_dl_library MATCHES "^-")
      string(APPEND MYLITE_PKGCONFIG_PRIVATE_LIBS " ${mylite_dl_library}")
    else()
      string(APPEND MYLITE_PKGCONFIG_PRIVATE_LIBS " -l${mylite_dl_library}")
    endif()
  endforeach()
endif()
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mylite.pc.in"
  "${CMAKE_CURRENT_BINARY_DIR}/mylite.pc"
  @ONLY
)

set(mylite_install_targets mylite mylite_headers)
if(NOT BUILD_SHARED_LIBS)
  list(APPEND mylite_install_targets mylite_sqlite)
endif()
install(
  TARGETS ${mylite_install_targets}
  EXPORT MyLiteTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(
  FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/mylite/mylite.h"
    "${CMAKE_CURRENT_BINARY_DIR}/include/mylite/version.h"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/mylite"
)
install(
  EXPORT MyLiteTargets
  FILE MyLiteTargets.cmake
  NAMESPACE MyLite::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MyLite"
)
install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/MyLiteConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/MyLiteConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MyLite"
)
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/mylite.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

if(NOT CMAKE_SIZE)
  find_program(CMAKE_SIZE NAMES llvm-size size)
endif()
if(CMAKE_SIZE)
  add_custom_target(mylite_size_report
    COMMAND "${CMAKE_COMMAND}"
      "-DSIZE_TOOL=${CMAKE_SIZE}"
      "-DNM_TOOL=${CMAKE_NM}"
      "-DINPUT=$<TARGET_FILE:mylite>"
      "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/mylite-size-report.txt"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MyLiteSizeReport.cmake"
    DEPENDS mylite
    COMMENT "Writing reproducible MyLite object, section, and symbol size report"
    VERBATIM
  )
endif()

if(BUILD_SHARED_LIBS AND UNIX AND NOT APPLE AND CMAKE_NM)
  add_custom_target(mylite_abi_check
    COMMAND "${CMAKE_COMMAND}"
      "-DNM_TOOL=${CMAKE_NM}"
      "-DLIBRARY=$<TARGET_FILE:mylite>"
      "-DMANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/abi/libmylite-0.symbols"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MyLiteAbiCheck.cmake"
    DEPENDS mylite
    COMMENT "Checking the public libmylite ABI symbol manifest"
    VERBATIM
  )
endif()
