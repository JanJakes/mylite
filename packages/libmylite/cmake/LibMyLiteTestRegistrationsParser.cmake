  add_test(NAME libmylite.benchmark.csv_loader COMMAND mylite_benchmark_csv_loader_test)
  add_test(
    NAME libmylite.benchmark.parse_expectations
    COMMAND mylite_benchmark_parse_expectations_test
  )
  add_test(NAME libmylite.benchmark.sql_mode COMMAND mylite_benchmark_sql_mode_test)
  add_test(NAME libmylite.lexer COMMAND mylite_lexer_test)
  add_test(NAME libmylite.parser.core COMMAND mylite_parser_core_test)
  add_test(
    NAME libmylite.parser.builtin_string_json
    COMMAND mylite_parser_builtin_string_json_test
  )
  add_test(
    NAME libmylite.parser.builtin_temporal_numeric
    COMMAND mylite_parser_builtin_temporal_numeric_test
  )
  add_test(
    NAME libmylite.parser.expression_aggregate
    COMMAND mylite_parser_expression_aggregate_test
  )
  add_test(
    NAME libmylite.parser.expression_window_explain_gaps
    COMMAND mylite_parser_expression_window_explain_gaps_test
  )
  add_test(
    NAME libmylite.parser.corpus_expression_value_surfaces
    COMMAND mylite_parser_corpus_expression_value_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_expression_operator_temporal_surfaces
    COMMAND mylite_parser_corpus_expression_operator_temporal_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_expression_residual_surfaces
    COMMAND mylite_parser_corpus_expression_residual_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_expression_query_surfaces
    COMMAND mylite_parser_corpus_expression_query_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_query_expression_clause_surfaces
    COMMAND mylite_parser_corpus_query_expression_clause_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_select_clause_residuals
    COMMAND mylite_parser_corpus_select_clause_residuals_test
  )
  add_test(
    NAME libmylite.parser.corpus_admin_set_residuals
    COMMAND mylite_parser_corpus_admin_set_residuals_test
  )
  add_test(
    NAME libmylite.parser.corpus_nonreserved_identifier_residuals
    COMMAND mylite_parser_corpus_nonreserved_identifier_residuals_test
  )
  add_test(
    NAME libmylite.parser.corpus_set_dml_expression_placeholders
    COMMAND mylite_parser_corpus_set_dml_expression_placeholders_test
  )
  add_test(
    NAME libmylite.parser.corpus_dml_variant_surfaces
    COMMAND mylite_parser_corpus_dml_variant_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_admin_transaction_surfaces
    COMMAND mylite_parser_corpus_admin_transaction_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_query_function_subquery_surfaces
    COMMAND mylite_parser_corpus_query_function_subquery_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_ddl_option_surfaces
    COMMAND mylite_parser_corpus_ddl_option_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_ddl_constraint_option_placeholders
    COMMAND mylite_parser_corpus_ddl_constraint_option_placeholders_test
  )
  add_test(
    NAME libmylite.parser.corpus_ddl_key_type_surfaces
    COMMAND mylite_parser_corpus_ddl_key_type_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_ddl_residual_surfaces
    COMMAND mylite_parser_corpus_ddl_residual_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_query_expression_surfaces
    COMMAND mylite_parser_corpus_query_expression_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_aggregate_window_surfaces
    COMMAND mylite_parser_corpus_aggregate_window_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_scalar_predicate_surfaces
    COMMAND mylite_parser_corpus_scalar_predicate_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_parenthesized_table_reference_surfaces
    COMMAND mylite_parser_corpus_parenthesized_table_reference_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_create_table_select_surfaces
    COMMAND mylite_parser_corpus_create_table_select_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_alter_table_partition_surfaces
    COMMAND mylite_parser_corpus_alter_table_partition_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_temporal_fractional_precision_surfaces
    COMMAND mylite_parser_corpus_temporal_fractional_precision_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_variable_value_surfaces
    COMMAND mylite_parser_corpus_variable_value_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_charset_collation_surfaces
    COMMAND mylite_parser_corpus_charset_collation_surfaces_test
  )
  add_test(
    NAME libmylite.parser.corpus_function_expression_placeholders
    COMMAND mylite_parser_corpus_function_expression_placeholders_test
  )
  add_test(
    NAME libmylite.parser.corpus_view_fulltext_utility_placeholders
    COMMAND mylite_parser_corpus_view_fulltext_utility_placeholders_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_expression_value_surfaces
    COMMAND mylite_runtime_parser_corpus_expression_value_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_expression_operator_temporal_surfaces
    COMMAND mylite_runtime_parser_corpus_expression_operator_temporal_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_expression_residual_surfaces
    COMMAND mylite_runtime_parser_corpus_expression_residual_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_expression_query_surfaces
    COMMAND mylite_runtime_parser_corpus_expression_query_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_query_expression_clause_surfaces
    COMMAND mylite_runtime_parser_corpus_query_expression_clause_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_select_clause_residuals
    COMMAND mylite_runtime_parser_corpus_select_clause_residuals_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_admin_set_residuals
    COMMAND mylite_runtime_parser_corpus_admin_set_residuals_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_nonreserved_identifier_residuals
    COMMAND mylite_runtime_parser_corpus_nonreserved_identifier_residuals_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_set_dml_expression_placeholders
    COMMAND mylite_runtime_parser_corpus_set_dml_expression_placeholders_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_dml_variant_surfaces
    COMMAND mylite_runtime_parser_corpus_dml_variant_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_admin_transaction_surfaces
    COMMAND mylite_runtime_parser_corpus_admin_transaction_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_query_function_subquery_surfaces
    COMMAND mylite_runtime_parser_corpus_query_function_subquery_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_ddl_option_surfaces
    COMMAND mylite_runtime_parser_corpus_ddl_option_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_ddl_constraint_option_placeholders
    COMMAND mylite_runtime_parser_corpus_ddl_constraint_option_placeholders_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_ddl_key_type_surfaces
    COMMAND mylite_runtime_parser_corpus_ddl_key_type_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_ddl_residual_surfaces
    COMMAND mylite_runtime_parser_corpus_ddl_residual_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_query_expression_surfaces
    COMMAND mylite_runtime_parser_corpus_query_expression_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_aggregate_window_surfaces
    COMMAND mylite_runtime_parser_corpus_aggregate_window_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_scalar_predicate_surfaces
    COMMAND mylite_runtime_parser_corpus_scalar_predicate_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_parenthesized_table_reference_surfaces
    COMMAND mylite_runtime_parser_corpus_parenthesized_table_reference_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_create_table_select_surfaces
    COMMAND mylite_runtime_parser_corpus_create_table_select_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_alter_table_partition_surfaces
    COMMAND mylite_runtime_parser_corpus_alter_table_partition_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_temporal_fractional_precision_surfaces
    COMMAND mylite_runtime_parser_corpus_temporal_fractional_precision_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_variable_value_surfaces
    COMMAND mylite_runtime_parser_corpus_variable_value_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_charset_collation_surfaces
    COMMAND mylite_runtime_parser_corpus_charset_collation_surfaces_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_function_expression_placeholders
    COMMAND mylite_runtime_parser_corpus_function_expression_placeholders_test
  )
  add_test(
    NAME libmylite.runtime.parser_corpus_view_fulltext_utility_placeholders
    COMMAND mylite_runtime_parser_corpus_view_fulltext_utility_placeholders_test
  )
  add_test(NAME libmylite.parser.partition_options COMMAND mylite_parser_partition_options_test)
  add_test(
    NAME libmylite.parser.partition_selection
    COMMAND mylite_parser_partition_selection_test
  )
  add_test(NAME libmylite.parser.ddl_table COMMAND mylite_parser_ddl_table_test)
  add_test(NAME libmylite.parser.types COMMAND mylite_parser_types_test)
  add_test(
    NAME libmylite.parser.constraints_indexes
    COMMAND mylite_parser_constraints_indexes_test
  )
  add_test(NAME libmylite.parser.show COMMAND mylite_parser_show_test)
  add_test(NAME libmylite.parser.select COMMAND mylite_parser_select_test)
  add_test(
    NAME libmylite.parser.select_into_user_variables
    COMMAND mylite_parser_select_into_user_variables_test
  )
  add_test(NAME libmylite.parser.dml_control COMMAND mylite_parser_dml_control_test)
  add_test(NAME libmylite.parser.ast_snapshot COMMAND mylite_parser_ast_snapshot_test)
  add_test(
    NAME libmylite.parser.admin_stored_program_placeholders
    COMMAND mylite_parser_admin_stored_program_placeholders_test
  )
  add_test(
    NAME libmylite.parser.utility_diagnostics_placeholders
    COMMAND mylite_parser_utility_diagnostics_placeholders_test
  )
  add_test(NAME libmylite.parser.errors COMMAND mylite_parser_errors_test)
  add_test(NAME libmylite.file_format COMMAND mylite_file_format_test)
