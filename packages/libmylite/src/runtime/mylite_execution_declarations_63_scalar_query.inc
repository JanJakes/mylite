static bool select_statement_is_scalar_projection(const struct mylite_sql_ast_node *statement);
static bool select_statement_is_row_function_scalar_projection(
    const struct mylite_sql_ast_node *statement
);
static bool select_statement_has_no_source_or_dual(const struct mylite_sql_ast_node *statement);
static bool select_statement_has_only_scalar_projection_metadata_clauses(
    const struct mylite_sql_ast_node *statement
);
static int reject_bare_native_function_identifier_lookup_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static bool select_statement_is_scalar_projection_attempt(
    const struct mylite_sql_ast_node *statement
);
static int execute_scalar_projection_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int append_scalar_projection_columns_and_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_item,
    mylite_result *result,
    struct session_scalar_cell *cells,
    struct mylite_result_cell *values,
    size_t *out_column_count
);

struct values_statement_row_shape {
    size_t column_count;
    size_t row_number;
};

static int validate_values_statement_rows(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *rows,
    size_t *out_column_count
);
static int validate_values_statement_row(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row,
    struct values_statement_row_shape shape
);
static int validate_values_statement_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value
);
static int validate_values_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value
);
static int validate_values_order_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    size_t column_count
);
static int validate_values_order_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item,
    size_t column_count
);
static int validate_values_order_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    size_t column_count
);
static int validate_values_order_ordinal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    size_t column_count
);
static int validate_values_order_identifier(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    size_t column_count
);
static bool values_column_name_to_index(
    const char *column_name,
    size_t column_count,
    size_t *out_index
);
static int plan_values_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
);
static int append_values_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    size_t column_count
);
static int append_values_result_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *rows,
    const struct planned_select_limit *limit,
    size_t column_count
);
static int append_values_result_row(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *row,
    struct values_statement_row_shape shape
);
static int values_statement_value_cell(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value,
    struct session_scalar_cell *out_cell
);
static int values_statement_integer_cell(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value,
    struct session_scalar_cell *out_cell
);

struct scalar_binary_numeric_result_column_shape {
    enum mylite_result_logical_type logical_type;
    uint64_t display_length;
    uint16_t decimals;
    uint32_t extra_flags;
    bool nullable;
};

struct scalar_connection_string_result_column_shape {
    uint64_t display_length;
    uint32_t extra_flags;
    bool nullable;
};

