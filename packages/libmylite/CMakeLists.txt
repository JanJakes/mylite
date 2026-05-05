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
  src/charset/mylite_charset.c
  src/runtime/mylite_connection.c
  src/runtime/mylite_connection_statement.c
  src/runtime/mylite_catalog.c
  src/runtime/mylite_catalog_physical.c
  src/runtime/mylite_catalog_schema.c
  src/runtime/mylite_diagnostics.c
  src/runtime/mylite_dml.c
  src/runtime/mylite_dml_delete_bind.c
  src/runtime/mylite_dml_delete_diagnostics.c
  src/runtime/mylite_dml_delete_execute.c
  src/runtime/mylite_dml_expression.c
  src/runtime/mylite_dml_insert_bound_value.c
  src/runtime/mylite_dml_insert_column_reference.c
  src/runtime/mylite_dml_insert_conflict.c
  src/runtime/mylite_dml_insert_default.c
  src/runtime/mylite_dml_insert_execute.c
  src/runtime/mylite_dml_insert_diagnostics.c
  src/runtime/mylite_dml_insert_replace_execute.c
  src/runtime/mylite_dml_insert_set_copy.c
  src/runtime/mylite_dml_insert_set_expression.c
  src/runtime/mylite_dml_insert_set_row_resolve.c
  src/runtime/mylite_dml_insert_sqlite_bind.c
  src/runtime/mylite_dml_insert_update_assign.c
  src/runtime/mylite_dml_insert_update_expression.c
  src/runtime/mylite_dml_insert_update_reference.c
  src/runtime/mylite_dml_insert_update_validate.c
  src/runtime/mylite_dml_insert_update.c
  src/runtime/mylite_dml_insert_value_resolve.c
  src/runtime/mylite_dml_insert_copy.c
  src/runtime/mylite_dml_insert_duplicate_update_copy.c
  src/runtime/mylite_dml_insert_copy_value.c
  src/runtime/mylite_dml_insert_validate.c
  src/runtime/mylite_dml_insert_transaction.c
  src/runtime/mylite_dml_insert_transaction_finish.c
  src/runtime/mylite_dml_statement.c
  src/runtime/mylite_dml_prepare.c
  src/runtime/mylite_dml_write_table.c
  src/runtime/mylite_dml_update_sql.c
  src/runtime/mylite_dml_update_bind.c
  src/runtime/mylite_dml_update_conflict.c
  src/runtime/mylite_dml_update_delete_copy.c
  src/runtime/mylite_dml_update_delete_expression_bind.c
  src/runtime/mylite_dml_update_diagnostics.c
  src/runtime/mylite_dml_update_delete_materialize.c
  src/runtime/mylite_dml_update_expression_bind.c
  src/runtime/mylite_dml_update_execute.c
  src/runtime/mylite_dml_update_rowset.c
  src/runtime/mylite_dml_update_value.c
  src/runtime/mylite_dml_value_diagnostics.c
  src/runtime/mylite_expression_collation.c
  src/runtime/mylite_expression_collation_function.c
  src/runtime/mylite_expression_collation_leaf.c
  src/runtime/mylite_expression_descriptor.c
  src/runtime/mylite_expression_descriptor_aggregate.c
  src/runtime/mylite_expression_descriptor_basic.c
  src/runtime/mylite_expression_descriptor_cast.c
  src/runtime/mylite_expression_descriptor_case.c
  src/runtime/mylite_expression_descriptor_compare.c
  src/runtime/mylite_expression_descriptor_dispatch.c
  src/runtime/mylite_expression_descriptor_function.c
  src/runtime/mylite_expression_descriptor_numeric.c
  src/runtime/mylite_expression_descriptor_numeric_format.c
  src/runtime/mylite_expression_descriptor_numeric_variadic.c
  src/runtime/mylite_expression_descriptor_operator.c
  src/runtime/mylite_expression_descriptor_scalar.c
  src/runtime/mylite_expression_descriptor_string.c
  src/runtime/mylite_expression_descriptor_string_char.c
  src/runtime/mylite_expression_descriptor_string_encoding.c
  src/runtime/mylite_expression_descriptor_string_quote.c
  src/runtime/mylite_expression_descriptor_subquery.c
  src/runtime/mylite_expression_descriptor_temporal.c
  src/runtime/mylite_expression_descriptor_temporal_time.c
  src/runtime/mylite_expression_descriptor_union.c
  src/runtime/mylite_expression_validation.c
  src/runtime/mylite_field_descriptor.c
  src/runtime/mylite_function_names_charset.c
  src/runtime/mylite_function_names_compare.c
  src/runtime/mylite_function_names_conversion.c
  src/runtime/mylite_function_names_numeric.c
  src/runtime/mylite_function_names_string.c
  src/runtime/mylite_function_names_temporal.c
  src/runtime/mylite_information_schema.c
  src/runtime/mylite_information_schema_dynamic.c
  src/runtime/mylite_information_schema_target.c
  src/runtime/mylite_metadata.c
  src/runtime/mylite_metadata_constants.c
  src/runtime/mylite_schema.c
  src/runtime/mylite_select.c
  src/runtime/mylite_select_aggregate.c
  src/runtime/mylite_select_aggregate_bind.c
  src/runtime/mylite_select_aggregate_bind_registration.c
  src/runtime/mylite_select_aggregate_count_distinct_bind.c
  src/runtime/mylite_select_aggregate_materialize.c
  src/runtime/mylite_select_aggregate_numeric.c
  src/runtime/mylite_select_catalog.c
  src/runtime/mylite_select_catalog_descriptor.c
  src/runtime/mylite_select_catalog_descriptor_source.c
  src/runtime/mylite_select_catalog_descriptor_type.c
  src/runtime/mylite_select_compare.c
  src/runtime/mylite_select_context.c
  src/runtime/mylite_select_diagnostics.c
  src/runtime/mylite_select_distinct_validate.c
  src/runtime/mylite_select_eval.c
  src/runtime/mylite_select_eval_expression.c
  src/runtime/mylite_select_from.c
  src/runtime/mylite_select_from_nullability.c
  src/runtime/mylite_select_from_resolve.c
  src/runtime/mylite_select_from_using.c
  src/runtime/mylite_select_group.c
  src/runtime/mylite_select_group_bind.c
  src/runtime/mylite_select_group_invariant.c
  src/runtime/mylite_select_group_resolve.c
  src/runtime/mylite_select_group_validate.c
  src/runtime/mylite_select_join_cache.c
  src/runtime/mylite_select_join_materialize.c
  src/runtime/mylite_select_join_output.c
  src/runtime/mylite_select_join_outer_materialize.c
  src/runtime/mylite_select_join_range_rowset.c
  src/runtime/mylite_select_join_rows.c
  src/runtime/mylite_select_limit.c
  src/runtime/mylite_select_materialize.c
  src/runtime/mylite_select_materialize_common.c
  src/runtime/mylite_select_metadata.c
  src/runtime/mylite_select_order_bind.c
  src/runtime/mylite_select_order_expression_bind.c
  src/runtime/mylite_select_plan.c
  src/runtime/mylite_select_prepare.c
  src/runtime/mylite_select_predicate_bind.c
  src/runtime/mylite_select_predicate_expression_bind.c
  src/runtime/mylite_select_projection.c
  src/runtime/mylite_select_projection_wildcard.c
  src/runtime/mylite_select_projection_wildcard_sequence.c
  src/runtime/mylite_select_reference.c
  src/runtime/mylite_select_resolve.c
  src/runtime/mylite_select_row_loader.c
  src/runtime/mylite_select_row_match.c
  src/runtime/mylite_select_row_compare.c
  src/runtime/mylite_select_rowset.c
  src/runtime/mylite_select_rowset_distinct.c
  src/runtime/mylite_select_rowset_sort.c
  src/runtime/mylite_select_scalar.c
  src/runtime/mylite_select_scalar_eval.c
  src/runtime/mylite_select_scalar_order_validate.c
  src/runtime/mylite_select_statement.c
  src/runtime/mylite_select_sql.c
  src/runtime/mylite_select_subquery.c
  src/runtime/mylite_select_subquery_classify.c
  src/runtime/mylite_select_subquery_diagnostics.c
  src/runtime/mylite_select_subquery_eval.c
  src/runtime/mylite_select_subquery_outer_reference.c
  src/runtime/mylite_select_subquery_quantified.c
  src/runtime/mylite_select_subquery_row.c
  src/runtime/mylite_select_subquery_scalar.c
  src/runtime/mylite_select_subquery_validate.c
  src/runtime/mylite_select_table_materialize.c
  src/runtime/mylite_select_target.c
  src/runtime/mylite_select_union.c
  src/runtime/mylite_select_union_order.c
  src/runtime/mylite_select_union_order_eval.c
  src/runtime/mylite_select_union_prepare.c
  src/runtime/mylite_select_using_range.c
  src/runtime/mylite_session_functions.c
  src/runtime/mylite_show.c
  src/runtime/mylite_show_charset_sql.c
  src/runtime/mylite_show_create.c
  src/runtime/mylite_show_create_common.c
  src/runtime/mylite_show_create_table.c
  src/runtime/mylite_show_create_table_body.c
  src/runtime/mylite_show_create_table_columns.c
  src/runtime/mylite_show_create_table_info.c
  src/runtime/mylite_show_create_table_indexes.c
  src/runtime/mylite_show_create_table_options.c
  src/runtime/mylite_show_create_table_target.c
  src/runtime/mylite_show_diagnostics_sql.c
  src/runtime/mylite_show_index_target.c
  src/runtime/mylite_show_prepare.c
  src/runtime/mylite_show_status_sql.c
  src/runtime/mylite_show_tables.c
  src/runtime/mylite_show_targets.c
  src/runtime/mylite_show_variables_sql.c
  src/runtime/mylite_sqlite_value.c
  src/runtime/mylite_span.c
  src/runtime/mylite_statement.c
  src/runtime/mylite_statement_ast.c
  src/runtime/mylite_statement_custom.c
  src/runtime/mylite_statement_execute.c
  src/runtime/mylite_statement_functions.c
  src/runtime/mylite_statement_prepare.c
  src/runtime/mylite_statement_prepare_custom.c
  src/runtime/mylite_statement_strcmp.c
  src/runtime/mylite_strcmp_compare.c
  src/runtime/mylite_storage_engine.c
  src/runtime/mylite_table_ddl.c
  src/runtime/mylite_table_ddl_alter.c
  src/runtime/mylite_table_ddl_alter_catalog.c
  src/runtime/mylite_table_ddl_alter_column.c
  src/runtime/mylite_table_ddl_alter_column_definition.c
  src/runtime/mylite_table_ddl_alter_column_value.c
  src/runtime/mylite_table_ddl_alter_copy.c
  src/runtime/mylite_table_ddl_alter_diagnostics.c
  src/runtime/mylite_table_ddl_alter_execute.c
  src/runtime/mylite_table_ddl_alter_index.c
  src/runtime/mylite_table_ddl_alter_index_build.c
  src/runtime/mylite_table_ddl_alter_index_model.c
  src/runtime/mylite_table_ddl_alter_index_validate.c
  src/runtime/mylite_table_ddl_alter_metadata.c
  src/runtime/mylite_table_ddl_alter_model.c
  src/runtime/mylite_table_ddl_alter_model_load.c
  src/runtime/mylite_table_ddl_alter_rebuild.c
  src/runtime/mylite_table_ddl_alter_unique_validate.c
  src/runtime/mylite_table_ddl_alter_validate.c
  src/runtime/mylite_table_ddl_alter_warnings.c
  src/runtime/mylite_table_ddl_catalog.c
  src/runtime/mylite_table_ddl_copy.c
  src/runtime/mylite_table_ddl_create_catalog_index.c
  src/runtime/mylite_table_ddl_create_column_copy.c
  src/runtime/mylite_table_ddl_create_copy.c
  src/runtime/mylite_table_ddl_create_options.c
  src/runtime/mylite_table_ddl_create_sql.c
  src/runtime/mylite_table_ddl_create_validate.c
  src/runtime/mylite_table_ddl_drop.c
  src/runtime/mylite_table_ddl_index.c
  src/runtime/mylite_table_ddl_index_catalog.c
  src/runtime/mylite_table_ddl_index_name.c
  src/runtime/mylite_table_ddl_index_warnings.c
  src/runtime/mylite_table_ddl_plan.c
  src/runtime/mylite_table_ddl_plan_lookup.c
  src/runtime/mylite_table_ddl_rename.c
  src/runtime/mylite_table_ddl_statement.c
  src/runtime/mylite_table_ddl_truncate.c
  src/runtime/mylite_temporal_functions.c
  src/runtime/mylite_transaction_auto_increment.c
  src/runtime/mylite_transaction_savepoint.c
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
