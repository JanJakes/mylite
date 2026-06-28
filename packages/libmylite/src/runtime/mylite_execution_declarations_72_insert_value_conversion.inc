static int collect_insert_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    const struct planned_insert *plan,
    const struct mylite_sql_ast_node *row_list,
    size_t **out_indexes,
    size_t *out_index_count
);
static bool insert_column_list_is_omitted(const struct mylite_sql_ast_node *column_list);
static bool insert_row_list_first_row_is_empty(const struct mylite_sql_ast_node *row_list);
static size_t count_visible_insert_target_columns(const struct planned_insert *plan);
static void collect_visible_insert_target_indexes(
    const struct planned_insert *plan,
    size_t *indexes
);
static int collect_explicit_insert_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    const struct planned_insert *plan,
    size_t *indexes,
    size_t column_count
);
static int collect_insert_set_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct planned_insert *plan,
    const char *unsupported_qualified_target_message,
    size_t **out_indexes,
    size_t *out_index_count
);
static int check_insert_target_duplicate(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    const size_t *target_indexes,
    size_t target_count
);
static int check_insert_omitted_columns(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const size_t *target_indexes,
    size_t target_count
);
static int validate_insert_row_shapes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row_list,
    size_t target_count
);
static int initialize_insert_auto_increment_plan(
    struct mylite_db *database,
    struct planned_insert *plan
);
static int initialize_insert_auto_increment_range(
    struct mylite_db *database,
    struct planned_insert *plan
);
static bool insert_auto_increment_column_is_indexed(const struct planned_insert *plan);
static int plan_insert_rows(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row_list,
    const size_t *target_indexes,
    size_t target_count,
    struct planned_insert *plan
);
static int plan_insert_row(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row_node,
    size_t row_number,
    const size_t *target_indexes,
    size_t target_count,
    struct planned_insert *plan,
    struct planned_insert_row *out_row
);
static int plan_insert_auto_increment_values(
    struct mylite_db *database,
    struct planned_insert *plan
);
static int load_insert_target_foreign_key_infos(
    struct mylite_db *database,
    struct planned_insert *plan
);
static int plan_insert_string_key_values(
    struct mylite_db *database,
    const struct planned_insert *plan
);
static int plan_insert_duplicate_update(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert *plan
);
static int plan_insert_duplicate_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment,
    struct planned_insert *plan,
    size_t assignment_index
);
static int plan_insert_duplicate_assignment_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value,
    struct planned_insert *plan,
    struct planned_insert_duplicate_update_assignment *duplicate_assignment
);
static int plan_insert_duplicate_values_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_insert *plan,
    struct planned_insert_duplicate_update_assignment *duplicate_assignment
);
static int plan_insert_duplicate_values_reference_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column,
    struct planned_insert *plan,
    size_t *out_column_index
);
static bool insert_duplicate_values_reference_columns_are_copy_compatible(
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column
);
static int plan_insert_duplicate_same_column_arithmetic(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_insert *plan,
    struct planned_insert_duplicate_update_assignment *duplicate_assignment,
    bool *out_planned
);
static int plan_insert_duplicate_row_scalar_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_insert *plan,
    struct planned_insert_duplicate_update_assignment *duplicate_assignment,
    bool *out_planned
);
static int validate_insert_duplicate_arithmetic_assignment_target(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t column_index
);
static int plan_insert_duplicate_update_key(
    struct mylite_db *database,
    struct planned_insert *plan
);
static int reject_insert_duplicate_parent_foreign_key_assignments(
    struct mylite_db *database,
    const struct planned_insert *plan
);
static bool insert_duplicate_assignment_targets_key(const struct planned_insert *plan);
static bool insert_duplicate_assignment_targets_auto_increment(const struct planned_insert *plan);
static bool insert_duplicate_assignment_targets_column_id(
    const struct planned_insert *plan,
    int64_t column_id
);
static bool insert_duplicate_column_index_is_keyed(
    const struct planned_insert *plan,
    size_t column_index
);
static bool insert_duplicate_assignment_is_noop(
    const struct planned_insert_duplicate_update_assignment *assignment
);
static bool insert_duplicate_assignment_is_same_column_values_reference(
    const struct planned_insert_duplicate_update_assignment *assignment
);
static size_t count_executable_insert_duplicate_assignments(const struct planned_insert *plan);
static void set_insert_duplicate_arithmetic_unsupported_error(struct mylite_db *database);
static int validate_update_string_key_value(
    struct mylite_db *database,
    const struct planned_update *plan
);
static int validate_update_multiple_string_key_values(
    struct mylite_db *database,
    const struct planned_update *plan
);
static int count_insert_auto_increment_modes(
    const struct planned_insert *plan,
    struct insert_auto_increment_mode_counts *out_counts
);
static int plan_insert_auto_increment_row_value(
    struct mylite_db *database,
    struct planned_insert *plan,
    const struct integer_column_range *range,
    size_t row_index,
    int64_t *next_value
);
static int generated_auto_increment_value_for_row(
    const struct planned_insert *plan,
    size_t row_index,
    bool *out_generated
);
static int assign_generated_auto_increment_value(
    struct mylite_db *database,
    struct planned_insert *plan,
    size_t row_index,
    int64_t generated_value
);
static int generated_auto_increment_value_for_lower_bound(
    struct mylite_db *database,
    int64_t lower_bound,
    const struct integer_column_range *range,
    int64_t *out_value
);
static int next_auto_increment_after_generated_value(
    const struct mylite_db *database,
    int64_t value,
    const struct integer_column_range *range,
    int64_t *out_next
);
static int next_auto_increment_after_explicit_insert_value(
    int64_t current_next,
    int64_t value,
    const struct integer_column_range *range,
    int64_t *out_next
);
static int next_auto_increment_after_update_value(
    struct mylite_db *database,
    int64_t current_next,
    int64_t value,
    const struct integer_column_range *range,
    int64_t *out_next
);
static int execute_insert_plan_rows(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    struct insert_execution_counters *counters
);
static int execute_insert_plan_row_with_retries(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
);
static int finish_successful_insert_plan_row(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
);
static int handle_insert_plan_constraint(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    size_t *replace_attempt_count,
    struct insert_execution_counters *counters,
    bool *out_row_complete
);
static int handle_replace_insert_constraint(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    size_t *replace_attempt_count,
    struct insert_execution_counters *counters
);
static int validate_insert_ignore_row_foreign_keys(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    bool *out_skip_row
);
static int validate_insert_ignore_row_foreign_key(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const struct loaded_foreign_key_info *foreign_key,
    size_t row_index,
    bool *out_violates
);
static int execute_insert_plan_row(
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    int *out_sqlite_step_rc
);
static int advance_auto_increment_after_insert_row(
    const struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    int64_t *auto_increment_next_after_rows
);
static int advance_auto_increment_after_duplicate_attempt(
    const struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    int64_t *auto_increment_next_after_rows
);
static int advance_auto_increment_after_ignored_duplicate(
    const struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    int64_t *auto_increment_next_after_rows
);
static void record_inserted_explicit_auto_increment(
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
);
static void record_inserted_generated_auto_increment(
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
);
static int append_insert_omitted_column_warnings(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const size_t *target_indexes,
    size_t target_count
);
static int plan_insert_set_row(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const size_t *target_indexes,
    size_t target_count,
    struct planned_insert *plan
);
static int allocate_insert_row_values(
    struct mylite_db *database,
    const struct planned_insert *plan,
    struct planned_insert_row *out_row
);
static int allocate_insert_column_value(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t column_index,
    struct planned_value *out_value
);
static int make_insert_ignore_implicit_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static bool dml_allows_missing_default_adjustment(
    const struct mylite_db *database,
    bool ignore_errors
);
static bool dml_allows_string_truncation_adjustment(
    const struct mylite_db *database,
    bool ignore_errors
);
static bool insert_allows_implicit_default_adjustment(
    const struct mylite_db *database,
    const struct planned_insert *plan
);
static int convert_insert_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    const char *diagnostic_column_name,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
);
static int convert_dml_constant_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    const char *diagnostic_column_name,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    bool warn_on_trailing_space_truncation,
    bool auto_increment_null_generates,
    bool allow_constant_arithmetic,
    bool *out_handled,
    struct planned_value *out_value
);
static bool dml_value_is_constant_scalar_expression(
    const struct mylite_sql_ast_node *value_node,
    bool allow_constant_arithmetic
);
static bool dml_scalar_expression_is_constant_arithmetic(
    const struct mylite_sql_ast_node *value_node
);
static bool dml_scalar_expression_contains_unsupported_source(
    const struct mylite_sql_ast_node *value_node
);
static bool dml_scalar_expression_child_is_syntax_metadata(
    const struct mylite_sql_ast_node *value_node,
    size_t child_index,
    const struct mylite_sql_ast_node *child
);
static int convert_dml_scalar_cell_for_column(
    struct mylite_db *database,
    const struct session_scalar_cell *cell,
    const struct mylite_catalog_column_descriptor *column,
    const char *diagnostic_column_name,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    bool warn_on_trailing_space_truncation,
    struct planned_value *out_value
);
static int convert_dml_scalar_cell_spatial_value(
    struct mylite_db *database,
    const struct session_scalar_cell *cell,
    struct planned_value *out_value
);
static int convert_dml_scalar_cell_text_value(
    struct mylite_db *database,
    const struct session_scalar_cell *cell,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_string_truncation_adjustment,
    bool warn_on_trailing_space_truncation,
    struct planned_value *out_value
);
static int convert_dml_scalar_cell_binary_value(
    struct mylite_db *database,
    const struct session_scalar_cell *cell,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_dml_scalar_cell_bit_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int copy_dml_scalar_cell_text(
    struct mylite_db *database,
    const struct session_scalar_cell *cell,
    const char *nul_message,
    bool allow_nul,
    char **out_text,
    size_t *out_text_length
);
static bool insert_value_is_unix_timestamp_arithmetic(const struct mylite_sql_ast_node *value_node);
static int convert_insert_unix_timestamp_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
);