static int append_scalar_projection_result_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *column_name,
    mylite_result *result
);
static int make_scalar_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
);
static int populate_scalar_literal_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_scalar_function_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static void populate_scalar_temporal_string_result_column_descriptor(
    const struct mylite_db *database,
    struct mylite_result_column_descriptor *descriptor
);
static void populate_scalar_date_result_column_descriptor(
    struct mylite_result_column_descriptor *descriptor
);
static void populate_scalar_datetime_result_column_descriptor(
    struct mylite_result_column_descriptor *descriptor
);
static int populate_scalar_string_literal_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct mylite_result_column_descriptor *descriptor
);
static int scalar_integer_literal_result_column_shape(
    const struct mylite_sql_ast_node *expression,
    struct scalar_binary_numeric_result_column_shape *out_shape
);
static int scalar_unsigned_integer_literal_result_column_shape(
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    struct scalar_binary_numeric_result_column_shape *out_shape
);
static int scalar_newdecimal_integer_literal_result_column_shape(
    const struct mylite_sql_source_span *span,
    struct scalar_binary_numeric_result_column_shape *out_shape
);
static int scalar_decimal_integer_significant_digit_count(
    const struct mylite_sql_source_span *span,
    size_t *out_significant_digit_count
);
static uint64_t scalar_connection_max_bytes_per_character(const struct mylite_db *database);
static void populate_scalar_binary_numeric_result_column_descriptor(
    struct mylite_result_column_descriptor *descriptor,
    struct scalar_binary_numeric_result_column_shape shape
);
static int populate_scalar_charset_collation_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static void populate_scalar_connection_string_result_column_descriptor(
    const struct mylite_db *database,
    struct mylite_result_column_descriptor *descriptor,
    struct scalar_connection_string_result_column_shape shape
);
static void populate_calendar_name_result_column_descriptor(
    const struct mylite_db *database,
    struct mylite_result_column_descriptor *descriptor
);
static void populate_uuid_string_result_column_descriptor(
    struct mylite_result_column_descriptor *descriptor,
    bool nullable
);
static void populate_scalar_json_result_column_descriptor(
    const struct mylite_db *database,
    struct mylite_result_column_descriptor *descriptor
);
static struct mylite_result_cell session_scalar_cell_result_cell(
    const struct session_scalar_cell *cell
);
static bool do_statement_has_only_scalar_projection_expressions(
    const struct mylite_sql_ast_node *statement
);
static int append_session_scalar_do_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int append_session_scalar_select_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list
);
static int append_session_scalar_expression_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static int append_select_modifier_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int append_found_rows_deprecation_warning(struct mylite_db *database);
static int append_sql_calc_found_rows_deprecation_warning(struct mylite_db *database);
static int append_sql_no_cache_deprecation_warning(struct mylite_db *database);
static int append_convert_using_charset_warning(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static int append_utf8_alias_warning(struct mylite_db *database);
static int append_utf8mb3_deprecation_warning(struct mylite_db *database);
static int append_session_scalar_cell_warnings(
    struct mylite_db *database,
    const struct session_scalar_cell *cell
);
static int append_session_scalar_cells_warnings(
    struct mylite_db *database,
    const struct session_scalar_cell *cells,
    size_t cell_count
);
static int accumulate_staged_division_by_zero_warnings(
    struct mylite_db *database,
    size_t cell_warning_count,
    size_t *total_warning_count
);
static int accumulate_staged_warning_count(
    struct mylite_db *database,
    size_t cell_warning_count,
    size_t *total_warning_count
);
static int append_division_by_zero_warnings(struct mylite_db *database, size_t warning_count);
static int append_invalid_logarithm_warnings(struct mylite_db *database, size_t warning_count);
static int append_truncated_incorrect_integer_warning(
    struct mylite_db *database,
    const char *value_text
);
static int append_truncated_incorrect_decimal_warning(
    struct mylite_db *database,
    const char *value_text
);
static int append_signed_complement_warnings(struct mylite_db *database, size_t warning_count);
static int append_unsigned_complement_warnings(struct mylite_db *database, size_t warning_count);
static int append_unhex_incorrect_string_warning(
    struct mylite_db *database,
    const char *value_text
);
static int copy_scalar_projection_column_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_text
);
static const char *select_statement_argument_count_error_function(
    const struct mylite_sql_ast_node *statement
);
static const char *do_statement_argument_count_error_function(
    const struct mylite_sql_ast_node *statement
);
static const char *argument_count_error_function_name(const struct mylite_sql_ast_node *expression);
static const char *argument_count_error_node_function_name(
    const struct mylite_sql_ast_node *expression
);
static int session_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int scalar_concat_operator_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int scalar_concat_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_concat_function_scalar_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cells,
    char **owned_texts,
    struct mylite_string_concat_argument *arguments,
    size_t argument_count,
    size_t *inout_next_argument
);
static int evaluate_concat_operator_scalar_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cells,
    char **owned_texts,
    struct mylite_string_concat_argument *arguments,
    size_t argument_count,
    size_t *inout_next_argument
);
static int evaluate_concat_operator_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int concat_operator_scalar_arithmetic_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int scalar_concat_operator_result(
    const struct mylite_string_concat_argument *arguments,
    size_t argument_count,
    char **out_text,
    size_t *out_text_length
);
static bool concat_operator_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression
);
static bool concat_operator_scalar_argument_is_nested_scalar_value(
    const struct mylite_sql_ast_node *expression
);
static bool concat_operator_scalar_argument_is_integer_arithmetic_expression(
    const struct mylite_sql_ast_node *expression
);
static bool concat_operator_scalar_arithmetic_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    bool *inout_saw_arithmetic,
    struct scalar_arithmetic_node_stack *stack
);
static int rand_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int rand_seed_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    uint32_t *out_seed
);
static int rand_scalar_seed_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    uint32_t *out_seed,
    struct session_scalar_cell *out_warnings
);
static int rand_scalar_seed_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool has_unary_sign,
    bool is_negative,
    uint32_t *out_seed,
    struct session_scalar_cell *out_warnings
);
static int rand_seed_numeric_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    uint32_t *out_seed
);
static int rand_seed_string_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    uint32_t *out_seed,
    struct session_scalar_cell *out_warnings
);
static int rand_seed_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    uint32_t *out_seed,
    struct session_scalar_cell *out_warnings
);
static int rand_seed_nullif_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int rand_seed_from_scalar_cell(
    struct mylite_db *database,
    struct session_scalar_cell *cell,
    uint32_t *out_seed,
    struct session_scalar_cell *out_warnings
);
static int rand_seed_from_integer_text(
    struct mylite_db *database,
    const char *text,
    uint32_t *out_seed
);
static int rand_seed_from_magnitude(uint64_t magnitude, bool is_negative, uint32_t *out_seed);
static int rand_seed_convert_string_value(
    struct mylite_db *database,
    const char *text,
    uint32_t *out_seed,
    struct session_scalar_cell *out_warnings
);
static void move_session_scalar_warning_state(
    struct session_scalar_cell *destination,
    struct session_scalar_cell *source
);
static int rand_seed_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    uint32_t *out_seed
);
static int current_timestamp_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
);
static int validate_supported_temporal_fractional_precision(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *subject_name,
    uint64_t *out_precision
);
static int sysdate_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int current_date_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
);
static int current_time_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
);
static int utc_timestamp_scalar_value(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
);
static int utc_date_scalar_value(struct mylite_db *database, struct session_scalar_cell *out_cell);
static int utc_time_scalar_value(struct mylite_db *database, struct session_scalar_cell *out_cell);
static const char *calendar_date_argument_count_error_function_name(
    enum mylite_sql_ast_node_kind ast_kind
);
static int scalar_subquery_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int scalar_subquery_select_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct session_scalar_cell *out_cell
);
static int scalar_subquery_inner_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int session_scalar_value_without_case(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int session_unary_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int session_binary_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int field_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int elt_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_elt_scalar_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_index,
    bool *out_is_null
);
static int evaluate_elt_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
);
static int evaluate_elt_scalar_string_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct field_scalar_argument *out_argument
);
static int evaluate_elt_scalar_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
);
static int format_elt_scalar_result(
    struct mylite_db *database,
    const struct field_scalar_argument *value,
    struct session_scalar_cell *out_cell
);
static int greatest_least_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int interval_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_interval_scalar_thresholds(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *thresholds,
    const struct field_scalar_argument *search,
    size_t threshold_count,
    int64_t *out_result
);
static int evaluate_interval_scalar_search_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
);
static int evaluate_interval_scalar_threshold_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
);
static int evaluate_interval_scalar_integer_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument,
    const char *unsupported_message
);
static int format_interval_scalar_result(
    struct mylite_db *database,
    int64_t value,
    struct session_scalar_cell *out_cell
);
static const char *greatest_least_function_name(const struct mylite_sql_ast_node *expression);
static int evaluate_greatest_least_scalar_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    struct field_scalar_argument *values,
    size_t argument_count,
    enum planned_row_scalar_field_domain *out_domain,
    size_t *out_numeric_scale
);
static int evaluate_greatest_least_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
);
static int evaluate_greatest_least_scalar_string_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct field_scalar_argument *out_argument
);
static int evaluate_greatest_least_scalar_integer_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
);
static int evaluate_greatest_least_scalar_decimal_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
);
static int merge_greatest_least_domain(
    struct mylite_db *database,
    enum planned_row_scalar_field_domain incoming,
    enum planned_row_scalar_field_domain *inout_domain
);
static size_t greatest_least_scalar_result_index(
    const struct field_scalar_argument *values,
    struct greatest_least_scalar_selection selection
);
static int compare_greatest_least_scalar_arguments(
    const struct field_scalar_argument *left,
    const struct field_scalar_argument *right,
    enum planned_row_scalar_field_domain domain
);
static int field_ascii_text_compare_case_insensitive(
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length
);
static int format_greatest_least_scalar_result(
    struct mylite_db *database,
    const struct field_scalar_argument *value,
    size_t numeric_scale,
    struct session_scalar_cell *out_cell
);
static int evaluate_field_scalar_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    struct field_scalar_argument *values,
    size_t argument_count,
    enum planned_row_scalar_field_domain *out_domain
);
static int evaluate_field_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
);
static int evaluate_field_scalar_string_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct field_scalar_argument *out_argument
);
static int evaluate_field_scalar_integer_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
);
static int merge_field_domain(
    struct mylite_db *database,
    enum planned_row_scalar_field_domain incoming,
    enum planned_row_scalar_field_domain *inout_domain
);
static bool field_ascii_text_is_supported(const char *text, size_t text_length);
static bool field_ascii_text_equals_case_insensitive(
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length
);
static size_t field_scalar_result_position(
    const struct field_scalar_argument_list *arguments,
    enum planned_row_scalar_field_domain domain
);
static bool field_scalar_arguments_match(
    const struct field_scalar_argument *left,
    const struct field_scalar_argument *right,
    enum planned_row_scalar_field_domain domain
);
static int format_field_scalar_result_position(
    struct mylite_db *database,
    size_t result_position,
    struct session_scalar_cell *out_cell
);
static void field_scalar_argument_deinit(struct field_scalar_argument *argument);
static int cast_binary_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int cast_char_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int cast_signed_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int cast_unsigned_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int convert_binary_type_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int convert_char_type_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int convert_signed_type_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int convert_unsigned_type_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int convert_using_binary_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int convert_using_charset_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int collate_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int scalar_convert_charset_info_for_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_convert_charset_info *out_info
);
static int scalar_convert_charset_info_by_name(
    struct mylite_db *database,
    const char *charset_name,
    struct scalar_convert_charset_info *out_info
);
static int apply_scalar_placeholder_cast(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum scalar_placeholder_cast_target default_target,
    bool convert_syntax,
    struct session_scalar_cell *inout_cell
);
static int parse_scalar_placeholder_cast_info(
    const struct mylite_sql_ast_node *expression,
    enum scalar_placeholder_cast_target default_target,
    bool convert_syntax,
    struct scalar_placeholder_cast_info *out_info
);
static int scalar_placeholder_target_span(
    const struct mylite_sql_ast_node *expression,
    bool convert_syntax,
    struct mylite_sql_source_span *out_span
);
static int apply_scalar_placeholder_text_length(
    struct mylite_db *database,
    const struct scalar_placeholder_cast_info *info,
    struct session_scalar_cell *inout_cell
);
static int apply_scalar_placeholder_date(
    struct mylite_db *database,
    struct session_scalar_cell *inout_cell
);
static int apply_scalar_placeholder_time(
    struct mylite_db *database,
    struct session_scalar_cell *inout_cell
);
static int apply_scalar_placeholder_datetime(
    struct mylite_db *database,
    struct session_scalar_cell *inout_cell
);
static int apply_scalar_placeholder_decimal(
    struct mylite_db *database,
    const struct scalar_placeholder_cast_info *info,
    struct session_scalar_cell *inout_cell
);
static int apply_scalar_placeholder_json(
    struct mylite_db *database,
    struct session_scalar_cell *inout_cell
);
static int assign_scalar_cell_text(
    struct mylite_db *database,
    struct session_scalar_cell *inout_cell,
    const char *text,
    size_t text_length
);
static int assign_scalar_cell_owned_text(
    struct mylite_db *database,
    struct session_scalar_cell *inout_cell,
    char *text,
    size_t text_length
);
static size_t scalar_cell_text_length(const struct session_scalar_cell *cell);
static void trim_scalar_span(struct mylite_sql_source_span *span);
static bool scalar_span_consume_keyword(struct mylite_sql_source_span *span, const char *keyword);
static bool scalar_span_parse_optional_length(
    struct mylite_sql_source_span *span,
    size_t *out_length
);
static bool scalar_span_parse_decimal_scale(struct mylite_sql_source_span *span, size_t *out_scale);
static bool scalar_span_parse_unsigned(
    const struct mylite_sql_source_span *span,
    size_t *inout_index,
    size_t *out_value
);
static bool scalar_span_keyword_at(
    const char *text,
    size_t text_length,
    size_t index,
    const char *keyword,
    size_t *out_end
);
static bool scalar_span_is_identifier_byte(unsigned char byte);
static int scalar_text_conversion_input_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct scalar_text_conversion_messages *messages,
    struct session_scalar_cell *out_cell
);
static int scalar_integer_cast_input_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum scalar_integer_cast_target target,
    const struct scalar_integer_cast_messages *messages,
    struct session_scalar_cell *out_cell
);
static int scalar_integer_cast_string_value(
    struct mylite_db *database,
    const char *text,
    enum scalar_integer_cast_target target,
    struct session_scalar_cell *out_cell
);
static int scalar_integer_cast_decimal_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    enum scalar_integer_cast_target target,
    struct session_scalar_cell *out_cell
);
static void parse_scalar_integer_cast_string(
    const char *text,
    struct scalar_integer_cast_parse *out_parse
);
static void parse_scalar_integer_cast_digits(
    struct scalar_integer_cast_digit_source source,
    struct scalar_integer_cast_parse *inout_parse,
    size_t *out_end_offset
);
static void parse_scalar_integer_cast_decimal_literal(
    const struct mylite_sql_source_span *span,
    uint64_t limit,
    struct scalar_integer_cast_parse *out_parse
);
static int stage_truncated_integer_warning(
    const char *input_text,
    struct session_scalar_cell *cell
);
static int stage_cast_truncated_decimal_warning(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    struct session_scalar_cell *cell
);
static int format_signed_cast_value(
    struct mylite_db *database,
    bool is_negative,
    uint64_t magnitude,
    struct session_scalar_cell *out_cell
);
static int format_negative_signed_cast_value(
    struct mylite_db *database,
    uint64_t magnitude,
    struct session_scalar_cell *out_cell
);
static int format_twos_complement_signed_cast_value(
    struct mylite_db *database,
    uint64_t magnitude,
    struct session_scalar_cell *out_cell
);
static int format_signed_cast_text(
    struct mylite_db *database,
    uint64_t magnitude,
    struct session_scalar_cell *out_cell
);
static int format_unsigned_cast_value(
    struct mylite_db *database,
    bool is_negative,
    uint64_t magnitude,
    struct session_scalar_cell *out_cell
);
static int format_unsigned_magnitude_value(
    struct mylite_db *database,
    uint64_t value,
    struct session_scalar_cell *out_cell
);
static bool scalar_integer_cast_is_ascii_space(unsigned char byte);
static int evaluate_bit_count_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
);
static int evaluate_bit_count_direct_literal_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value,
    bool *out_handled
);
static int evaluate_scalar_bitwise_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
);
static int evaluate_scalar_bitwise_frame(
    struct mylite_db *database,
    struct scalar_bitwise_eval_stack *expression_stack,
    struct scalar_bitwise_value_stack *value_stack,
    const struct scalar_bitwise_eval_frame *frame
);
static int evaluate_scalar_bitwise_apply_frame(
    struct mylite_db *database,
    struct scalar_bitwise_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_bitwise_apply_unary_frame(
    struct mylite_db *database,
    struct scalar_bitwise_value_stack *value_stack
);
static int evaluate_scalar_bitwise_short_circuit_or_enter_right_frame(
    struct mylite_db *database,
    struct scalar_bitwise_eval_stack *expression_stack,
    const struct scalar_bitwise_value_stack *value_stack,
    const struct scalar_bitwise_eval_frame *frame
);
static int evaluate_scalar_bitwise_enter_frame(
    struct mylite_db *database,
    struct scalar_bitwise_eval_stack *expression_stack,
    struct scalar_bitwise_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int evaluate_scalar_bitwise_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
);
static int apply_scalar_bitwise_operator(
    const struct scalar_bitwise_value *left,
    const struct scalar_bitwise_value *right,
    enum mylite_sql_ast_operator operator_kind,
    struct scalar_bitwise_value *out_result
);
static int scalar_bitwise_eval_stack_push(
    struct mylite_db *database,
    struct scalar_bitwise_eval_stack *stack,
    enum scalar_bitwise_eval_frame_kind kind,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
);
static void scalar_bitwise_eval_stack_deinit(struct scalar_bitwise_eval_stack *stack);
static int scalar_bitwise_value_stack_push(
    struct mylite_db *database,
    struct scalar_bitwise_value_stack *stack,
    struct scalar_bitwise_value value
);
static bool scalar_bitwise_value_stack_pop(
    struct scalar_bitwise_value_stack *stack,
    struct scalar_bitwise_value *out_value
);
static void scalar_bitwise_value_stack_deinit(struct scalar_bitwise_value_stack *stack);
static int scalar_logical_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_scalar_logical_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
);
static int evaluate_scalar_logical_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_logical_eval_frame *frame
);
static int evaluate_scalar_logical_apply_not_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack
);
static int evaluate_scalar_logical_apply_comparison_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_logical_apply_is_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_logical_comparison_short_circuit_or_enter_right_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    const struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_logical_eval_frame *frame
);
static int evaluate_scalar_logical_apply_logical_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static void evaluate_scalar_logical_and_result(
    const struct scalar_arithmetic_value *left,
    const struct scalar_arithmetic_value *right,
    struct scalar_arithmetic_value *result
);
static void evaluate_scalar_logical_or_result(
    const struct scalar_arithmetic_value *left,
    const struct scalar_arithmetic_value *right,
    struct scalar_arithmetic_value *result
);
static void evaluate_scalar_logical_xor_result(
    const struct scalar_arithmetic_value *left,
    const struct scalar_arithmetic_value *right,
    struct scalar_arithmetic_value *result
);
static bool scalar_arithmetic_truth_value(const struct scalar_arithmetic_value *value);
static int evaluate_scalar_logical_short_circuit_or_enter_right_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_logical_eval_frame *frame
);
static int evaluate_scalar_logical_enter_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int evaluate_scalar_logical_enter_unary_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int evaluate_scalar_logical_enter_logical_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_logical_enter_comparison_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_logical_enter_is_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_logical_enter_null_safe_comparison_frame(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *expression_stack,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_logical_enter_arithmetic_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int scalar_logical_eval_stack_push(
    struct mylite_db *database,
    struct scalar_logical_eval_stack *stack,
    enum scalar_logical_eval_frame_kind kind,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
);
static void scalar_logical_eval_stack_deinit(struct scalar_logical_eval_stack *stack);
static int scalar_comparison_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int row_constructor_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_scalar_comparison_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
);
static int evaluate_scalar_comparison_frame(
    struct mylite_db *database,
    struct scalar_comparison_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_comparison_eval_frame *frame
);
static int evaluate_scalar_comparison_apply_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_comparison_apply_not_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack
);
static int evaluate_scalar_comparison_short_circuit_or_enter_right_frame(
    struct mylite_db *database,
    struct scalar_comparison_eval_stack *expression_stack,
    const struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_comparison_eval_frame *frame
);
static int evaluate_scalar_comparison_enter_frame(
    struct mylite_db *database,
    struct scalar_comparison_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static bool scalar_comparison_result_is_true(const struct scalar_comparison_operation *operation);
static int scalar_comparison_eval_stack_push(
    struct mylite_db *database,
    struct scalar_comparison_eval_stack *stack,
    enum scalar_comparison_eval_frame_kind kind,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
);
static void scalar_comparison_eval_stack_deinit(struct scalar_comparison_eval_stack *stack);
static int scalar_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_scalar_arithmetic_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
);
static int evaluate_scalar_arithmetic_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_arithmetic_eval_frame *frame
);
static int evaluate_scalar_arithmetic_apply_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_arithmetic_apply_unary_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_scalar_arithmetic_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int evaluate_scalar_arithmetic_operand_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int evaluate_scalar_arithmetic_non_arithmetic_binary_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int finish_scalar_arithmetic_result(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    struct scalar_arithmetic_value *out_value
);
static int evaluate_scalar_arithmetic_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
);
static int scalar_arithmetic_div_left_operand_short_circuits(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_short_circuits
);
static int scalar_arithmetic_div_short_circuit_visit_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack,
    bool *out_short_circuits
);
static int scalar_arithmetic_div_short_circuit_push_child(
    struct mylite_db *database,
    struct scalar_arithmetic_node_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int scalar_arithmetic_div_short_circuit_push_function_arguments(
    struct mylite_db *database,
    struct scalar_arithmetic_node_stack *stack,
    const struct mylite_sql_ast_node *arguments,
    bool *out_short_circuits
);
static int parse_scalar_arithmetic_operand(
    struct mylite_db *database,
    const struct session_scalar_cell *cell,
    struct scalar_arithmetic_value *out_value
);
static int apply_scalar_arithmetic_operator(
    struct mylite_db *database,
    const struct scalar_arithmetic_operation *operation,
    int64_t *out_result
);
static int scalar_arithmetic_eval_stack_push(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *stack,
    enum scalar_arithmetic_eval_frame_kind kind,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_operator operator_kind
);
static void scalar_arithmetic_eval_stack_deinit(struct scalar_arithmetic_eval_stack *stack);
static int scalar_arithmetic_value_stack_push(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *stack,
    struct scalar_arithmetic_value value
);
static bool scalar_arithmetic_value_stack_pop(
    struct scalar_arithmetic_value_stack *stack,
    struct scalar_arithmetic_value *out_value
);
static void scalar_arithmetic_value_stack_deinit(struct scalar_arithmetic_value_stack *stack);
static bool scalar_arithmetic_node_stack_push(
    struct scalar_arithmetic_node_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static void scalar_arithmetic_node_stack_deinit(struct scalar_arithmetic_node_stack *stack);
static bool checked_int64_add(int64_t left, int64_t right, int64_t *out_result);
static bool checked_int64_subtract(int64_t left, int64_t right, int64_t *out_result);
static bool checked_int64_multiply(int64_t left, int64_t right, int64_t *out_result);
static bool checked_int64_modulo(int64_t left, int64_t right, int64_t *out_result);
static bool checked_int64_divide(int64_t left, int64_t right, int64_t *out_result);
static bool checked_int64_negate(int64_t value, int64_t *out_result);
static void set_scalar_arithmetic_unsupported_error(struct mylite_db *database);
static void set_scalar_division_unsupported_error(struct mylite_db *database);
static void set_scalar_arithmetic_operand_out_of_range_error(struct mylite_db *database);
static void set_scalar_arithmetic_overflow_error(struct mylite_db *database);
static void set_scalar_bitwise_unsupported_error(struct mylite_db *database);
static void set_abs_signed_minimum_overflow_error(struct mylite_db *database);
static void set_abs_unsupported_error(struct mylite_db *database);
static void set_sign_unsupported_error(struct mylite_db *database);
static void set_rounding_unsupported_error(struct mylite_db *database);
static int set_rounding_signed_overflow_error(struct mylite_db *database);
static void set_sqrt_unsupported_error(struct mylite_db *database);
static void set_angle_conversion_unsupported_error(struct mylite_db *database);
static void set_inverse_trig_unsupported_error(struct mylite_db *database);
static void set_direct_trig_unsupported_error(struct mylite_db *database);
static void set_atan_unsupported_error(struct mylite_db *database);
static void set_exp_log_power_unsupported_error(struct mylite_db *database);
static void set_base_conversion_unsupported_error(struct mylite_db *database);
static void set_bit_count_unsupported_error(struct mylite_db *database);
static void set_crc32_unsupported_error(struct mylite_db *database);
static void set_hex_unsupported_error(struct mylite_db *database);
static void set_format_unsupported_error(struct mylite_db *database);
static void set_truncate_unsupported_error(struct mylite_db *database);
static void set_scalar_logical_unsupported_error(struct mylite_db *database);
static void set_scalar_comparison_unsupported_error(struct mylite_db *database);
static int literal_projection_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int if_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int ifnull_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int coalesce_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int nullif_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int isnull_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int case_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int searched_case_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int simple_case_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_case_arithmetic_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
);
static int copy_case_result_cell(
    struct mylite_db *database,
    const struct session_scalar_cell *selected_cell,
    size_t previous_warning_count,
    struct session_scalar_cell *out_cell
);
static bool case_arithmetic_values_are_equal(
    const struct scalar_arithmetic_value *left,
    const struct scalar_arithmetic_value *right
);
static int if_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int if_eval_current_expression(
    struct mylite_db *database,
    struct if_eval_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    const struct mylite_sql_ast_node **next_expression,
    struct session_scalar_cell *out_cell
);
static int if_eval_isnull_expression(
    struct mylite_db *database,
    struct if_eval_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    const struct mylite_sql_ast_node **next_expression
);
static bool if_eval_completed_value(
    struct if_eval_stack *stack,
    struct session_scalar_cell *cell,
    const struct mylite_sql_ast_node **next_expression,
    struct session_scalar_cell *out_cell
);
static void if_eval_complete_if_frame(
    const struct if_eval_frame *frame,
    struct session_scalar_cell *cell,
    const struct mylite_sql_ast_node **next_expression
);
static void if_eval_complete_ifnull_frame(
    const struct if_eval_frame *frame,
    struct session_scalar_cell *cell,
    const struct mylite_sql_ast_node **next_expression
);
static void if_eval_complete_coalesce_frame(
    struct if_eval_frame *frame,
    struct session_scalar_cell *cell,
    const struct mylite_sql_ast_node **next_expression
);
static void if_eval_complete_nullif_frame(
    struct if_eval_frame *frame,
    struct session_scalar_cell *cell,
    const struct mylite_sql_ast_node **next_expression
);
static void if_eval_complete_isnull_frame(
    struct session_scalar_cell *cell,
    const struct mylite_sql_ast_node **next_expression
);
static int if_non_function_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    struct session_scalar_cell *out_cell
);
static int if_integer_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    const char *function_name,
    struct session_scalar_cell *out_cell
);
static int if_eval_stack_push(
    struct mylite_db *database,
    struct if_eval_stack *stack,
    enum if_eval_frame_kind kind,
    const struct mylite_sql_ast_node *first_value,
    const struct mylite_sql_ast_node *second_value
);
static void if_eval_stack_deinit(struct if_eval_stack *stack);
static void copy_session_scalar_cell(
    struct session_scalar_cell *destination,
    const struct session_scalar_cell *source
);
static void session_scalar_cell_deinit(struct session_scalar_cell *cell);
static void session_scalar_cell_array_deinit(struct session_scalar_cell *cells, size_t cell_count);
static bool if_scalar_condition_is_true(const struct session_scalar_cell *cell);
static int normalize_decimal_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    bool is_negative,
    char *buffer,
    size_t buffer_size
);
static int last_insert_id_set_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int last_insert_id_set_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    bool *out_is_null,
    uint64_t *out_value
);
static int signed_last_insert_id_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    uint64_t *out_value
);
static void set_last_insert_id_argument_unsupported_error(struct mylite_db *database);
static int system_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static uint64_t information_schema_stats_expiry_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
);
static int database_character_set_system_variable_value(
    struct mylite_db *database,
    bool global_scope,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
static int database_collation_system_variable_value(
    struct mylite_db *database,
    bool global_scope,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
static int format_session_scalar_uint64_value(
    struct mylite_db *database,
    uint64_t value,
    struct session_scalar_cell *out_cell
);
static uint64_t auto_increment_step_system_variable_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    bool global_scope
);
static uint64_t sql_select_limit_system_variable_value(
    const struct mylite_db *database,
    bool global_scope
);
static uint64_t timeout_system_variable_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    bool global_scope
);
static const char *default_sql_mode_value(void);
static const struct mylite_diagnostics *system_variable_count_diagnostics(
    const struct mylite_db *database
);
static int diagnostics_count_system_variable_value(
    const struct mylite_diagnostics *diagnostics,
    enum mylite_execution_system_variable_kind variable,
    uint64_t *out_count
);
static int resolve_session_system_variable(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_execution_system_variable_kind *out_kind
);
static bool resolve_system_variable_kind(
    const struct system_variable_component *name,
    enum mylite_execution_system_variable_kind *out_kind
);
static int show_system_variable_value(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    bool global_scope,
    char *integer_buffer,
    size_t integer_buffer_size,
    const char **out_value
);
static int format_show_system_variable_uint64_value(
    struct mylite_db *database,
    uint64_t value,
    char *integer_buffer,
    size_t integer_buffer_size,
    const char **out_value
);
static int format_timestamp_system_variable_value(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
);
static bool show_variables_scope_is_global(const struct mylite_sql_ast_node *scope);
static bool show_status_descriptor_visible(
    const struct mylite_execution_show_status_descriptor *descriptor,
    bool global_scope
);
static int append_show_status(
    struct mylite_db *database,
    mylite_result *result,
    const struct show_like_filter *filter,
    const struct mylite_sql_ast_node *where_clause,
    bool global_scope,
    const struct mylite_execution_show_status_descriptor *descriptor
);
static int show_catalog_filter_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct show_like_filter *filter,
    const struct show_catalog_where_row *row,
    bool *out_matches
);
static int show_catalog_where_clause_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct show_catalog_where_row *row,
    bool *out_matches
);
static int evaluate_show_catalog_where_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_catalog_where_row *row,
    enum show_variables_where_truth *out_truth
);
static int visit_show_catalog_where_predicate(
    struct mylite_db *database,
    struct show_variables_where_frame_stack *frame_stack,
    const struct mylite_sql_ast_node *predicate
);
static int evaluate_show_catalog_where_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_catalog_where_row *row,
    struct show_variables_where_truth_stack *truth_stack
);
static int evaluate_show_catalog_where_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_catalog_where_row *row,
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_catalog_where_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_catalog_where_row *row,
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_catalog_where_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_catalog_where_row *row,
    enum show_variables_where_truth *out_truth
);
static int show_catalog_where_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct show_catalog_where_row *row,
    const char **out_value
);
static int compare_show_catalog_where_literal(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const char *left,
    const struct mylite_sql_ast_node *right,
    enum show_variables_where_truth *out_truth
);
static enum show_variables_where_truth show_catalog_null_literal_truth(
    enum mylite_sql_ast_operator operator_kind,
    const char *left
);
static enum show_variables_where_truth show_catalog_null_left_truth(
    enum mylite_sql_ast_operator operator_kind
);
static int show_catalog_where_truth_from_comparison(
    struct mylite_db *database,
    struct show_catalog_where_comparison comparison,
    enum show_variables_where_truth *out_truth
);
static int decode_show_catalog_where_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_text
);
static int append_show_variable(
    struct mylite_db *database,
    mylite_result *result,
    const struct show_like_filter *filter,
    const struct mylite_sql_ast_node *where_clause,
    bool global_scope,
    const struct mylite_execution_system_variable_descriptor *descriptor
);
static bool show_variable_descriptor_visible(
    const struct mylite_execution_system_variable_descriptor *descriptor,
    bool global_scope
);
static int show_variables_where_clause_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct show_variables_where_row *row,
    bool *out_matches
);
static int evaluate_show_variables_where_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    enum show_variables_where_truth *out_truth
);
static int visit_show_variables_where_predicate(
    struct mylite_db *database,
    struct show_variables_where_frame_stack *frame_stack,
    const struct mylite_sql_ast_node *predicate
);
static int evaluate_show_variables_where_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    struct show_variables_where_truth_stack *truth_stack
);
static int evaluate_show_variables_where_logical_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    struct show_variables_where_truth_stack *truth_stack,
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_variables_where_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_variables_where_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    enum show_variables_where_truth *out_truth
);
static int evaluate_show_variables_where_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    enum show_variables_where_truth *out_truth
);
static enum show_variables_where_truth negate_show_variables_where_truth(
    enum show_variables_where_truth truth
);
static enum show_variables_where_truth and_show_variables_where_truth(
    enum show_variables_where_truth left,
    enum show_variables_where_truth right
);
static enum show_variables_where_truth or_show_variables_where_truth(
    enum show_variables_where_truth left,
    enum show_variables_where_truth right
);
static int show_variables_where_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct show_variables_where_row *row,
    const char **out_value
);
static int resolve_show_variables_where_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    enum show_variables_where_column *out_column
);
static int show_variables_column_reference_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char **out_text
);
static int compare_show_variables_where_literal(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const char *left,
    const struct mylite_sql_ast_node *right,
    enum show_variables_where_truth *out_truth
);
static int decode_show_variables_where_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_text
);
static void show_variables_where_frame_stack_deinit(struct show_variables_where_frame_stack *stack);
static int show_variables_where_frame_stack_push(
    struct mylite_db *database,
    struct show_variables_where_frame_stack *stack,
    struct show_variables_where_eval_frame frame
);
static bool show_variables_where_frame_stack_pop(
    struct show_variables_where_frame_stack *stack,
    struct show_variables_where_eval_frame *out_frame
);
static void show_variables_where_truth_stack_deinit(struct show_variables_where_truth_stack *stack);
static int show_variables_where_truth_stack_push(
    struct mylite_db *database,
    struct show_variables_where_truth_stack *stack,
    enum show_variables_where_truth truth
);
static bool show_variables_where_truth_stack_pop(
    struct show_variables_where_truth_stack *stack,
    enum show_variables_where_truth *out_truth
);
