static int bind_insert_row(sqlite3_stmt *statement, const struct planned_insert *plan, size_t row);
static int handle_insert_unique_key_conflict(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index
);
static int handle_insert_duplicate_key_update(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *insert_statement,
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
);
static int handle_insert_duplicate_update_constraint(
    struct mylite_db *database,
    int sqlite_rc,
    const struct planned_insert *plan,
    size_t row_index
);
static int handle_replace_unique_key_conflict(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *insert_statement,
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
);
static int replace_conflicting_row_matches_plan(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    bool *out_matches
);
static int build_replace_conflicting_row_select_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    char **out_sql
);
static int compare_replace_conflicting_row(
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    bool *out_matches
);
static bool sqlite_column_matches_planned_value(
    sqlite3_stmt *statement,
    int column_index,
    const struct planned_value *value
);
static int delete_replace_conflicting_row(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    size_t row_index,
    sqlite3_int64 *out_deleted_rows
);
static int build_replace_conflicting_row_delete_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    char **out_sql
);
static int step_insert_row(sqlite3_stmt *statement, int *out_sqlite_rc);
static bool sqlite_status_is_constraint(int sqlite_rc);
static int reset_insert_statement_after_constraint(sqlite3_stmt *statement);
static int handle_check_constraint_violation(
    struct mylite_db *database,
    int64_t table_id,
    bool ignore_errors,
    bool *out_was_check_violation
);
static int handle_generated_not_null_violation(
    struct mylite_db *database,
    int sqlite_rc,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    bool *out_was_generated_not_null_violation
);
static int handle_generated_range_constraint_violation(
    struct mylite_db *database,
    struct generated_constraint_columns columns,
    size_t row_number,
    bool *out_was_generated_range_violation
);
static bool sqlite_error_message_has_check_constraint(
    const char *message,
    const char **out_physical_name
);
static bool sqlite_error_message_has_not_null_column(
    const char *message,
    const char **out_column_name,
    size_t *out_column_name_length
);
static bool generated_range_constraint_column_index(
    const char *physical_name,
    size_t *out_column_index
);
static int set_or_warn_check_constraint_violation(
    struct mylite_db *database,
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    bool ignore_errors
);
static int find_insert_unique_key_conflict(
    struct mylite_db *database,
    int sqlite_step_rc,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info **out_conflicting_index,
    char *value_text,
    size_t value_text_size
);
static int insert_unique_key_row_has_null_part(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *index,
    bool *out_has_null_part
);
static int apply_insert_duplicate_key_update(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    struct insert_execution_counters *counters
);
static int validate_insert_duplicate_key_assignment_conflicts(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    const struct planned_value *assignment_values
);
static int fetch_insert_duplicate_current_row_values(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    struct planned_value **out_values
);
static int build_insert_duplicate_current_row_select_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    char **out_sql
);
static int project_insert_duplicate_updated_row_values(
    struct mylite_db *database,
    const struct planned_insert *plan,
    struct insert_duplicate_projected_row_inputs inputs,
    struct planned_value **out_values
);
static bool insert_duplicate_index_has_assigned_part(
    const struct planned_insert *plan,
    const struct loaded_index_info *index
);
static int insert_duplicate_key_tuple_has_null_part(
    const struct planned_insert *plan,
    const struct loaded_index_info *index,
    const struct planned_value *values,
    bool *out_has_null_part
);
static int unique_key_tuple_exists_excluding_insert_duplicate_row(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *candidate,
    const struct loaded_index_info *conflicting_index,
    const struct planned_value *projected_values,
    bool *out_exists
);
static int build_unique_key_lookup_excluding_insert_duplicate_row_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *candidate,
    const struct loaded_index_info *conflicting_index,
    char **out_sql
);
static int append_insert_duplicate_key_predicate_sql_from_parameters(
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *index,
    size_t first_parameter
);
static int bind_insert_duplicate_key_tuple_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct loaded_index_info *index,
    const struct planned_value *values,
    size_t value_count
);
static int convert_insert_duplicate_update_value(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct planned_value *current_values,
    const struct planned_insert_duplicate_update_assignment *duplicate_assignment,
    struct planned_value *out_value
);
static int convert_insert_duplicate_arithmetic_value(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct planned_value *current_values,
    const struct planned_insert_duplicate_update_assignment *duplicate_assignment,
    struct planned_value *out_value
);
static int parse_insert_duplicate_arithmetic_delta(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_delta
);
static int assign_insert_duplicate_arithmetic_result(
    struct mylite_db *database,
    const struct insert_duplicate_arithmetic_result_request *request,
    struct planned_value *out_value
);
static void set_insert_duplicate_arithmetic_delta_out_of_range_error(struct mylite_db *database);
static void set_insert_duplicate_arithmetic_range_error(
    struct mylite_db *database,
    const struct insert_duplicate_arithmetic_range_error *error
);
static bool insert_duplicate_update_has_same_column_arithmetic(const struct planned_insert *plan);
static bool insert_duplicate_update_allows_null_adjustment(
    const struct mylite_db *database,
    const struct planned_insert *plan,
    const struct mylite_catalog_column_descriptor *column
);
static int convert_insert_duplicate_update_values(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    struct planned_value **out_values
);
static void insert_duplicate_update_values_deinit(
    struct planned_value **values,
    size_t value_count
);
static int copy_planned_value(
    struct mylite_db *database,
    const struct planned_value *source,
    struct planned_value *out_value
);
static int materialize_insert_duplicate_row_scalar_expression(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct planned_row_scalar_expression *source,
    struct planned_row_scalar_expression *out_expression
);
static int build_insert_duplicate_update_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    const struct planned_value *assignment_values,
    char **out_sql
);
static int append_insert_duplicate_key_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *conflicting_index,
    size_t *next_parameter
);
static int append_insert_duplicate_assignments_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert *plan,
    size_t *next_parameter
);
static int append_insert_duplicate_changed_conditions_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert *plan,
    const struct planned_value *assignment_values,
    size_t *next_parameter
);
static int append_insert_duplicate_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    const struct planned_value *assignment_value,
    size_t *next_parameter
);
static int bind_insert_duplicate_update_parameters(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    const struct planned_value *assignment_values
);
static int bind_insert_duplicate_assignment_parameters(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_insert *plan,
    size_t row_index,
    const struct planned_value *assignment_values
);
static int bind_insert_duplicate_changed_condition_parameters(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_insert *plan,
    size_t row_index,
    const struct planned_value *assignment_values
);
static int handle_update_unique_key_conflict(
    struct mylite_db *database,
    const struct planned_update *executable_plan,
    const struct planned_update *plan,
    int sqlite_rc
);
static int handle_update_multiple_unique_key_conflict(
    struct mylite_db *database,
    const struct planned_update *executable_plan,
    const struct planned_update *plan,
    int sqlite_rc
);
static bool update_unique_key_index_has_assignment(
    const struct planned_update *plan,
    const struct loaded_index_info *index
);
static const struct planned_value *update_unique_key_assignment_value_for_part(
    const struct planned_update *executable_plan,
    const struct planned_update *plan,
    const struct loaded_index_part *part
);
static int unique_key_tuple_exists(
    struct mylite_db *database,
    const char *physical_table_name,
    const struct loaded_index_info *index,
    const struct planned_value *values,
    size_t value_count,
    bool *out_exists
);
static int build_unique_key_lookup_sql(
    const char *physical_table_name,
    const struct loaded_index_info *index,
    char **out_sql
);
static bool loaded_index_contains_column_id(
    const struct loaded_index_info *index,
    int64_t column_id
);
static int unique_key_single_value_exists(
    struct mylite_db *database,
    const char *physical_table_name,
    const struct loaded_index_info *index,
    const struct planned_value *value,
    bool *out_exists
);
static int find_update_unique_key_conflict_tuple(
    struct mylite_db *database,
    const struct planned_update *executable_plan,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    char *destination,
    size_t destination_size,
    bool *out_found
);
static int build_update_unique_key_conflict_sql(
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    char **out_sql
);
static int build_update_unique_key_internal_conflict_sql(
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    char **out_sql
);
static int append_update_unique_key_internal_conflict_alias_list(
    struct mylite_dynamic_string *string,
    size_t part_count
);
static int append_update_unique_key_internal_conflict_select_list(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    size_t *next_parameter
);
static int append_update_unique_key_internal_conflict_not_null_filter(
    struct mylite_dynamic_string *string,
    size_t part_count
);
static int append_update_unique_key_conflict_select_list(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    size_t *next_parameter
);
static int append_update_unique_key_conflict_candidate_source(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
);
static int append_update_unique_key_conflict_target_filter(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
static int append_update_unique_key_conflict_exists_clause(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    const char *conflict_alias,
    size_t *next_parameter
);
static int append_update_unique_key_conflict_expression(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_part *part,
    size_t *next_parameter
);
static int bind_update_unique_key_conflict_parameters(
    sqlite3_stmt *statement,
    const struct update_unique_key_conflict_bind_request *request
);
static int bind_update_unique_key_internal_conflict_parameters(
    sqlite3_stmt *statement,
    const struct update_unique_key_conflict_bind_request *request
);
static int bind_update_unique_key_assignment_parameters(
    sqlite3_stmt *statement,
    const struct update_unique_key_conflict_bind_request *request,
    int *parameter_index
);
static int format_sqlite_key_tuple(
    sqlite3_stmt *statement,
    const struct loaded_index_part *parts,
    size_t part_count,
    char *destination,
    size_t destination_size
);
static int format_sqlite_key_part(
    sqlite3_stmt *statement,
    const struct loaded_index_part *part,
    int column_index,
    char *destination,
    size_t destination_size
);
static int append_formatted_key_part(
    char *destination,
    size_t destination_size,
    size_t *used,
    size_t part_index,
    const char *value_text
);
static int bind_unique_key_tuple_parameters(
    sqlite3_stmt *statement,
    const struct loaded_index_info *index,
    const struct planned_value *values,
    size_t value_count
);
static int format_key_tuple(
    const struct loaded_index_info *index,
    const struct planned_value *values,
    size_t value_count,
    char *destination,
    size_t destination_size
);
static int format_key_part_value(
    const struct loaded_index_part *part,
    const struct planned_value *value,
    char *destination,
    size_t destination_size
);
static int format_blob_key_part_value(
    const struct loaded_index_part *part,
    const struct planned_value *value,
    bool trim_fixed_binary_padding,
    char *destination,
    size_t destination_size
);
static int format_text_prefix_key_part_value(
    const struct loaded_index_part *part,
    const struct planned_value *value,
    char *destination,
    size_t destination_size
);
static int index_part_prefix_length(
    const struct loaded_index_part *part,
    size_t *out_prefix_length
);
static size_t limited_copy_length(size_t value_length, size_t maximum_length);
static int format_key_value(
    const struct planned_value *value,
    char *destination,
    size_t destination_size
);
static bool loaded_index_part_uses_binary_key_display(const struct loaded_index_part *part);
static size_t binary_key_display_byte_count(
    const struct loaded_index_part *part,
    const void *bytes,
    size_t byte_count
);
static int format_binary_key_bytes(
    const void *bytes,
    size_t byte_count,
    char *destination,
    size_t destination_size
);
static int bind_select_parameters(sqlite3_stmt *statement, const struct planned_select *plan);
static int bind_select_parameters_at(
    sqlite3_stmt *statement,
    const struct planned_select *plan,
    int *parameter_index
);
static int bind_select_parameter_work_stack(
    sqlite3_stmt *statement,
    struct select_parameter_bind_stack *stack,
    int *parameter_index
);
static int bind_select_parameter_work_item(
    sqlite3_stmt *statement,
    struct select_parameter_bind_stack *stack,
    const struct select_parameter_bind_item *item,
    int *parameter_index
);
static int push_select_plan_parameter_work(
    struct select_parameter_bind_stack *stack,
    const struct planned_select *plan
);
static int push_select_source_list_parameter_work(
    struct select_parameter_bind_stack *stack,
    const struct planned_select_source *sources,
    size_t source_count
);
static int bind_select_limit_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_limit *limit,
    int *parameter_index
);
static int select_parameter_bind_stack_push(
    struct select_parameter_bind_stack *stack,
    struct select_parameter_bind_item item
);
static void select_parameter_bind_stack_deinit(struct select_parameter_bind_stack *stack);
static int bind_select_join_condition_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_join_condition *condition,
    int *parameter_index
);
static int bind_select_source_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_source *sources,
    size_t source_count,
    int *parameter_index
);
static int bind_row_scalar_select_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_select *plan
);
static int bind_count_having_select_parameters(
    sqlite3_stmt *statement,
    const struct planned_count_having_select *plan
);
static int bind_row_scalar_select_parameters_at(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_select *plan,
    int *parameter_index
);
static int bind_row_scalar_select_source_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_select *plan,
    int *parameter_index
);
static int bind_row_scalar_tableless_filter_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_select *plan,
    int *parameter_index
);
static int bind_select_order_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_order *order,
    int *parameter_index
);
static int bind_insert_select_materialize_parameters(
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan
);
static int bind_insert_select_compound_parameters(
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan
);
static int bind_row_scalar_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_non_concat_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_window_function_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_window_function_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *argument,
    int *parameter_index
);
static int bind_row_scalar_rand_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_conversion_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_conversion_base_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_integer_arithmetic_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_integer_arithmetic_frame_parameters(
    sqlite3_stmt *statement,
    struct row_scalar_integer_arithmetic_bind_stack *stack,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static void row_scalar_integer_arithmetic_bind_stack_deinit(
    struct row_scalar_integer_arithmetic_bind_stack *stack
);
static int row_scalar_integer_arithmetic_bind_stack_push(
    struct row_scalar_integer_arithmetic_bind_stack *stack,
    const struct planned_row_scalar_expression *expression
);
static int bind_row_scalar_field_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_elt_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_reversed_arguments_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_interval_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_char_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_date_format_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_date_interval_second_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_datediff_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_timediff_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_addtime_subtime_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_timestampdiff_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_timestamp_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_length_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_codepoint_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_case_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_trim_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_unix_timestamp_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_temporal_extract_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_sec_to_time_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_from_unixtime_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_temporal_constructor_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_slice_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_padding_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_bitmask_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_search_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_replace_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_insert_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_reverse_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_soundex_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_string_quote_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_substring_index_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_find_in_set_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_strcmp_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_regexp_like_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_valid_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_search_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_member_of_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_extract_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_value_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_unquote_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_quote_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_extract_document_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_introspection_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_length_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_depth_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_storage_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_keys_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_type_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_pretty_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_set_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_set_document_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_set_value_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_json_constructor_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_control_flow_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_control_flow_arguments(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    enum row_scalar_control_flow_bind_mode mode,
    int *parameter_index
);
static int bind_row_scalar_control_flow_frame_parameters(
    sqlite3_stmt *statement,
    struct row_scalar_control_flow_bind_stack *stack,
    const struct row_scalar_control_flow_bind_frame *frame,
    int *parameter_index
);
static int bind_row_scalar_control_flow_value_or_column_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int row_scalar_control_flow_bind_stack_push_arguments(
    struct row_scalar_control_flow_bind_stack *stack,
    const struct planned_row_scalar_expression *expression,
    enum row_scalar_control_flow_bind_mode mode
);
static int row_scalar_control_flow_bind_stack_push(
    struct row_scalar_control_flow_bind_stack *stack,
    struct row_scalar_control_flow_bind_frame frame
);
static void row_scalar_control_flow_bind_stack_deinit(
    struct row_scalar_control_flow_bind_stack *stack
);
static int bind_row_scalar_left_right_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_substring_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_hex_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_unhex_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_base64_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_registered_function_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_uuid_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_uuid_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *argument,
    size_t argument_index,
    int *parameter_index
);
static int bind_row_scalar_uuid_inner_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static int bind_row_scalar_uuid_leaf_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *argument,
    int *parameter_index
);
static int bind_count_parameters(sqlite3_stmt *statement, const struct planned_count *plan);
static int bind_count_derived_source_parameters(
    sqlite3_stmt *statement,
    const struct planned_count *plan,
    int *parameter_index
);
static int bind_count_derived_compound_parameters(
    sqlite3_stmt *statement,
    const struct planned_count_derived_compound *compound_plan,
    int *parameter_index
);
static int bind_count_derived_compound_branch_parameters(
    sqlite3_stmt *statement,
    const struct planned_count_derived_compound_branch *branch,
    int *parameter_index
);
static int bind_column_aggregate_parameters(
    sqlite3_stmt *statement,
    const struct planned_column_aggregate_select *plan
);
static int bind_column_aggregate_item_parameters(
    sqlite3_stmt *statement,
    const struct planned_column_aggregate *item,
    int *parameter_index
);
static int bind_grouped_aggregate_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan
);
static int bind_grouped_aggregate_count_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan
);
static int bind_grouped_aggregate_rollup_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan
);
static int bind_grouped_aggregate_parameters_at(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    bool include_limit,
    int *parameter_index
);
static int bind_grouped_aggregate_parameters_with_limit(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    bool include_limit
);
static int bind_grouped_projection_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    int *parameter_index
);
static int bind_grouped_having_aggregate_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate_item *item,
    int *parameter_index
);
static int bind_grouped_aggregate_row_scalar_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate_item *item,
    int *parameter_index
);
static int bind_grouped_key_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    int *parameter_index
);
static int bind_grouped_order_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    int *parameter_index
);
static int bind_delete_parameters(sqlite3_stmt *statement, const struct planned_delete *plan);
static int bind_update_parameters(sqlite3_stmt *statement, const struct planned_update *plan);
static int bind_update_multiple_assignment_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
);
static int bind_update_assignment_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct planned_update *plan
);
static int bind_update_date_interval_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
);
static int bind_update_auto_update_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
);
static int bind_joined_update_source_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
);
static int bind_update_multiple_changed_condition_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
);
static int bind_update_row_scalar_changed_condition_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_row_scalar_expression *expression
);
static int bind_update_changed_condition_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct planned_update *plan
);
static int bind_update_matched_parameters(
    sqlite3_stmt *statement,
    const struct planned_update *plan
);
static int bind_planned_value_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct planned_value *value
);
static int bind_int64_parameter(sqlite3_stmt *statement, int parameter_index, int64_t value);
static int bind_text_parameter(sqlite3_stmt *statement, int parameter_index, const char *value);
static int append_selected_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    mylite_result *result,
    const struct mylite_catalog_column_descriptor *columns,
    size_t descriptor_count
);
static int append_selected_sqlite_row_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *columns,
    size_t descriptor_count,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
);
static int append_selected_sqlite_row_value_with_descriptor(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *column,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
);
static int append_selected_sqlite_integer_value(
    sqlite3_stmt *statement,
    size_t column_index,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
);
static int append_selected_sqlite_float_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *column,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
);
static int append_selected_sqlite_text_value(
    sqlite3_stmt *statement,
    size_t column_index,
    struct mylite_result_cell *out_value
);
static int append_selected_sqlite_blob_value(
    sqlite3_stmt *statement,
    size_t column_index,
    struct mylite_result_cell *out_value
);
static int choose_sqlite_rowid_alias(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *unsupported_message,
    const char **out_alias
);
static bool column_name_exists(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *name
);
static int read_show_table_status_row_count(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t *out_count
);
static int build_show_table_status_count_sql(
    const struct mylite_catalog_table_descriptor *table,
    char **out_sql
);
static int format_show_table_status_integer(
    struct mylite_db *database,
    int64_t value,
    char *buffer,
    size_t buffer_size
);