struct unix_timestamp_arithmetic_messages {
    const char *unsupported;
    const char *delta_range;
};

static int evaluate_insert_unix_timestamp_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct unix_timestamp_arithmetic_messages *messages,
    struct scalar_arithmetic_value *out_value
);
static int evaluate_insert_unix_timestamp_delta(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct unix_timestamp_arithmetic_messages *messages,
    struct scalar_arithmetic_value *out_value
);
static int parse_insert_unix_timestamp_delta_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    const char *delta_range_message,
    struct scalar_arithmetic_value *out_value
);
static int finish_insert_unix_timestamp_integer_value(
    struct mylite_db *database,
    int64_t integer,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int finish_insert_unix_timestamp_text_value(
    struct mylite_db *database,
    int64_t integer,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
);
static int format_insert_unix_timestamp_integer_text(
    struct mylite_db *database,
    int64_t integer,
    char **out_text,
    size_t *out_text_length
);
static bool insert_unix_timestamp_now_node_is_admitted(const struct mylite_sql_ast_node *value_node
);
static bool insert_unix_timestamp_delta_node_is_admitted(
    const struct mylite_sql_ast_node *value_node
);
static int convert_statement_time_value_for_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value,
    bool *out_handled
);
static int convert_insert_value_for_plan(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
);
static int convert_insert_value_by_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    const char *diagnostic_column_name,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
);
static int convert_auto_increment_insert_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_null_insert_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool ignore_errors,
    struct planned_value *out_value
);
static int materialize_dml_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool adjust_missing_default,
    struct planned_value *out_value
);
static int materialize_integer_expression_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int materialize_bit_expression_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int materialize_character_expression_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool allow_nonspace_truncation,
    struct planned_value *out_value
);
static int materialize_scalar_expression_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool allow_nonspace_truncation,
    struct planned_value *out_value
);
static int materialize_scalar_expression_date_add_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int materialize_scalar_expression_concat_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool allow_nonspace_truncation,
    struct planned_value *out_value
);
static int materialize_scalar_expression_text_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool allow_nonspace_truncation,
    char *text,
    size_t text_length,
    struct planned_value *out_value
);
static bool parse_scalar_expression_date_add_default_text(
    const char *text,
    int64_t *out_interval,
    enum mylite_date_interval_unit *out_unit
);
static int append_scalar_expression_concat_literal_value(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const char **in_out_cursor
);
static const char *scalar_expression_concat_literal_end(const char *literal);
static int materialize_binary_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int validate_default_function_source_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
);
static int convert_default_function_value_for_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor *target_column,
    bool ignore_errors,
    struct planned_value *out_value
);
static int resolve_default_function_source_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_default_function_source_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *source_column,
    struct planned_value *out_value
);
static int materialize_default_function_current_timestamp_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *source_column,
    struct planned_value *out_value
);
static bool default_function_source_target_types_are_compatible(
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column
);
static const char *default_function_implicit_conversion_message(
    const struct mylite_catalog_column_descriptor *target_column
);
static int materialize_dml_missing_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_integer_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_integer_string_text(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int parse_dml_hex_literal_magnitude(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_magnitude,
    bool *out_overflow
);
static int convert_integer_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool is_negative,
    struct planned_value *out_value
);
static bool ascii_decimal_digit(unsigned char byte);
static int copy_dml_numeric_scan_digits(
    struct mylite_db *database,
    const char *text,
    const struct mylite_execution_dml_numeric_token_scan *scan,
    char **out_digits
);
static int dml_integer_value_exceeds_column_range(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    bool *out_exceeds_range
);
static int convert_enum_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    struct planned_value *out_value
);
static int convert_enum_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_numeric_ordinal_fallback,
    bool missing_is_error,
    enum enum_string_trailing_space_policy trailing_space_policy,
    struct planned_value *out_value
);
static void trim_trailing_ascii_spaces(char *text, size_t *text_length);
static int convert_enum_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool missing_is_error,
    struct planned_value *out_value
);
static int enum_literal_ordinal(
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_ordinal,
    bool *out_is_negative
);
static int copy_enum_label_value(
    struct mylite_db *database,
    const struct enum_label_descriptor *label,
    struct planned_value *out_value
);
static int copy_enum_no_match_value(struct mylite_db *database, struct planned_value *out_value);
static int make_enum_first_label_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_set_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    struct planned_value *out_value
);
static int convert_set_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_numeric_bitmap_fallback,
    bool missing_is_error,
    bool canonical_definition_order,
    struct planned_value *out_value
);
static int convert_empty_set_string_literal(
    struct mylite_db *database,
    const struct set_type_info *info,
    bool canonical_definition_order,
    struct planned_value *out_value
);
static int convert_set_member_list_literal(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    bool has_comma,
    const struct set_type_info *info,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_numeric_bitmap_fallback,
    bool missing_is_error,
    bool canonical_definition_order,
    struct planned_value *out_value
);
static int append_set_predicate_member_text(struct set_predicate_member_text_request request);
static int finish_set_member_list_conversion(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    bool has_comma,
    const struct set_type_info *info,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_numeric_bitmap_fallback,
    bool missing_is_error,
    bool canonical_definition_order,
    bool all_members_known,
    uint64_t bitmap,
    struct mylite_dynamic_string *display,
    struct planned_value *out_value
);
static int convert_set_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool missing_is_error,
    struct planned_value *out_value
);
static int copy_set_bitmap_value(
    struct mylite_db *database,
    const struct set_type_info *info,
    uint64_t bitmap,
    struct planned_value *out_value
);
static int copy_set_no_match_value(struct mylite_db *database, struct planned_value *out_value);
static int convert_varchar_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_nonspace_truncation,
    struct planned_value *out_value
);
static int convert_char_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_nonspace_truncation,
    struct planned_value *out_value
);
static int convert_text_family_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_nonspace_truncation,
    bool warn_on_trailing_space_truncation,
    struct planned_value *out_value
);
static int decode_dml_character_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const char *unsupported_message,
    const char *nul_message,
    bool allow_nul,
    char **out_text,
    size_t *out_text_length
);
static int copy_dml_character_literal_text(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    bool is_negative,
    char **out_text,
    size_t *out_text_length
);
static int convert_decimal_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_decimal_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    const struct decimal_type_info *info,
    struct planned_value *out_value
);
static int convert_decimal_string_text(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    const struct decimal_type_info *info,
    struct planned_value *out_value
);
static int convert_decimal_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool is_negative,
    const struct decimal_type_info *info,
    struct planned_value *out_value
);
static int convert_decimal_scanned_string_literal(
    struct mylite_db *database,
    const char *text,
    const struct mylite_execution_dml_numeric_token_scan *scan,
    bool truncated,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool adjust_errors,
    const struct decimal_type_info *info,
    struct planned_value *out_value
);
static int build_decimal_text_from_numeric_scan(
    struct mylite_db *database,
    const char *text,
    const struct mylite_execution_dml_numeric_token_scan *scan,
    const struct decimal_type_info *info,
    char **out_text,
    bool *out_is_out_of_range
);
static int64_t dml_numeric_scan_decimal_position(
    const struct mylite_execution_dml_numeric_token_scan *scan
);
static bool dml_numeric_scan_exceeds_decimal_range(
    const struct mylite_execution_dml_numeric_token_scan *scan,
    const struct decimal_type_info *info
);
static size_t dml_numeric_scan_decimal_text_size(
    const struct mylite_execution_dml_numeric_token_scan *scan,
    const struct decimal_type_info *info
);
static char dml_numeric_scan_virtual_digit(
    const char *digits,
    const struct mylite_execution_dml_numeric_token_scan *scan,
    int64_t digit_index
);
static bool dml_numeric_scan_has_nonzero_virtual_digit_at_or_after(
    const struct mylite_execution_dml_numeric_token_scan *scan,
    int64_t digit_index
);
static int convert_approximate_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_approximate_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    const struct approximate_type_info *info,
    struct planned_value *out_value
);
static int convert_approximate_string_text(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    const struct approximate_type_info *info,
    struct planned_value *out_value
);
static int convert_approximate_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool is_negative,
    const struct approximate_type_info *info,
    struct planned_value *out_value
);
static int convert_approximate_scanned_string_literal(
    struct mylite_db *database,
    const char *text,
    const struct mylite_execution_dml_numeric_token_scan *scan,
    bool truncated,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool adjust_errors,
    const struct approximate_type_info *info,
    struct planned_value *out_value
);
static int parse_approximate_scanned_value(
    struct mylite_db *database,
    const char *text,
    const struct mylite_execution_dml_numeric_token_scan *scan,
    double *out_value,
    bool *out_is_out_of_range
);
static int convert_adjusted_approximate_range_value(
    struct mylite_db *database,
    const struct approximate_type_info *info,
    bool is_negative,
    const char *column_name,
    size_t row_number,
    struct planned_value *out_value
);
static bool approximate_value_is_out_of_range(
    double value,
    const struct approximate_type_info *info
);
static double approximate_storage_value(double value, const struct approximate_type_info *info);
static double approximate_endpoint_value(const struct approximate_type_info *info, bool negative);
static int convert_date_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_time_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_datetime_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_timestamp_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_predicate_enum_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_predicate_set_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_year_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_year_integer_value(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_year_string_value(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_year_string_magnitude(const char *text, size_t text_length, uint32_t *out_year);
static bool year_string_accepts_trailing_hyphen(struct year_string_trailing_hyphen_request request);
static int convert_year_string_magnitude_by_length(
    struct year_string_magnitude_conversion conversion,
    uint32_t *out_year
);
static int convert_year_direct_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_value *out_value
);
static int convert_year_direct_string(const char *text, size_t text_length, uint32_t *out_year);
static int parse_year_value_magnitude(
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_magnitude,
    bool *out_is_negative,
    bool *out_parse_overflow
);
static bool convert_year_numeric_magnitude(
    uint64_t magnitude,
    bool is_negative,
    uint32_t *out_year
);
static int make_year_value(
    struct mylite_db *database,
    uint32_t year,
    struct planned_value *out_value
);
static int make_zero_year_value(struct mylite_db *database, struct planned_value *out_value);
static int make_zero_year_value_with_warning(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool incorrect_integer,
    char *value_text,
    struct planned_value *out_value
);
static bool year_text_is_canonical(const char *text, size_t text_length);
static int canonicalize_date_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_relaxed_date_storage_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static bool normalize_date_storage_text(
    const struct temporal_predicate_normalization_input *input,
    char *out_text,
    enum temporal_storage_truncation *out_truncation
);
static bool normalize_fractional_datetime_storage_text(
    const struct temporal_predicate_normalization_input *input,
    char *out_text
);
static bool normalize_date_only_datetime_storage_text(
    const struct temporal_predicate_normalization_input *input,
    char *out_text
);
static bool normalize_single_digit_hour_datetime_storage_text(
    const struct temporal_predicate_normalization_input *input,
    char *out_text
);
static bool datetime_storage_time_is_midnight(const char *text);
static char *copy_temporal_text(struct mylite_db *database, const char *text, size_t text_length);
static int canonicalize_time_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static bool normalize_time_numeric_text(
    const char *text,
    size_t text_length,
    char *out_text,
    size_t *out_text_length
);
static bool normalize_fractional_time_text(
    const char *text,
    size_t text_length,
    char *out_text,
    size_t *out_text_length
);
static int canonicalize_datetime_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_relaxed_datetime_storage_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    int target_offset_minutes,
    struct planned_value *out_value
);
static int canonicalize_timestamp_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_relaxed_timestamp_storage_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    int target_offset_minutes,
    struct planned_value *out_value
);
static bool session_sql_mode_is_strict(const struct mylite_db *database);
static bool temporal_sql_mode_is_strict(const struct mylite_db *database);
static bool temporal_sql_mode_has_no_zero_date(const struct mylite_db *database);
static bool temporal_sql_mode_has_no_zero_in_date(const struct mylite_db *database);
static bool temporal_sql_mode_has_allow_invalid_dates(const struct mylite_db *database);
static enum temporal_text_kind classify_date_text(const char *text, size_t text_length);
static enum temporal_text_kind classify_datetime_text(const char *text, size_t text_length);
static enum temporal_text_kind classify_temporal_date_components(
    uint32_t year,
    uint32_t month,
    uint32_t day
);
static int make_date_zero_with_warning(
    struct mylite_db *database,
    char *text,
    const char *column_name,
    size_t row_number,
    struct planned_value *out_value
);
static int make_datetime_zero_with_warning(
    struct mylite_db *database,
    char *text,
    const char *column_name,
    size_t row_number,
    struct planned_value *out_value
);
static int make_timestamp_zero_with_warning(
    struct mylite_db *database,
    char *text,
    const char *column_name,
    size_t row_number,
    struct planned_value *out_value
);
static bool predicate_date_text_admitted(
    const struct mylite_db *database,
    const char *text,
    size_t text_length
);
static bool predicate_datetime_text_admitted(
    const struct mylite_db *database,
    const char *text,
    size_t text_length
);
static bool predicate_timestamp_text_admitted(
    const struct mylite_db *database,
    const char *text,
    size_t text_length
);
static bool date_text_has_canonical_shape(const char *text, size_t text_length);
static bool date_text_is_zero_date(const char *text, size_t text_length);
static bool time_text_is_canonical_valid(const char *text, size_t text_length);
static bool time_text_has_canonical_shape(const char *text, size_t text_length);
static bool time_text_uses_canonical_hour_width(
    const char *text,
    size_t text_length,
    const uint32_t *hour
);
static bool time_text_to_seconds(const char *text, size_t text_length, int64_t *out_seconds);
static bool time_text_to_components(
    const char *text,
    size_t text_length,
    bool *out_is_negative,
    uint32_t *out_hour,
    uint32_t *out_minute,
    uint32_t *out_second
);
static bool datetime_text_is_canonical_valid(const char *text, size_t text_length);
static bool timestamp_text_is_canonical_valid(const char *text, size_t text_length);
static bool timestamp_text_in_supported_range(const char *text, size_t text_length);
static bool datetime_text_has_canonical_shape(const char *text, size_t text_length);
static bool datetime_text_is_zero_datetime(const char *text, size_t text_length);
static bool datetime_time_components_valid(uint32_t hour, uint32_t minute, uint32_t second);
static bool date_component_text_to_u32(const char *text, size_t length, uint32_t *out_value);
static bool date_year_month_day_valid(uint32_t year, uint32_t month, uint32_t day);
static bool date_year_is_leap(uint32_t year);
static int make_zero_date_value(struct mylite_db *database, struct planned_value *out_value);
static int make_zero_time_value(struct mylite_db *database, struct planned_value *out_value);
static int make_zero_datetime_value(struct mylite_db *database, struct planned_value *out_value);
static int make_zero_timestamp_value(struct mylite_db *database, struct planned_value *out_value);
static int make_current_timestamp_value(
    struct mylite_db *database,
    struct planned_value *out_value
);
static int make_sysdate_value(struct mylite_db *database, struct planned_value *out_value);
static int make_current_date_value(struct mylite_db *database, struct planned_value *out_value);
static int make_current_time_value(struct mylite_db *database, struct planned_value *out_value);
static int make_utc_timestamp_value(struct mylite_db *database, struct planned_value *out_value);
static int make_utc_date_value(struct mylite_db *database, struct planned_value *out_value);
static int make_utc_time_value(struct mylite_db *database, struct planned_value *out_value);
static int64_t current_timestamp_epoch(const struct mylite_db *database);
static int format_current_timestamp_text(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
);
static int format_sysdate_text(struct mylite_db *database, char *buffer, size_t buffer_size);
static int format_sysdate_text_with_precision(
    struct mylite_db *database,
    uint64_t precision,
    char *buffer,
    size_t buffer_size
);
static int format_current_date_text(struct mylite_db *database, char *buffer, size_t buffer_size);
static int format_current_time_text(struct mylite_db *database, char *buffer, size_t buffer_size);
static int format_utc_timestamp_text(struct mylite_db *database, char *buffer, size_t buffer_size);
static int format_utc_date_text(struct mylite_db *database, char *buffer, size_t buffer_size);
static int format_utc_time_text(struct mylite_db *database, char *buffer, size_t buffer_size);
static int format_session_timestamp_epoch_text(
    struct mylite_db *database,
    int64_t epoch,
    char *buffer,
    size_t buffer_size
);
static int current_timestamp_session_parts(struct mylite_db *database, struct tm *out_time_parts);
static int current_timestamp_utc_parts(struct mylite_db *database, struct tm *out_time_parts);
static int session_time_parts_from_epoch(
    struct mylite_db *database,
    int64_t epoch,
    struct tm *out_time_parts
);
static int utc_time_parts_from_epoch(
    struct mylite_db *database,
    int64_t epoch,
    struct tm *out_time_parts
);
static struct wall_clock_timestamp current_wall_clock_timestamp(void);
static int64_t current_wall_clock_epoch(void);
static int make_zero_approximate_value(struct planned_value *out_value);
static int parse_approximate_literal_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    bool is_negative,
    const struct approximate_type_info *info,
    const char *column_name,
    size_t row_number,
    double *out_value
);
static int assign_approximate_value(double value, struct planned_value *out_value);
static int copy_approximate_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int format_approximate_value_text(
    struct mylite_db *database,
    double value,
    const struct approximate_type_info *info,
    const char *context_name,
    char *buffer,
    size_t buffer_size
);
static int format_scaled_approximate_value_text(
    struct mylite_db *database,
    double value,
    const struct approximate_type_info *info,
    const char *context_name,
    char *buffer,
    size_t buffer_size
);
static int format_float_value_text(
    struct mylite_db *database,
    double value,
    const char *context_name,
    char *buffer,
    size_t buffer_size
);
static int assign_text_value(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    struct planned_value *out_value
);
static int make_json_null_value(struct mylite_db *database, struct planned_value *out_value);
static int make_empty_blob_value(struct mylite_db *database, struct planned_value *out_value);
static int convert_json_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    const char *diagnostic_column_name,
    struct planned_value *out_value
);
static int normalize_json_value(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    struct planned_value *out_value
);
static int assign_blob_value(
    struct mylite_db *database,
    char *bytes,
    size_t byte_count,
    struct planned_value *out_value
);
static int copy_text_value(
    struct mylite_db *database,
    const char *text,
    struct planned_value *out_value
);
static int copy_blob_value(
    struct mylite_db *database,
    const void *bytes,
    size_t byte_count,
    struct planned_value *out_value
);
static int canonicalize_decimal_text(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    bool is_negative,
    const struct decimal_type_info *info,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int parse_decimal_literal_parts(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    struct decimal_literal_parts *out_parts
);
static int build_decimal_digit_buffer(
    struct mylite_db *database,
    const char *text,
    const struct decimal_type_info *info,
    const struct decimal_literal_parts *parts,
    struct decimal_digit_buffer *out_buffer
);
static int round_decimal_digit_buffer_if_needed(
    struct mylite_db *database,
    const char *text,
    const struct decimal_type_info *info,
    const struct decimal_literal_parts *parts,
    struct decimal_digit_buffer *buffer
);
static bool decimal_value_is_out_of_range(
    const struct decimal_type_info *info,
    bool is_negative,
    const char *digits,
    size_t digit_count
);
static int handle_decimal_out_of_range(
    struct mylite_db *database,
    const struct decimal_type_info *info,
    const char *column_name,
    size_t row_number,
    bool is_negative,
    bool ignore_errors,
    struct planned_value *out_value
);
static int build_decimal_zero_value(
    struct mylite_db *database,
    const struct decimal_type_info *info,
    struct planned_value *out_value
);
static int build_decimal_endpoint_value(
    struct mylite_db *database,
    const struct decimal_type_info *info,
    bool is_negative,
    struct planned_value *out_value
);
static int assign_decimal_text_value(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    struct planned_value *out_value
);
static int copy_decimal_text_value(
    struct mylite_db *database,
    const char *text,
    struct planned_value *out_value
);
static bool decimal_digits_are_zero(const char *digits, size_t digit_count);
static int increment_decimal_digits(char *digits, size_t *digit_count, size_t capacity);
static int format_decimal_digits(
    struct mylite_db *database,
    const char *digits,
    size_t digit_count,
    bool is_negative,
    uint64_t scale,
    struct planned_value *out_value
);

struct string_literal_decode_messages {
    const char *unsupported;
    const char *nul;
};

static int decode_sql_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    const char *unsupported_message,
    const char *nul_message,
    char **out_text,
    size_t *out_text_length
);
static int decode_sql_string_literal_with_policy(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    const char *unsupported_message,
    const char *nul_message,
    bool allow_nul,
    char **out_text,
    size_t *out_text_length
);
static int append_decoded_sql_string_literal_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    struct mylite_dynamic_string *string,
    const struct string_literal_decode_messages *messages,
    bool allow_nul
);
static int append_decoded_sql_string_literal_span(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    struct mylite_dynamic_string *string,
    const struct string_literal_decode_messages *messages,
    bool allow_nul
);
static int append_decoded_sql_string_escape(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    char escaped_byte,
    const char *nul_message,
    bool allow_nul
);
static int convert_binary_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_bit_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int make_implicit_bit_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_bit_value_magnitude(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    uint64_t magnitude,
    bool is_negative,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int clip_bit_value_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool include_out_of_range_warning,
    struct planned_value *out_value
);
static int parse_bit_value_magnitude(
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_magnitude,
    bool *out_is_negative,
    bool *out_parse_overflow
);
static int parse_bit_literal_magnitude(
    const struct mylite_sql_source_span *span,
    uint64_t *out_magnitude,
    bool *out_parse_overflow
);
static int bit_string_bytes_to_magnitude(
    const char *bytes,
    size_t byte_count,
    uint64_t *out_magnitude,
    bool *out_parse_overflow
);
static int make_bit_blob_value(
    struct mylite_db *database,
    const struct bit_blob_value *bit_value,
    struct planned_value *out_value
);
static int format_bit_default_text(
    struct mylite_db *database,
    const char *column_name,
    uint64_t magnitude,
    char *buffer,
    size_t buffer_size
);
static int decode_binary_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    char **out_bytes,
    size_t *out_byte_count
);
static int decode_binary_bit_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_bytes,
    size_t *out_byte_count
);
static int decode_binary_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_bytes,
    size_t *out_byte_count
);
static int decode_quoted_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_bytes,
    size_t *out_byte_count
);
static int decode_0x_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_bytes,
    size_t *out_byte_count
);
static int hex_nibble_value(char byte, unsigned char *out_value);
static int convert_binary_bytes_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    char **bytes,
    size_t *byte_count
);
static int make_implicit_binary_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int validate_varchar_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct varchar_text_validation validation
);
static int convert_varchar_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct char_text_conversion *conversion,
    bool allow_nonspace_truncation
);
static int convert_char_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct char_text_conversion *conversion,
    bool allow_nonspace_truncation
);
static int validate_canonical_char_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct varchar_text_validation validation
);
static int validate_text_family_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct varchar_text_validation validation
);
static int convert_text_family_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct char_text_conversion *conversion,
    bool allow_nonspace_truncation,
    bool warn_on_trailing_space_truncation
);
static int utf8_prefix_byte_length_for_byte_limit(
    struct utf8_byte_prefix_request request,
    size_t *out_prefix_length
);
static int validate_canonical_decimal_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const char *text,
    size_t text_length,
    size_t row_number
);
static int validate_utf8_text(const char *text, size_t text_length, size_t *out_character_count);
static int utf8_prefix_byte_length_for_character_count(
    struct utf8_prefix_request request,
    size_t *out_prefix_length,
    size_t *out_character_count
);
static bool text_is_ascii_spaces(const char *text, size_t text_length);
static int utf8_sequence_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
);
static int utf8_two_byte_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
);
static int utf8_three_byte_e0_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
);
static int utf8_three_byte_plain_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
);
static int utf8_three_byte_surrogate_gap_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
);
static int utf8_four_byte_f0_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
);
static int utf8_four_byte_plain_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
);
static int utf8_four_byte_last_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
);
static bool utf8_byte_is_continuation(unsigned char byte);
static int varchar_length_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t *out_length
);
static int char_length_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t *out_length
);
static int parse_varchar_descriptor_length(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    size_t *out_length
);
static int parse_char_descriptor_length(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    size_t *out_length
);
static int make_empty_text_value(struct mylite_db *database, struct planned_value *out_value);
static int clip_integer_for_column(
    struct mylite_db *database,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    int64_t *out_value
);
static int parse_unsigned_integer_literal(
    const struct mylite_sql_source_span *span,
    uint64_t *out_value
);
static bool boolean_literal_magnitude(
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_value
);
static int convert_integer_for_column(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    int64_t *out_value
);
static int convert_integer_for_column_with_policy(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    int64_t *out_value
);
static int plan_select_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
);
static bool select_source_context_is_joined(const struct select_source_context *source_context);
static int plan_select_wildcard_columns(
    struct mylite_db *database,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
);
static int plan_select_qualified_wildcard_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *qualified_wildcard,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
);
static int collect_qualified_wildcard_parts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *qualified_wildcard,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t *out_part_count
);
static int set_unknown_qualified_wildcard_table_error(
    struct mylite_db *database,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count
);
static bool select_source_matches_qualified_wildcard_parts(
    const struct select_source_context *source_context,
    const char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count
);
static bool joined_source_matches_qualified_wildcard_parts(
    const struct planned_select_source *source,
    const char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count
);
static int append_visible_select_columns_from_source(
    struct mylite_db *database,
    struct planned_select *out_plan,
    size_t source_index,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
);
static int plan_joined_select_wildcard_columns(
    struct mylite_db *database,
    const struct select_source_context *source_context,
    struct planned_select *out_plan
);
static int plan_single_source_select_wildcard_columns(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
);
static int select_item_column_reference(
    const struct mylite_sql_ast_node *item,
    const struct mylite_sql_ast_node **out_column
);
static bool select_item_qualified_wildcard(
    const struct mylite_sql_ast_node *item,
    const struct mylite_sql_ast_node **out_qualified_wildcard
);
