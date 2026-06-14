static int validate_if_value_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
);
static int validate_if_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
);
static int validate_if_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
);
static int validate_ifnull_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
);
static int validate_coalesce_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
);
static int validate_nullif_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
);
static int validate_isnull_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
);
static int validate_case_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static int validate_case_value_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static int validate_case_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int validate_case_argument_count_error_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_handled
);
static int validate_case_mod_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int validate_case_unary_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int validate_case_binary_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int validate_case_literal_value_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static bool case_value_expression_is_admitted(const struct mylite_sql_ast_node *expression);
static bool is_case_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_case_when_list_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_case_when_clause_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_case_else_clause_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_case_expression_kind(enum mylite_sql_ast_node_kind kind);
static void set_case_unsupported_error(struct mylite_db *database);
static void set_if_unsupported_error(struct mylite_db *database, const char *function_name);
static const char *if_function_name(const struct mylite_sql_ast_node *expression);
static bool is_if_non_function_value_expression(const struct mylite_sql_ast_node *expression);
static bool is_if_integer_literal_in_range(const struct mylite_sql_ast_node *literal);
static int if_validation_stack_push(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static void if_validation_stack_deinit(struct if_validation_stack *stack);
static bool is_scalar_projection_select_item_expression(const struct mylite_sql_ast_node *expression
);
static bool is_scalar_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_collate_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_projection_expression_without_collate(
    const struct mylite_sql_ast_node *expression
);
static bool is_row_constructor_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_user_variable_assignment_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_last_insert_id_set_projection_expression(const struct mylite_sql_ast_node *expression
);
static bool is_scalar_function_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_numeric_scalar_function_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_string_scalar_function_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_temporal_scalar_function_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_json_scalar_function_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_scalar_subquery_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_abs_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_sign_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_rounding_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_sqrt_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_angle_conversion_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_inverse_trig_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_direct_trig_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_atan_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_exp_log_power_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_base_conversion_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_bit_count_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_crc32_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_digest_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_hex_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_base64_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_compression_random_projection_expression(const struct mylite_sql_ast_node *expression
);
static bool scalar_projection_expression_contains_compression_random_function(
    const struct mylite_sql_ast_node *expression
);
static bool is_unhex_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_weight_string_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_uuid_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_uuid_value_projection_argument_supported(const struct mylite_sql_ast_node *expression
);
static bool is_char_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_binary_string_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_char_projection_argument_supported(const struct mylite_sql_ast_node *expression);
static bool is_char_projection_literal_supported(const struct mylite_sql_ast_node *literal);
static bool is_char_projection_unary_supported(const struct mylite_sql_ast_node *expression);
static bool is_format_truncate_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_length_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_case_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_trim_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_slice_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_search_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_strcmp_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_bitmask_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_concat_ws_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_replace_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_insert_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_reverse_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_string_quote_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_charset_collation_projection_expression(const struct mylite_sql_ast_node *expression
);
static bool is_string_metadata_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_conversion_projection_expression(const struct mylite_sql_ast_node *expression
);
static bool is_cast_binary_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_cast_convert_basic_projection_expression(const struct mylite_sql_ast_node *expression
);
static bool is_convert_binary_type_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_convert_using_binary_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_convert_using_charset_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_date_interval_second_projection_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_time_arithmetic_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_date_format_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_temporal_extract_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_elt_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_field_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_greatest_least_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_interval_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_regexp_like_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_regexp_string_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_json_valid_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_json_contains_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_json_extract_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_json_value_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_json_introspection_projection_expression(const struct mylite_sql_ast_node *expression
);
static bool is_json_unquote_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_json_quote_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_json_construction_projection_expression(const struct mylite_sql_ast_node *expression
);
static bool is_json_set_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_value_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_arithmetic_projection_expression(const struct mylite_sql_ast_node *expression
);
static bool is_scalar_bitwise_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_logical_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_comparison_projection_expression(const struct mylite_sql_ast_node *expression
);
static bool is_scalar_concat_projection_expression(const struct mylite_sql_ast_node *expression);
static bool scalar_arithmetic_projection_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack
);
static bool scalar_bitwise_projection_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack
);
static bool scalar_logical_projection_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack
);
static bool scalar_comparison_projection_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack
);
static bool is_scalar_sounds_like_operand_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_regexp_comparison_operand_expression(
    const struct mylite_sql_ast_node *expression
);
static bool scalar_concat_projection_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack
);
static bool is_scalar_arithmetic_operator(enum mylite_sql_ast_operator operator_kind);
static bool is_scalar_bitwise_operator(enum mylite_sql_ast_operator operator_kind);
static bool is_scalar_bitwise_unary_operator(enum mylite_sql_ast_operator operator_kind);
static bool is_scalar_logical_operator(enum mylite_sql_ast_operator operator_kind);
static bool is_scalar_logical_unary_operator(enum mylite_sql_ast_operator operator_kind);
static bool is_scalar_comparison_operator(enum mylite_sql_ast_operator operator_kind);
static bool is_scalar_is_operator(enum mylite_sql_ast_operator operator_kind);
static bool expression_is_unparenthesized_scalar_is(const struct mylite_sql_ast_node *expression);
static bool is_scalar_projection_literal_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_function_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_projection_attempt_expression(const struct mylite_sql_ast_node *expression);
static bool is_scalar_value_projection_attempt_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_scalar_arithmetic_attempt_expression(const struct mylite_sql_ast_node *expression);
static bool scalar_arithmetic_attempt_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack
);
static bool is_scalar_value_projection_attempt_operand(const struct mylite_sql_ast_node *expression
);
static bool is_scalar_numeric_function_attempt_operand(const struct mylite_sql_ast_node *expression
);
static int append_system_variable_read_warning(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
);
static int parse_system_variable_component(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    size_t *offset,
    struct system_variable_component *out_component
);
static int append_quoted_system_variable_byte(
    struct mylite_db *database,
    struct system_variable_component *component,
    size_t *component_length,
    char value
);
static bool system_variable_component_equals(
    const struct system_variable_component *component,
    const char *expected
);
static bool system_variable_component_is_empty(const struct system_variable_component *component);
static const struct mylite_sql_ast_node *unwrap_parenthesized_expression(
    const struct mylite_sql_ast_node *expression
);
static const struct mylite_sql_ast_node *unwrap_any_value_function_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_session_scalar_expression(const struct mylite_sql_ast_node *expression);
static int append_show_processlist_row(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    const struct mylite_processlist_session_snapshot *session,
    mylite_result *result
);
static int format_processlist_user_host(
    struct mylite_db *database,
    const char *identity,
    char *user,
    size_t user_size,
    char *host,
    size_t host_size
);
static int copy_show_processlist_info(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    char **out_info
);
static size_t statement_info_length_without_terminator(const char *sql, size_t sql_size);
static int append_show_processlist_warning(struct mylite_db *database);
static int append_information_schema_profiling_warning(struct mylite_db *database);
static int plan_diagnostics_show_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_diagnostics_show_limit *out_limit
);
static int convert_diagnostics_show_limit_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
);
static int append_show_diagnostics_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_diagnostics_show_limit *limit
);
static int append_show_diagnostics_row(
    struct mylite_db *database,
    mylite_result *result,
    const char *level,
    const struct mylite_diagnostic_record *record
);
static int append_show_count_warnings_row(struct mylite_db *database, mylite_result *result);
static int append_show_errors_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_diagnostics_show_limit *limit
);
static int append_show_count_errors_row(struct mylite_db *database, mylite_result *result);
static int previous_diagnostics_condition_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
);
static int previous_diagnostics_error_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
);
static bool diagnostics_has_error_condition(const struct mylite_diagnostics *diagnostics);
static bool diagnostic_record_level_is_error(const struct mylite_diagnostic_record *record);
static int format_uint64(
    struct mylite_db *database,
    uint64_t value,
    char *buffer,
    size_t buffer_size
);

