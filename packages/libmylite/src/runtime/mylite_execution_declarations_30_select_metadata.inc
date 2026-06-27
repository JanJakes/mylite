static int execute_select_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static bool select_statement_has_aggregate_window_expression(
    const struct mylite_sql_ast_node *statement
);
static bool ast_node_has_aggregate_window_expression(const struct mylite_sql_ast_node *node);
static bool ast_node_is_aggregate_function(const struct mylite_sql_ast_node *node);
static bool ast_node_is_generic_aggregate_window_function(const struct mylite_sql_ast_node *node);
static bool source_span_text_equals_ascii_case_insensitive(
    const struct mylite_sql_source_span *span,
    const char *text
);
static bool ast_node_is_window_clause_marker(const struct mylite_sql_ast_node *node);
static int execute_select_into_user_variables_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static const struct mylite_sql_ast_node *select_statement_into_list(
    const struct mylite_sql_ast_node *statement
);
static const struct mylite_sql_ast_node *select_statement_source_clause(
    const struct mylite_sql_ast_node *statement
);
static const struct mylite_sql_ast_node *select_statement_optional_clause_start(
    const struct mylite_sql_ast_node *statement
);
static int apply_select_into_user_variables(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *into_list,
    const mylite_result *select_result,
    size_t *out_assigned_row_count,
    uint64_t *out_found_rows
);
static enum mylite_session_user_variable_value_kind select_into_user_variable_value_kind(
    const mylite_result *select_result,
    size_t column_index
);
static int execute_grouped_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int execute_count_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int execute_column_aggregate_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static bool select_statement_has_count_having_clause(const struct mylite_sql_ast_node *statement);
static int execute_count_having_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static bool select_statement_has_count_expression_aggregate(
    const struct mylite_sql_ast_node *statement
);
static int execute_count_expression_aggregate_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int execute_descriptor_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int execute_compound_select_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);

struct compound_row_comparison_columns {
    bool *string_collation_columns;
    bool *bytewise_collation_columns;
    size_t column_count;
};

struct compound_branch_results {
    mylite_result **items;
    size_t count;
};

