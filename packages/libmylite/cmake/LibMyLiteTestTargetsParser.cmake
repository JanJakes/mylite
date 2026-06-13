  add_executable(mylite_benchmark_csv_loader_test
    benchmarks/mylite_benchmark_csv.c
    tests/benchmark_csv_loader_test.c
  )
  target_include_directories(mylite_benchmark_csv_loader_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks"
  )
  mylite_configure_c_target(mylite_benchmark_csv_loader_test)

  add_executable(mylite_benchmark_parse_expectations_test
    benchmarks/mylite_benchmark_parse_expectations.c
    tests/benchmark_parse_expectations_test.c
  )
  target_link_libraries(mylite_benchmark_parse_expectations_test PRIVATE MyLite::mylite)
  target_include_directories(mylite_benchmark_parse_expectations_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_benchmark_parse_expectations_test)

  add_executable(mylite_benchmark_sql_mode_test
    benchmarks/mylite_benchmark_csv.c
    benchmarks/mylite_benchmark_sql_mode.c
    tests/benchmark_sql_mode_test.c
  )
  target_link_libraries(mylite_benchmark_sql_mode_test PRIVATE MyLite::mylite)
  target_include_directories(mylite_benchmark_sql_mode_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  mylite_configure_c_target(mylite_benchmark_sql_mode_test)

  add_executable(mylite_lexer_test
    tests/lexer_test.c
  )
  target_link_libraries(mylite_lexer_test PRIVATE MyLite::mylite)
  target_include_directories(mylite_lexer_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_compile_definitions(mylite_lexer_test PRIVATE
    MYLITE_LEXER_CORPUS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/mysql_lexer_success.sql"
  )
  mylite_configure_c_target(mylite_lexer_test)

  add_library(mylite_parser_test_support OBJECT
    tests/parser_test_support.c
  )
  target_link_libraries(mylite_parser_test_support PRIVATE MyLite::mylite)
  target_include_directories(mylite_parser_test_support PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  mylite_configure_c_target(mylite_parser_test_support)

  function(mylite_add_parser_test target source)
    add_executable(${target}
      ${source}
      $<TARGET_OBJECTS:mylite_parser_test_support>
    )
    target_link_libraries(${target} PRIVATE MyLite::mylite)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    mylite_configure_c_target(${target})
  endfunction()

  mylite_add_parser_test(mylite_parser_core_test tests/parser_core_test.c)
  mylite_add_parser_test(
    mylite_parser_builtin_string_json_test
    tests/parser_builtin_string_json_test.c
  )
  mylite_add_parser_test(
    mylite_parser_builtin_temporal_numeric_test
    tests/parser_builtin_temporal_numeric_test.c
  )
  mylite_add_parser_test(
    mylite_parser_expression_aggregate_test
    tests/parser_expression_aggregate_test.c
  )
  mylite_add_parser_test(
    mylite_parser_expression_window_explain_gaps_test
    tests/parser_expression_window_explain_gaps_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_expression_value_surfaces_test
    tests/parser_corpus_expression_value_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_expression_operator_temporal_surfaces_test
    tests/parser_corpus_expression_operator_temporal_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_expression_residual_surfaces_test
    tests/parser_corpus_expression_residual_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_expression_query_surfaces_test
    tests/parser_corpus_expression_query_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_query_expression_clause_surfaces_test
    tests/parser_corpus_query_expression_clause_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_select_clause_residuals_test
    tests/parser_corpus_select_clause_residuals_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_admin_set_residuals_test
    tests/parser_corpus_admin_set_residuals_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_nonreserved_identifier_residuals_test
    tests/parser_corpus_nonreserved_identifier_residuals_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_set_dml_expression_placeholders_test
    tests/parser_corpus_set_dml_expression_placeholders_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_dml_variant_surfaces_test
    tests/parser_corpus_dml_variant_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_admin_transaction_surfaces_test
    tests/parser_corpus_admin_transaction_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_query_function_subquery_surfaces_test
    tests/parser_corpus_query_function_subquery_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_ddl_option_surfaces_test
    tests/parser_corpus_ddl_option_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_ddl_constraint_option_placeholders_test
    tests/parser_corpus_ddl_constraint_option_placeholders_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_ddl_key_type_surfaces_test
    tests/parser_corpus_ddl_key_type_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_ddl_residual_surfaces_test
    tests/parser_corpus_ddl_residual_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_query_expression_surfaces_test
    tests/parser_corpus_query_expression_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_aggregate_window_surfaces_test
    tests/parser_corpus_aggregate_window_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_scalar_predicate_surfaces_test
    tests/parser_corpus_scalar_predicate_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_parenthesized_table_reference_surfaces_test
    tests/parser_corpus_parenthesized_table_reference_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_create_table_select_surfaces_test
    tests/parser_corpus_create_table_select_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_alter_table_partition_surfaces_test
    tests/parser_corpus_alter_table_partition_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_temporal_fractional_precision_surfaces_test
    tests/parser_corpus_temporal_fractional_precision_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_variable_value_surfaces_test
    tests/parser_corpus_variable_value_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_charset_collation_surfaces_test
    tests/parser_corpus_charset_collation_surfaces_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_function_expression_placeholders_test
    tests/parser_corpus_function_expression_placeholders_test.c
  )
  mylite_add_parser_test(
    mylite_parser_corpus_view_fulltext_utility_placeholders_test
    tests/parser_corpus_view_fulltext_utility_placeholders_test.c
  )
  add_executable(
    mylite_runtime_parser_corpus_expression_value_surfaces_test
    tests/runtime_parser_corpus_expression_value_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_expression_value_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(mylite_runtime_parser_corpus_expression_value_surfaces_test)
  add_executable(
    mylite_runtime_parser_corpus_expression_operator_temporal_surfaces_test
    tests/runtime_parser_corpus_expression_operator_temporal_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_expression_operator_temporal_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(
    mylite_runtime_parser_corpus_expression_operator_temporal_surfaces_test
  )
  add_executable(
    mylite_runtime_parser_corpus_expression_residual_surfaces_test
    tests/runtime_parser_corpus_expression_residual_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_expression_residual_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(
    mylite_runtime_parser_corpus_expression_residual_surfaces_test
  )
  add_executable(
    mylite_runtime_parser_corpus_expression_query_surfaces_test
    tests/runtime_parser_corpus_expression_query_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_expression_query_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(mylite_runtime_parser_corpus_expression_query_surfaces_test)
  add_executable(
    mylite_runtime_parser_corpus_query_expression_clause_surfaces_test
    tests/runtime_parser_corpus_query_expression_clause_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_query_expression_clause_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(
    mylite_runtime_parser_corpus_query_expression_clause_surfaces_test
  )
  add_executable(
    mylite_runtime_parser_corpus_select_clause_residuals_test
    tests/runtime_parser_corpus_select_clause_residuals_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_select_clause_residuals_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(mylite_runtime_parser_corpus_select_clause_residuals_test)
  add_executable(
    mylite_runtime_parser_corpus_admin_set_residuals_test
    tests/runtime_parser_corpus_admin_set_residuals_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_admin_set_residuals_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(mylite_runtime_parser_corpus_admin_set_residuals_test)
  add_executable(
    mylite_runtime_parser_corpus_nonreserved_identifier_residuals_test
    tests/runtime_parser_corpus_nonreserved_identifier_residuals_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_nonreserved_identifier_residuals_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(
    mylite_runtime_parser_corpus_nonreserved_identifier_residuals_test
  )
  add_executable(
    mylite_runtime_parser_corpus_set_dml_expression_placeholders_test
    tests/runtime_parser_corpus_set_dml_expression_placeholders_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_set_dml_expression_placeholders_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(
    mylite_runtime_parser_corpus_set_dml_expression_placeholders_test
  )
  add_executable(
    mylite_runtime_parser_corpus_dml_variant_surfaces_test
    tests/runtime_parser_corpus_dml_variant_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_dml_variant_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(
    mylite_runtime_parser_corpus_dml_variant_surfaces_test
  )
  add_executable(
    mylite_runtime_parser_corpus_admin_transaction_surfaces_test
    tests/runtime_parser_corpus_admin_transaction_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_admin_transaction_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(
    mylite_runtime_parser_corpus_admin_transaction_surfaces_test
  )
  add_executable(
    mylite_runtime_parser_corpus_query_function_subquery_surfaces_test
    tests/runtime_parser_corpus_query_function_subquery_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_query_function_subquery_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(
    mylite_runtime_parser_corpus_query_function_subquery_surfaces_test
  )
  add_executable(
    mylite_runtime_parser_corpus_ddl_option_surfaces_test
    tests/runtime_parser_corpus_ddl_option_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_ddl_option_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(mylite_runtime_parser_corpus_ddl_option_surfaces_test)
  add_executable(
    mylite_runtime_parser_corpus_ddl_constraint_option_placeholders_test
    tests/runtime_parser_corpus_ddl_constraint_option_placeholders_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_ddl_constraint_option_placeholders_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(
    mylite_runtime_parser_corpus_ddl_constraint_option_placeholders_test
  )
  add_executable(
    mylite_runtime_parser_corpus_ddl_key_type_surfaces_test
    tests/runtime_parser_corpus_ddl_key_type_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_ddl_key_type_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(mylite_runtime_parser_corpus_ddl_key_type_surfaces_test)
  add_executable(
    mylite_runtime_parser_corpus_ddl_residual_surfaces_test
    tests/runtime_parser_corpus_ddl_residual_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_ddl_residual_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(mylite_runtime_parser_corpus_ddl_residual_surfaces_test)
  add_executable(
    mylite_runtime_parser_corpus_query_expression_surfaces_test
    tests/runtime_parser_corpus_query_expression_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_query_expression_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(mylite_runtime_parser_corpus_query_expression_surfaces_test)
  add_executable(
    mylite_runtime_parser_corpus_aggregate_window_surfaces_test
    tests/runtime_parser_corpus_aggregate_window_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_aggregate_window_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(mylite_runtime_parser_corpus_aggregate_window_surfaces_test)
  add_executable(
    mylite_runtime_parser_corpus_scalar_predicate_surfaces_test
    tests/runtime_parser_corpus_scalar_predicate_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_scalar_predicate_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(mylite_runtime_parser_corpus_scalar_predicate_surfaces_test)
  add_executable(
    mylite_runtime_parser_corpus_parenthesized_table_reference_surfaces_test
    tests/runtime_parser_corpus_parenthesized_table_reference_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_parenthesized_table_reference_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(
    mylite_runtime_parser_corpus_parenthesized_table_reference_surfaces_test
  )
  add_executable(
    mylite_runtime_parser_corpus_create_table_select_surfaces_test
    tests/runtime_parser_corpus_create_table_select_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_create_table_select_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(mylite_runtime_parser_corpus_create_table_select_surfaces_test)
  add_executable(
    mylite_runtime_parser_corpus_alter_table_partition_surfaces_test
    tests/runtime_parser_corpus_alter_table_partition_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_alter_table_partition_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(mylite_runtime_parser_corpus_alter_table_partition_surfaces_test)
  add_executable(
    mylite_runtime_parser_corpus_temporal_fractional_precision_surfaces_test
    tests/runtime_parser_corpus_temporal_fractional_precision_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_temporal_fractional_precision_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(
    mylite_runtime_parser_corpus_temporal_fractional_precision_surfaces_test
  )
  add_executable(
    mylite_runtime_parser_corpus_variable_value_surfaces_test
    tests/runtime_parser_corpus_variable_value_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_variable_value_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(mylite_runtime_parser_corpus_variable_value_surfaces_test)
  add_executable(
    mylite_runtime_parser_corpus_charset_collation_surfaces_test
    tests/runtime_parser_corpus_charset_collation_surfaces_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_charset_collation_surfaces_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(mylite_runtime_parser_corpus_charset_collation_surfaces_test)
  add_executable(
    mylite_runtime_parser_corpus_function_expression_placeholders_test
    tests/runtime_parser_corpus_function_expression_placeholders_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_function_expression_placeholders_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(
    mylite_runtime_parser_corpus_function_expression_placeholders_test
  )
  add_executable(
    mylite_runtime_parser_corpus_view_fulltext_utility_placeholders_test
    tests/runtime_parser_corpus_view_fulltext_utility_placeholders_test.c
  )
  target_link_libraries(
    mylite_runtime_parser_corpus_view_fulltext_utility_placeholders_test
    PRIVATE MyLite::mylite
  )
  mylite_configure_c_target(
    mylite_runtime_parser_corpus_view_fulltext_utility_placeholders_test
  )
  mylite_add_parser_test(mylite_parser_partition_options_test tests/parser_partition_options_test.c)
  mylite_add_parser_test(
    mylite_parser_partition_selection_test
    tests/parser_partition_selection_test.c
  )
  mylite_add_parser_test(mylite_parser_ddl_table_test tests/parser_ddl_table_test.c)
  mylite_add_parser_test(mylite_parser_types_test tests/parser_types_test.c)
  mylite_add_parser_test(
    mylite_parser_constraints_indexes_test
    tests/parser_constraints_indexes_test.c
  )
  mylite_add_parser_test(mylite_parser_show_test tests/parser_show_test.c)
  mylite_add_parser_test(mylite_parser_select_test tests/parser_select_test.c)
  mylite_add_parser_test(
    mylite_parser_select_into_user_variables_test
    tests/parser_select_into_user_variables_test.c
  )
  mylite_add_parser_test(mylite_parser_dml_control_test tests/parser_dml_control_test.c)
  mylite_add_parser_test(
    mylite_parser_admin_stored_program_placeholders_test
    tests/parser_admin_stored_program_placeholders_test.c
  )
  mylite_add_parser_test(
    mylite_parser_utility_diagnostics_placeholders_test
    tests/parser_utility_diagnostics_placeholders_test.c
  )
  mylite_add_parser_test(mylite_parser_errors_test tests/parser_errors_test.c)

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