static int plan_show_create_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_show_create_table *out_plan
);
static void planned_show_create_table_deinit(struct planned_show_create_table *plan);
static int execute_show_create_table_from_plan(
    struct mylite_db *database,
    const struct planned_show_create_table *plan,
    mylite_result *result
);
static int append_show_create_view_result(
    struct mylite_db *database,
    mylite_result *result,
    const char *view_name,
    const struct mylite_catalog_view_descriptor *view
);
static int execute_show_create_builtin_sys_view_statement(
    struct mylite_db *database,
    const struct mylite_execution_catalog_builtin_sys_view *view,
    bool schema_qualified,
    mylite_result **out_result
);
static int append_show_create_builtin_sys_view_result(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_execution_catalog_builtin_sys_view *view,
    bool schema_qualified
);
static int append_show_create_view_text_result(
    struct mylite_db *database,
    mylite_result *result,
    const char *view_name,
    const char *show_create_sql,
    const char *character_set_client,
    const char *collation_connection
);
static const struct mylite_execution_catalog_builtin_sys_view *find_builtin_sys_view_definition(
    const char *view_name
);
static const struct mylite_execution_catalog_builtin_sys_view *show_create_target_builtin_sys_view(
    const struct table_name_resolution *target
);
static bool show_create_target_was_schema_qualified(const struct mylite_sql_ast_node *statement);
static int show_create_statement_targets_selected_builtin_sys_view(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct mylite_execution_catalog_builtin_sys_view **out_view
);
static void populate_builtin_sys_view_target(
    struct table_name_resolution *target,
    const struct mylite_execution_catalog_builtin_sys_view *view
);
static int build_show_create_table_sql(
    struct mylite_db *database,
    const struct planned_show_create_table *plan,
    char **out_sql
);
static int append_show_create_table_column_definition(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    bool is_last_column
);
static int append_show_create_table_column_charset_collation(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    bool is_national
);
static int append_show_create_table_column_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
);
static int append_show_create_table_non_integer_column_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
);
static int append_show_create_table_binary_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
);
static int append_show_create_table_binary_blob_expression_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
);
static bool binary_default_bytes_use_hex_literal(const char *bytes, size_t byte_count);
static int append_binary_default_hex_literal(
    struct mylite_dynamic_string *string,
    const char *bytes,
    size_t byte_count
);
static int append_mysql_quoted_binary_default_text(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_length
);
static int append_show_create_table_expression_default(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
);
static int append_show_create_table_text_expression_default(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
);
static int append_show_create_table_column_suffix(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    bool is_last_column
);
static int append_show_create_table_indexes(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
);
static int append_show_create_table_foreign_keys(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
);
static int append_show_create_table_check_constraints(
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
);
static int append_show_create_table_table_options(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
);
static bool show_create_table_should_append_default_collation(
    const struct planned_show_create_table *plan
);
static int append_show_create_table_auto_increment_option(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
);
static int append_show_create_table_comment_option(
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
);
static int append_show_create_table_storage_statistics_options(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
);
static int append_show_create_table_integer_option(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const char *prefix,
    int64_t value
);
static bool show_create_table_has_auto_increment(const struct planned_show_create_table *plan);
static int append_show_create_table_index(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan,
    const struct loaded_index_info *index,
    bool is_last_index
);
static int append_show_create_table_index_header(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan,
    const struct loaded_index_info *index
);
static const char *show_create_table_index_display_name(
    const struct planned_show_create_table *plan,
    const struct loaded_index_info *index
);
static bool show_create_foreign_key_name_is_generated(
    const struct planned_show_create_table *plan,
    const struct loaded_foreign_key_info *foreign_key
);
static int append_show_create_table_index_options(
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *index
);
static int append_show_create_table_index_part(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *part,
    bool needs_comma
);
static int append_show_create_table_foreign_key(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key,
    bool is_last_foreign_key
);
static int append_show_create_table_foreign_key_parts(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key,
    bool child_columns
);
static int append_show_create_table_foreign_key_rule(
    struct mylite_dynamic_string *string,
    const char *timing,
    const char *rule
);
static int index_display_group(const struct loaded_index_info *index);
static int show_create_table_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char **out_type_text
);
static int show_descriptor_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
);
static int show_descriptor_numeric_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char **out_type_text,
    bool *out_matched
);
static bool show_descriptor_temporal_type_text(
    const char *logical_type,
    const char **out_type_text
);
static int format_decimal_descriptor_type_text(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size,
    const struct decimal_type_info *info,
    const char **out_type_text
);
static int format_approximate_descriptor_type_text(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size,
    const struct approximate_type_info *info,
    const char **out_type_text
);
static int format_varchar_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
);
static int format_char_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
);
static int format_enum_descriptor_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
);
static int format_set_descriptor_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
);
static int format_text_family_type_text(const char *logical_type, const char **out_type_text);
static int format_binary_string_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
);
static const char *integer_descriptor_display_text(const char *logical_type);
static int build_show_create_database_sql(
    const struct mylite_catalog_schema_descriptor *schema,
    char **out_sql
);

static int maybe_finish_create_if_not_exists_noop(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct planned_create_table *plan,
    bool *out_finished
);
static int maybe_finish_create_temporary_if_not_exists_noop(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct planned_create_table *plan,
    bool *out_finished
);
static bool create_table_has_if_not_exists(const struct mylite_sql_ast_node *statement);
static const struct mylite_sql_ast_node *create_table_options_node(
    const struct mylite_sql_ast_node *statement
);
static bool drop_table_has_if_exists(const struct mylite_sql_ast_node *statement);