static int reject_compound_select_branch_shape(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *branch,
    enum mylite_sql_ast_set_operator operator_kind,
    bool allow_final_order_limit
);
static int validate_compound_select_operator_chain(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *terms,
    enum mylite_sql_ast_set_operator *out_operator_kind
);
static int execute_compound_select_branch_results(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    enum mylite_sql_ast_set_operator operator_kind,
    struct compound_branch_results *out_results
);
static void deinit_compound_branch_results(struct compound_branch_results *results);
static size_t compound_select_branch_count(const struct mylite_sql_ast_node *statement);
static int init_compound_row_comparison_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct compound_branch_results *branch_results,
    struct compound_row_comparison_columns *out_columns
);
static void deinit_compound_row_comparison_columns(struct compound_row_comparison_columns *columns);
static int apply_compound_term_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *term,
    const mylite_result *branch_result,
    mylite_result **in_out_result,
    struct compound_row_comparison_columns *comparison_columns
);
static int append_compound_result_columns(
    struct mylite_db *database,
    mylite_result *target,
    const mylite_result *source
);
static int append_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    bool distinct,
    const bool *string_collation_columns
);
static int apply_intersect_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    bool distinct,
    const bool *string_collation_columns
);
static int apply_intersect_all_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    const bool *string_collation_columns
);
static int apply_except_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    bool distinct,
    const bool *string_collation_columns
);
static int apply_except_all_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    const bool *string_collation_columns
);
static int create_empty_compound_result_like(
    struct mylite_db *database,
    const mylite_result *source,
    mylite_result **out_result
);
static int deduplicate_compound_result(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const bool *string_collation_columns
);
static int append_distinct_compound_rows(
    struct mylite_db *database,
    mylite_result *target,
    const mylite_result *source,
    const bool *string_collation_columns
);
static int append_all_compound_rows(
    struct mylite_db *database,
    mylite_result *target,
    const mylite_result *source
);
static int append_compound_row(
    struct mylite_db *database,
    mylite_result *target,
    const mylite_result *source,
    size_t row_index
);
static int merge_compound_string_collation_columns(
    struct mylite_db *database,
    const mylite_result *branch_result,
    struct compound_row_comparison_columns *columns
);
static bool compound_result_column_uses_string_collation(
    const mylite_result *result,
    size_t column_index
);
static bool result_column_uses_string_collation(const struct mylite_result_column *column);
static bool compound_result_column_uses_bytewise_collation(
    const mylite_result *result,
    size_t column_index
);
static bool result_column_uses_bytewise_collation(const struct mylite_result_column *column);
static bool compound_statement_column_uses_string_collation(
    const struct mylite_sql_ast_node *statement,
    size_t column_index
);
static bool compound_select_item_uses_string_collation(
    const struct mylite_sql_ast_node *branch,
    size_t column_index
);
static bool compound_expression_uses_string_collation(const struct mylite_sql_ast_node *expression);
static bool compound_statement_column_has_binary_expression(
    const struct mylite_sql_ast_node *statement,
    size_t column_index
);
static bool compound_select_item_uses_binary_collation(
    const struct mylite_sql_ast_node *branch,
    size_t column_index
);
static bool compound_expression_uses_binary_collation(const struct mylite_sql_ast_node *expression);
static bool compound_result_contains_row(
    const mylite_result *target,
    const mylite_result *source,
    size_t source_row_index,
    const bool *string_collation_columns
);
static bool compound_result_find_unmatched_row(
    const mylite_result *target,
    const mylite_result *source,
    size_t source_row_index,
    const bool *matched_rows,
    size_t *out_row_index,
    const bool *string_collation_columns
);
static const char *compound_select_non_select_branch_error(
    enum mylite_sql_ast_set_operator operator_kind
);
static const char *compound_select_calc_found_rows_error(
    enum mylite_sql_ast_set_operator operator_kind
);
static const char *compound_select_options_error(enum mylite_sql_ast_set_operator operator_kind);
static const char *compound_select_order_by_error(enum mylite_sql_ast_set_operator operator_kind);
static const char *compound_select_limit_error(enum mylite_sql_ast_set_operator operator_kind);
static const char *compound_select_locking_clause_error(
    enum mylite_sql_ast_set_operator operator_kind
);
static bool compound_result_rows_equal(
    const mylite_result *left,
    size_t left_row_index,
    const mylite_result *right,
    size_t right_row_index,
    const bool *string_collation_columns
);
static bool compound_result_cells_equal(
    const mylite_result *left,
    size_t left_row_index,
    const mylite_result *right,
    size_t right_row_index,
    size_t column_index,
    const bool *string_collation_columns
);
static bool compound_result_cells_equal_ascii_ci(
    const void *left_bytes,
    size_t left_size,
    const void *right_bytes,
    size_t right_size
);
static int execute_scalar_or_row_scalar_select_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result,
    bool *out_handled
);
static void apply_sql_select_limit_to_plan_limit(
    const struct mylite_db *database,
    struct planned_select_limit *limit
);
static void apply_sql_select_limit_to_result(
    const struct mylite_db *database,
    mylite_result *result
);
static int apply_compound_select_final_order_and_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result *result
);
static const struct mylite_sql_ast_node *compound_select_final_branch(
    const struct mylite_sql_ast_node *statement
);
static int apply_compound_select_final_order(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *order_clause
);
static int compound_select_order_key_and_direction(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct mylite_sql_ast_node **out_order_key,
    bool *out_descending
);
static int compound_select_order_column_index(
    struct mylite_db *database,
    const mylite_result *result,
    const struct mylite_sql_ast_node *order_key,
    size_t *out_column_index
);
static void compound_select_sort_result(
    mylite_result *result,
    size_t column_index,
    bool descending
);
static int compound_select_compare_order_rows(
    const mylite_result *result,
    size_t left_row,
    size_t right_row,
    size_t column_index,
    bool descending
);
static void compound_select_swap_result_rows(mylite_result *result, size_t left, size_t right);
static int apply_compound_select_final_limit(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *limit_clause
);
static void compound_select_discard_result_prefix(mylite_result *result, uint64_t offset);
static int reject_select_modifier_usage_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int execute_information_schema_select_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int execute_information_schema_join_compat_select_if_needed(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result,
    bool *out_handled
);
static int information_schema_join_compat_select_statement_is_supported(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool *out_matches
);
static int select_statement_targets_information_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool *out_matches
);
static int select_source_targets_information_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source,
    struct scalar_arithmetic_node_stack *statement_stack,
    bool *out_matches
);
static int table_source_targets_information_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source,
    bool *out_matches
);
static int derived_source_targets_information_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source,
    struct scalar_arithmetic_node_stack *statement_stack,
    bool *out_matches
);
static int execute_mysql_system_table_select_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int select_statement_targets_mysql_data_dictionary_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    char *out_table_name,
    size_t table_name_size,
    bool *out_matches
);
static int select_statement_targets_absent_mysql_enterprise_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    char *out_table_name,
    size_t table_name_size,
    bool *out_matches
);
static int select_statement_targets_mysql_system_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool *out_matches
);
static int resolve_mysql_system_table_query(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct information_schema_query *out_query,
    const struct mylite_execution_catalog_mysql_system_table **out_definition
);
static int mysql_system_table_resolve_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct information_schema_query *out_query,
    const struct mylite_execution_catalog_mysql_system_table **out_definition
);
static const struct mylite_execution_catalog_mysql_system_table *find_mysql_system_table_definition(
    const char *schema_name,
    const char *table_name
);
static bool mysql_data_dictionary_table_is_hidden(const char *table_name);
static bool mysql_enterprise_table_is_target_absent(const char *table_name);
static int execute_mysql_system_table_query(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int build_mysql_system_table_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *out_rows
);
static int append_mysql_system_table_system_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
);
static int append_mysql_schema_system_table_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
);
static int append_performance_schema_system_table_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
);
static int append_performance_schema_accounts_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_performance_schema_hosts_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_performance_schema_users_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_performance_schema_processlist_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    struct information_schema_row_set *rows
);
static int append_performance_schema_threads_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    struct information_schema_row_set *rows
);
static int append_performance_schema_connection_attribute_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_performance_schema_connection_count_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *const *values,
    size_t expected_column_count
);
static int format_performance_schema_session_count(
    struct mylite_db *database,
    char *count_text,
    size_t count_text_size
);
static int append_performance_schema_variable_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    bool global_scope
);
static int append_performance_schema_status_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    bool global_scope
);
static int append_performance_schema_variable_by_thread_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_performance_schema_status_by_thread_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_performance_schema_status_by_account_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_performance_schema_status_by_host_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_performance_schema_status_by_user_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_performance_schema_performance_timer_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_performance_schema_setup_actor_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_performance_schema_setup_consumer_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_performance_schema_setup_logger_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_performance_schema_setup_meter_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_performance_schema_setup_metric_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_performance_schema_setup_object_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_performance_schema_setup_thread_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_performance_schema_user_defined_function_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_performance_schema_user_variable_by_thread_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int format_performance_schema_current_thread_id(
    struct mylite_db *database,
    char *thread_id_text,
    size_t thread_id_text_size
);
static bool performance_schema_show_status_descriptor_visible(
    const struct mylite_execution_show_status_descriptor *descriptor,
    bool global_scope
);
static bool performance_schema_status_name_is_command_counter(const char *name);
static bool performance_schema_status_name_is_session_only(const char *name);
static int append_sys_schema_system_table_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
);
static bool append_sys_schema_summary_system_table_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows,
    int *out_status
);
static int append_sys_schema_x_system_table_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
);
static bool append_sys_schema_x_summary_system_table_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows,
    int *out_status
);
static bool mysql_system_table_definition_has_no_rows(
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static bool mysql_system_table_definition_has_catalog_rows(
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static int append_mysql_engine_cost_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_mysql_plugin_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_mysql_server_cost_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_sys_config_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_version_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_innodb_lock_waits_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_host_summary_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_host_summary_by_file_io_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_host_summary_by_file_io_type_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_host_summary_by_stages_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_host_summary_by_statement_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_host_summary_by_statement_type_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_user_summary_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_user_summary_by_file_io_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_user_summary_by_file_io_type_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_user_summary_by_stages_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_user_summary_by_statement_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_user_summary_by_statement_type_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_wait_classes_global_by_avg_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_wait_classes_global_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_waits_by_host_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_waits_by_user_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_waits_global_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_innodb_buffer_stats_by_schema_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_innodb_buffer_stats_by_table_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_io_by_thread_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_io_global_by_file_by_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_io_global_by_file_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_io_global_by_wait_by_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_io_global_by_wait_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_latest_file_io_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_memory_by_host_by_current_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_memory_by_thread_by_current_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_memory_by_user_by_current_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_memory_global_by_current_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_memory_global_total_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_metrics_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_processlist_system_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    struct information_schema_row_set *rows
);
static int append_sys_session_ssl_status_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_statement_digest_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *view_name
);
static int append_sys_x_ps_digest_avg_latency_distribution_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_x_ps_digest_95th_percentile_by_avg_us_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_ps_check_lost_instrumentation_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_schema_auto_increment_columns_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_schema_auto_increment_columns_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_sys_schema_auto_increment_columns_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int append_sys_schema_auto_increment_columns_column_row(
    struct sys_schema_auto_increment_columns_context *context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
);
static bool sys_schema_auto_increment_columns_max_value(
    const char *data_type,
    bool is_unsigned,
    uint64_t *out_value
);
static int format_sys_schema_auto_increment_columns_ratio(
    struct mylite_db *database,
    int64_t auto_increment,
    uint64_t max_value,
    char *buffer,
    size_t buffer_size
);
static void sort_sys_schema_auto_increment_columns_default_order(
    struct information_schema_row_set *rows
);
static int compare_sys_schema_auto_increment_columns_rows(char *const *left, char *const *right);
static int append_sys_schema_index_statistics_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    bool formatted_latency
);
static int append_sys_schema_index_statistics_mysql_system_rows(
    struct sys_schema_index_statistics_context *context
);
static int append_sys_schema_index_statistics_mysql_system_table_rows(
    struct sys_schema_index_statistics_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static int append_sys_schema_index_statistics_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_sys_schema_index_statistics_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int append_sys_schema_index_statistics_base_index_rows(
    struct sys_schema_index_statistics_context *context,
    const struct mylite_catalog_table_descriptor *table
);
static int append_sys_schema_index_statistics_index_row(
    struct sys_schema_index_statistics_context *context,
    const char *schema_name,
    const char *table_name,
    const char *index_name
);
static int append_sys_x_ps_schema_table_statistics_io_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_x_ps_schema_table_statistics_io_mysql_system_rows(
    struct sys_schema_table_statistics_context *context
);
static int append_sys_x_ps_schema_table_statistics_io_mysql_system_table_row(
    struct sys_schema_table_statistics_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static int append_sys_x_ps_schema_table_statistics_io_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_sys_x_ps_schema_table_statistics_io_table_row_from_catalog(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int append_sys_x_ps_schema_table_statistics_io_table_row(
    struct sys_schema_table_statistics_context *context,
    const char *schema_name,
    const char *table_name
);
static int append_sys_schema_table_statistics_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    bool formatted_metrics,
    bool include_buffer_metrics
);
static int append_sys_schema_table_statistics_mysql_system_rows(
    struct sys_schema_table_statistics_context *context
);
static int append_sys_schema_table_statistics_mysql_system_table_row(
    struct sys_schema_table_statistics_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static int append_sys_schema_table_statistics_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_sys_schema_table_statistics_table_row_from_catalog(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int append_sys_schema_table_statistics_table_row(
    struct sys_schema_table_statistics_context *context,
    const char *schema_name,
    const char *table_name
);
static int append_sys_schema_redundant_indexes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_x_schema_flattened_keys_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_schema_redundant_indexes_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_sys_x_schema_flattened_keys_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_sys_schema_redundant_indexes_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int append_sys_x_schema_flattened_keys_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int build_sys_flattened_key_rows(
    struct sys_schema_redundant_indexes_context *context,
    const struct mylite_catalog_table_descriptor *table,
    struct sys_flattened_key_row_set *out_rows
);
static int append_sys_x_schema_flattened_key_rows(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row_set *key_rows
);
static int append_sys_schema_redundant_index_rows(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row_set *key_rows
);
static int append_sys_x_schema_flattened_key_row(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row *key_row
);
static int append_sys_schema_redundant_index_row(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row *redundant,
    const struct sys_flattened_key_row *dominant
);
static bool sys_schema_redundant_index_pair_matches(
    const struct sys_flattened_key_row *redundant,
    const struct sys_flattened_key_row *dominant
);
static bool sys_schema_redundant_columns_left_prefix(const char *left, const char *right);
static int append_sys_flattened_key_row_from_index(
    struct sys_schema_redundant_indexes_context *context,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index,
    struct sys_flattened_key_row_set *rows
);
static bool sys_flattened_key_index_supported(const struct loaded_index_info *index);
static int build_sys_flattened_key_index_columns(
    const struct loaded_index_info *index,
    char **out_columns
);
static int reserve_sys_flattened_key_rows(
    struct sys_flattened_key_row_set *rows,
    size_t required_capacity
);
static void sys_flattened_key_rows_deinit(struct sys_flattened_key_row_set *rows);
static int build_sys_redundant_drop_index_sql(
    const struct sys_flattened_key_row *redundant,
    char **out_sql
);
static int append_sys_schema_table_lock_waits_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_schema_tables_with_full_table_scans_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_schema_unused_indexes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_sys_schema_unused_indexes_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_sys_schema_unused_indexes_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int append_sys_schema_unused_indexes_index_rows(
    struct sys_schema_unused_indexes_context *context,
    const struct mylite_catalog_table_descriptor *table
);
static int append_sys_schema_unused_indexes_index_row(
    struct sys_schema_unused_indexes_context *context,
    const char *table_name,
    const struct loaded_index_info *index
);
static bool sys_schema_unused_indexes_index_supported(const struct loaded_index_info *index);
static int append_sys_schema_object_overview_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int collect_sys_schema_object_overview_builtin_groups(
    struct sys_schema_object_overview_context *context
);
static int collect_sys_schema_object_overview_mysql_system_index_groups(
    struct sys_schema_object_overview_context *context
);
static int append_sys_schema_object_overview_schema_groups(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_sys_schema_object_overview_table_groups(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int append_sys_schema_object_overview_base_table_index_groups(
    struct sys_schema_object_overview_context *context,
    const struct mylite_catalog_table_descriptor *table
);
static int increment_sys_schema_object_overview_group(
    struct sys_schema_object_overview_context *context,
    const char *schema_name,
    const char *object_type,
    uint64_t count
);
static int grow_sys_schema_object_overview_groups(struct sys_schema_object_overview_context *context
);
static void sort_sys_schema_object_overview_groups(
    struct sys_schema_object_overview_context *context
);
static int compare_sys_schema_object_overview_groups(
    const struct sys_schema_object_overview_group *left,
    const struct sys_schema_object_overview_group *right
);
static int append_sys_schema_object_overview_group_rows(
    struct sys_schema_object_overview_context *context
);
static int compare_optional_text(const char *left, const char *right);
static size_t mysql_system_table_primary_key_column_count(
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static int append_mysql_innodb_index_stats_builtin_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_mysql_innodb_index_stats_builtin_index_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    const char *distinct_count,
    const char *stat_description
);
static int append_mysql_innodb_table_stats_builtin_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
);
static int append_mysql_innodb_table_stats_builtin_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *row_count
);
static int append_mysql_system_table_catalog_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
);
static int append_mysql_system_table_catalog_schema(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
static int append_mysql_system_table_catalog_table(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int append_mysql_innodb_table_stats_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_mysql_innodb_index_stats_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_mysql_innodb_index_stats_index_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index
);
static int append_mysql_innodb_index_stats_generated_cluster_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_mysql_innodb_index_stats_diff_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count
);
static int append_mysql_innodb_index_stats_generated_diff_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
);
static int append_mysql_innodb_index_stats_page_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    const char *stat_name,
    const char *stat_description
);
static int append_mysql_innodb_index_stats_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    const char *stat_name,
    const char *stat_value,
    const char *sample_size,
    const char *stat_description
);
static int mysql_innodb_index_stats_prefix_count(
    struct mylite_db *database,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t *out_count
);
static int read_mysql_innodb_index_stats_distinct_count(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count,
    int64_t *out_count
);
static int build_mysql_innodb_index_stats_distinct_sql(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count,
    char **out_sql
);
static int append_mysql_innodb_index_stats_prefix_expression(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_index
);
static int append_mysql_innodb_index_stats_prefix_description(
    struct mylite_db *database,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count,
    char **out_description
);
static int append_mysql_innodb_index_stats_prefix_name(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_index
);
static const struct loaded_index_part *mysql_innodb_index_stats_missing_clustered_part(
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t missing_index
);
static int mysql_system_current_timestamp(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
);
static int mysql_system_current_timestamp2(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
);
static bool selected_schema_is_mysql_system_schema(const struct mylite_db *database);
static bool schema_name_is_mysql_system_schema(const char *schema_name);
static int resolve_information_schema_query(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct information_schema_query *out_query
);
