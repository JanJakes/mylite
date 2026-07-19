static int plan_select_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *out_predicate
);
static int plan_joined_select_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *source_context,
    struct planned_select_predicate *out_predicate
);
static int plan_select_predicate_with_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static int plan_select_having_predicate_with_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *having_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static int merge_select_predicate_with_and(
    struct mylite_db *database,
    struct planned_select_predicate *target,
    struct planned_select_predicate *addition
);
static void planned_select_predicate_deinit(struct planned_select_predicate *predicate);
static void planned_select_predicate_deinit_without_exists(
    struct planned_select_predicate *predicate
);
static void planned_select_predicate_owned_columns_deinit(
    struct planned_select_predicate *predicate
);
static void planned_exists_subquery_deinit(struct planned_exists_subquery *subquery);
static void planned_in_subquery_deinit(struct planned_in_subquery *subquery);
static void planned_scalar_aggregate_subquery_deinit(
    struct planned_scalar_aggregate_subquery *subquery
);
static int plan_select_predicate_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static int plan_select_predicate_node_without_exists(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static int plan_select_predicate_work_item(
    struct mylite_db *database,
    struct predicate_work_item item,
    struct predicate_work_stack *work_stack,
    struct predicate_result_stack *result_stack,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static int plan_select_predicate_work_item_without_exists(
    struct mylite_db *database,
    struct predicate_work_item item,
    struct predicate_work_stack *work_stack,
    struct predicate_result_stack *result_stack,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static int finish_planned_select_logical_predicate(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    struct predicate_result_stack *result_stack,
    struct planned_select_predicate *out_predicate
);
static int finish_planned_select_not_predicate(
    struct mylite_db *database,
    struct predicate_result_stack *result_stack,
    struct planned_select_predicate *out_predicate
);
static int plan_select_predicate_ast_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct predicate_work_stack *work_stack,
    struct predicate_result_stack *result_stack,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static int plan_select_predicate_ast_node_without_exists(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct predicate_work_stack *work_stack,
    struct predicate_result_stack *result_stack,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static const struct mylite_sql_ast_node *unwrap_parenthesized_predicate(
    const struct mylite_sql_ast_node *node
);
static bool is_logical_predicate_node(const struct mylite_sql_ast_node *node);
static int append_select_predicate_logical_work(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct predicate_work_stack *work_stack
);
static int append_select_predicate_not_work(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct predicate_work_stack *work_stack
);
static int append_select_predicate_deprecated_warning_work(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    struct predicate_work_stack *work_stack
);
static int plan_select_predicate_leaf_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct predicate_result_stack *result_stack,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static int plan_select_predicate_leaf_node_without_exists(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct predicate_result_stack *result_stack,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
);
static bool planned_predicate_kind_for_operator(
    enum mylite_sql_ast_operator operator_kind,
    enum planned_select_predicate_kind *out_kind
);
static int plan_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_row_scalar_function_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
);
static bool predicate_node_is_sounds_like_expression(const struct mylite_sql_ast_node *node);
static int plan_sounds_like_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_row_scalar_expression_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_row_scalar_expression_between_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_scalar_literal_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_scalar_literal_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_literal_left_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_same_scope_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static bool same_scope_column_comparison_is_supported(
    const struct planned_select_predicate_node *node
);
static int plan_scalar_literal_is_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_constant_truth_predicate(
    struct mylite_db *database,
    bool truth,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_where_scalar_literal_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_select_predicate_node *node
);
static int plan_where_scalar_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static int plan_where_row_scalar_predicate_literal_value(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *row_scalar_expression,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static bool row_scalar_predicate_literal_prefers_text(
    const struct planned_row_scalar_expression *expression
);
static bool row_scalar_predicate_literal_prefers_blob(
    const struct planned_row_scalar_expression *expression
);
static bool row_scalar_expression_needs_real_comparison_context(
    const struct planned_row_scalar_expression *expression
);
static int plan_where_scalar_hex_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct planned_value *out_value
);
static int plan_where_scalar_exact_integer_string_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct planned_value *out_value
);
static int plan_where_scalar_decimal_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static bool where_decimal_string_literal_is_supported(const char *text, size_t text_length);
static bool where_decimal_span_is_supported(const struct mylite_sql_source_span *span);
static bool predicate_row_scalar_cast_expression_decimal_scale(
    const struct mylite_sql_ast_node *expression,
    size_t *out_scale
);
static bool predicate_row_scalar_cast_expression_is_binary(
    const struct mylite_sql_ast_node *expression
);
static bool predicate_node_is_scalar_literal_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static bool predicate_node_is_literal_left_predicate_value(
    const struct mylite_sql_ast_node *predicate_node
);
static bool predicate_node_is_string_literal_predicate_value(
    const struct mylite_sql_ast_node *predicate_node
);
static int resolve_literal_left_predicate_context_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *candidate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    bool *out_found
);
static int plan_literal_left_predicate_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct mylite_catalog_column_descriptor *context_column,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int convert_literal_left_predicate_value_for_context(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *context_column,
    struct planned_value *out_value
);
static bool predicate_node_is_supported_row_scalar_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static bool predicate_node_is_supported_row_scalar_predicate_value_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static bool predicate_node_is_column_reference(const struct mylite_sql_ast_node *node);
static bool predicate_comparison_value_is_null(const struct mylite_sql_ast_node *value_node);
static enum mylite_sql_ast_operator comparison_operator_flipped(
    enum mylite_sql_ast_operator operator_kind
);
static bool scalar_literal_is_result(
    const struct planned_value *value,
    enum mylite_sql_ast_operator operator_kind
);
static bool predicate_node_is_find_in_set_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static bool predicate_node_is_json_valid_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static bool predicate_node_is_json_contains_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static bool predicate_node_is_regexp_like_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static bool predicate_node_is_string_length_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static bool predicate_node_is_substring_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static int plan_row_scalar_expression_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static bool predicate_node_is_date_format_numeric_predicate_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static int plan_date_format_numeric_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static bool predicate_node_is_temporal_extract_expression(
    const struct mylite_sql_ast_node *predicate_node
);
static int plan_temporal_extract_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_temporal_extract_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_temporal_extract_between_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_temporal_extract_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_temporal_extract_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_temporal_extract_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static bool temporal_extract_predicate_kind_is_numeric(enum mylite_temporal_extract_kind kind);
static int plan_string_length_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_string_length_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_string_length_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_string_length_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_string_length_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static int plan_substring_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_substring_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_row_scalar_expression_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_substring_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_substring_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static int plan_find_in_set_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_find_in_set_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_find_in_set_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_find_in_set_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_find_in_set_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static int plan_json_valid_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_json_valid_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_json_valid_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_json_valid_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_json_valid_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static int plan_json_contains_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_json_contains_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_json_contains_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_json_contains_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_json_contains_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static int plan_regexp_like_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_regexp_like_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_regexp_like_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_regexp_like_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_regexp_like_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
);
static bool comparison_predicate_rhs_is_column_reference(
    const struct mylite_sql_ast_node *predicate_node
);

struct row_constructor_element_nodes {
    const struct mylite_sql_ast_node *column_node;
    const struct mylite_sql_ast_node *value_node;
};

static int plan_row_constructor_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
);
static int plan_row_constructor_equality_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *left_node,
    const struct mylite_sql_ast_node *right_node,
    size_t element_count,
    const struct select_source_context *source_context,
    enum mylite_sql_ast_operator element_operator,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_root_index
);
static int plan_row_constructor_order_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *left_node,
    const struct mylite_sql_ast_node *right_node,
    size_t element_count,
    const struct select_source_context *source_context,
    enum mylite_sql_ast_operator operator_kind,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_root_index
);
static int plan_row_constructor_strict_order_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *left_node,
    const struct mylite_sql_ast_node *right_node,
    size_t element_count,
    const struct select_source_context *source_context,
    enum mylite_sql_ast_operator operator_kind,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_root_index
);
static int plan_row_constructor_element_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *left_node,
    const struct mylite_sql_ast_node *right_node,
    enum mylite_sql_ast_operator operator_kind,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_row_constructor_column_value_element_predicate(
    struct mylite_db *database,
    struct row_constructor_element_nodes nodes,
    enum mylite_sql_ast_operator operator_kind,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_row_constructor_element_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_select_predicate_node *node
);
static int append_row_constructor_and_node(
    struct mylite_db *database,
    struct planned_select_predicate *predicate,
    size_t left_index,
    size_t right_index,
    size_t *out_node_index
);
static int append_row_constructor_or_node(
    struct mylite_db *database,
    struct planned_select_predicate *predicate,
    size_t left_index,
    size_t right_index,
    size_t *out_node_index
);
static int append_row_constructor_not_node(
    struct mylite_db *database,
    struct planned_select_predicate *predicate,
    size_t child_index,
    size_t *out_node_index
);
static bool predicate_node_is_row_constructor(const struct mylite_sql_ast_node *node);
static bool row_constructor_predicate_operator_is_supported(
    enum mylite_sql_ast_operator operator_kind
);
static bool row_constructor_predicate_operator_is_order(enum mylite_sql_ast_operator operator_kind);
static enum mylite_sql_ast_operator row_constructor_strict_order_operator(
    enum mylite_sql_ast_operator operator_kind
);
static int plan_scalar_literal_or_literal_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
);
static int plan_rhs_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
);
static int plan_comparison_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_scalar_count_subquery_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
);
static int plan_scalar_aggregate_subquery_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
);
static int plan_scalar_aggregate_subquery_between_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
);
static int plan_scalar_aggregate_subquery_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *scalar_subquery,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_scalar_aggregate_subquery *out_subquery,
    size_t outer_source_count
);
static bool scalar_subquery_is_column_aggregate(const struct mylite_sql_ast_node *scalar_subquery);
static int scalar_aggregate_subquery_select_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct mylite_sql_ast_node **out_expression,
    enum planned_column_aggregate_function *out_function
);
static int plan_scalar_aggregate_subquery_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum planned_column_aggregate_function function,
    const struct planned_exists_subquery *subquery,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
);
static int scalar_aggregate_subquery_source_context(
    const struct planned_exists_subquery *subquery,
    struct select_source_context *out_source_context,
    const struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count
);
static int allocate_scalar_aggregate_subquery(
    struct mylite_db *database,
    struct planned_scalar_aggregate_subquery **out_subquery
);
static int plan_scalar_count_subquery(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *scalar_subquery,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    size_t outer_source_count,
    struct planned_exists_subquery *out_subquery
);
static int validate_scalar_count_subquery_select_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list
);
static bool scalar_count_subquery_select_expression_is_supported(
    const struct mylite_sql_ast_node *expression
);
static int plan_comparison_predicate_like_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int like_escape_character_from_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    char *out_escape_character
);
static bool comparison_predicate_rhs_requires_row_scalar_value(
    const struct mylite_sql_ast_node *predicate_node,
    const struct planned_select_predicate_node *node
);
static bool predicate_value_is_integer_literal(const struct mylite_sql_ast_node *value_node);
static void planned_select_predicate_node_deinit_runtime_values(
    struct planned_select_predicate_node *node
);
static int validate_comparison_predicate_column(
    struct mylite_db *database,
    const struct planned_select_predicate_node *node
);
static int plan_predicate_row_scalar_value_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_predicate_row_scalar_upper_value_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_exists_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_exists_subquery(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct exists_subquery_outer_context *outer_context,
    struct planned_exists_subquery *out_subquery
);
static int plan_exists_table_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_exists_subquery *out_subquery
);
static int plan_exists_join_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_exists_subquery *out_subquery
);
static int validate_exists_join_source(
    struct mylite_db *database,
    const struct planned_exists_subquery *subquery
);
static int validate_exists_select_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct planned_exists_subquery *subquery
);
static bool exists_select_item_is_supported_literal(const struct mylite_sql_ast_node *expression);
static int plan_exists_inner_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_exists_subquery *subquery
);
static void offset_exists_inner_predicate_source_indexes(
    struct planned_select_predicate *predicate,
    size_t source_index_offset
);
static int plan_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int resolve_exists_column_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *inner_source_context,
    const struct mylite_catalog_column_descriptor *inner_columns,
    size_t inner_column_count,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    const struct select_predicate_plan_options *options,
    const struct mylite_catalog_column_descriptor **out_column,
    size_t *out_source_index
);
static int resolve_exists_column_reference_in_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    const char *column_name,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const struct mylite_catalog_column_descriptor **out_column,
    size_t *out_source_index,
    bool *out_resolved
);
static int resolve_exists_column_reference_in_joined_source(
    struct mylite_db *database,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    const char *column_name,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor **out_column,
    size_t *out_source_index,
    bool *out_resolved
);
static bool exists_correlated_column_comparison_is_supported(
    const struct planned_select_predicate_node *node,
    size_t inner_source_index,
    size_t inner_source_count
);
static bool exists_source_index_is_inner(
    size_t source_index,
    size_t inner_source_index,
    size_t inner_source_count
);
static bool comparison_operator_is_string_predicate(enum mylite_sql_ast_operator operator_kind);
static bool comparison_operator_is_enum_predicate(enum mylite_sql_ast_operator operator_kind);
static bool comparison_operator_is_like(enum mylite_sql_ast_operator operator_kind);
static bool comparison_operator_is_regexp(enum mylite_sql_ast_operator operator_kind);
static int plan_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_is_boolean_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_comparison_result_is_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_row_scalar_expression_is_boolean_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_between_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_between_predicate_bound(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *bound_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool upper_bound,
    struct planned_select_predicate_node *node
);
static int plan_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static bool predicate_node_is_quantified_subquery(const struct mylite_sql_ast_node *predicate_node);
static int plan_quantified_subquery_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_row_constructor_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
);
static int plan_row_scalar_expression_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_row_scalar_predicate_left_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_in_literal_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static bool predicate_value_list_contains_row_scalar_predicate_value(
    const struct mylite_sql_ast_node *value_list
);
static int plan_descriptor_in_value_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_descriptor_in_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node,
    size_t value_index
);
static int plan_scalar_subquery_in_literal_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
);
static int plan_in_subquery_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_statement,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    size_t outer_source_count,
    struct planned_select_predicate_node *node
);
static int plan_in_subquery(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    size_t outer_source_count,
    struct planned_in_subquery *out_subquery
);
static int plan_in_subquery_table_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_in_subquery *out_subquery
);
static int plan_in_subquery_select_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    struct planned_in_subquery *subquery,
    struct mylite_catalog_column_descriptor *out_column
);
static bool in_subquery_columns_are_compatible(
    const struct mylite_catalog_column_descriptor *outer_column,
    const struct mylite_catalog_column_descriptor *inner_column
);
static int plan_in_subquery_inner_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_in_subquery *subquery
);
static int plan_row_scalar_in_value_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *context_column,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
);
static int plan_row_scalar_in_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *context_column,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node,
    size_t value_index
);
static int convert_predicate_in_value_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value **out_values,
    size_t *out_value_count
);
static int convert_predicate_in_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_predicate_year_in_value_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value **out_values,
    size_t *out_value_count
);
static bool year_in_value_list_uses_string_conversion(
    const struct mylite_sql_ast_node *value_list,
    size_t value_count
);
static int append_planned_select_predicate_node(
    struct mylite_db *database,
    struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *out_node_index
);
static int retain_planned_select_predicate_column(
    struct mylite_db *database,
    struct planned_select_predicate *predicate,
    const struct mylite_catalog_column_descriptor *column,
    const struct mylite_catalog_column_descriptor **out_column
);
static bool planned_select_predicate_columns_match(
    const struct catalog_column_descriptor_pair *columns
);
static int reserve_planned_select_predicate_nodes(
    struct mylite_db *database,
    struct planned_select_predicate *predicate,
    size_t required_capacity
);
static int reserve_planned_select_predicate_columns(
    struct mylite_db *database,
    struct planned_select_predicate *predicate,
    size_t required_capacity
);
static int append_deprecated_logical_and_warning(struct mylite_db *database);
static int append_deprecated_logical_or_warning(struct mylite_db *database);
static bool planned_select_predicate_has_expression(
    const struct planned_select_predicate *predicate
);
static int append_predicate_work_node(
    struct mylite_db *database,
    struct predicate_work_stack *stack,
    const struct mylite_sql_ast_node *node
);
static int append_predicate_work_deprecated_and_warning(
    struct mylite_db *database,
    struct predicate_work_stack *stack
);
static int append_predicate_work_deprecated_or_warning(
    struct mylite_db *database,
    struct predicate_work_stack *stack
);
static int append_predicate_work_finish_logical(
    struct mylite_db *database,
    struct predicate_work_stack *stack,
    enum mylite_sql_ast_operator operator_kind
);
static int append_predicate_work_finish_not(
    struct mylite_db *database,
    struct predicate_work_stack *stack
);
static int append_predicate_work_item(
    struct mylite_db *database,
    struct predicate_work_stack *stack,
    struct predicate_work_item item
);
static int append_predicate_result_index(
    struct mylite_db *database,
    struct predicate_result_stack *stack,
    size_t index
);
static int pop_predicate_result_index(struct predicate_result_stack *stack, size_t *out_index);
#ifdef MYLITE_EXECUTION_PARAMETER_BINDING_IMPLEMENTATION
static int bind_select_exists_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_exists_subquery *subquery,
    int *parameter_index
);
static int bind_select_predicate_node_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_non_in_predicate_node_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_predicate_node_parameters_without_subqueries(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_in_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_row_scalar_in_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_in_literal_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_scalar_subquery_in_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_scalar_aggregate_subquery_comparison_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_scalar_aggregate_subquery_between_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_scalar_aggregate_subquery_parameters(
    sqlite3_stmt *statement,
    const struct planned_scalar_aggregate_subquery *subquery,
    int *parameter_index
);
static int bind_scalar_aggregate_subquery_or_value_parameters(
    sqlite3_stmt *statement,
    const struct planned_scalar_aggregate_subquery *subquery,
    const struct planned_value *value,
    int *parameter_index
);
static int bind_select_in_value_list_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_in_subquery_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_in_subquery *subquery,
    int *parameter_index
);
static int bind_select_quantified_subquery_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
#endif
static int resolve_predicate_column_with_source_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor **out_column,
    size_t *out_source_index
);
static int convert_predicate_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_predicate_integer_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    bool is_negative,
    struct planned_value *out_value
);
static int convert_predicate_integer_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int scan_predicate_integer_string_prefix(
    const char *text,
    size_t text_length,
    uint64_t *out_magnitude,
    bool *out_is_negative,
    bool *out_has_digits
);
static int append_predicate_truncated_double_warning(
    struct mylite_db *database,
    const char *value,
    size_t value_length
);
static int convert_predicate_string_numeric_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static bool predicate_integer_string_ascii_whitespace(unsigned char byte);
static int convert_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_predicate_value_with_date_comparison_mode(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value,
    bool *out_compare_date_as_datetime
);
static int convert_predicate_date_between_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *lower_node,
    const struct mylite_sql_ast_node *upper_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_lower_value,
    struct planned_value *out_upper_value,
    bool *out_compare_date_as_datetime
);
static int convert_predicate_date_in_value_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value **out_values,
    size_t *out_value_count,
    bool *out_compare_date_as_datetime
);
static int convert_predicate_date_in_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *values,
    size_t value_count,
    bool *out_uses_datetime
);
static int convert_predicate_date_in_values_to_datetime(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *values,
    size_t value_count
);
static bool date_in_value_already_compares_as_datetime(
    const struct mylite_sql_ast_node *value_node,
    const struct planned_value *value
);
static void planned_value_array_deinit(struct planned_value *values, size_t value_count);
static int convert_predicate_year_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_predicate_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_value *out_value
);
static int convert_predicate_string_pattern_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_value *out_value
);
static int convert_predicate_regexp_pattern_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    bool case_sensitive,
    struct planned_value *out_value
);
static int convert_predicate_date_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    bool force_datetime_comparison,
    struct planned_value *out_value,
    bool *out_compare_as_datetime
);
static bool normalize_date_datetime_predicate_text(
    const char *text,
    size_t text_length,
    char *out_text,
    bool *out_append_warning
);
static int copy_date_midnight_predicate_value(
    struct mylite_db *database,
    const char *date_text,
    struct planned_value *out_value
);
static int convert_predicate_time_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_predicate_datetime_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_predicate_timestamp_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);

struct temporal_predicate_normalization_input {
    const char *text;
    size_t text_length;
    int target_offset_minutes;
};

static bool normalize_iso_temporal_predicate_text(
    const struct temporal_predicate_normalization_input *input,
    char *out_text
);
static bool normalize_z_temporal_predicate_text(
    const char *text,
    size_t text_length,
    char *out_text
);
static bool parse_temporal_predicate_offset(const char *text, int *out_offset_minutes);
static bool shift_datetime_parts_by_minutes(
    const struct mylite_temporal_datetime_parts *parts,
    int delta_minutes,
    char *out_text
);
static bool format_datetime_parts_text(
    const struct mylite_temporal_datetime_parts *parts,
    char *out_text
);
static int convert_integer_for_predicate(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    int64_t *out_value
);
static int plan_update_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
);
static int plan_joined_update_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct select_source_context *source_context,
    struct planned_update *out_plan
);
static int plan_joined_update_multiple_assignments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct select_source_context *source_context,
    struct planned_update *out_plan
);
static int plan_joined_update_multiple_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment,
    const struct select_source_context *source_context,
    struct planned_update *out_plan,
    size_t assignment_index
);
static int validate_joined_update_multiple_assignment_targets(
    struct mylite_db *database,
    const struct planned_update *plan
);
static int reject_builtin_schema_joined_update_assignment_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list
);
static int plan_joined_update_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node,
    struct planned_select_source *out_source
);
static int finish_joined_update_target_plan(
    struct mylite_db *database,
    struct planned_update *out_plan
);
static int reject_builtin_schema_joined_update_target(
    struct mylite_db *database,
    const struct planned_update *plan
);
static void set_joined_update_wrong_usage_error(
    struct mylite_db *database,
    const char *clause_name
);
static int plan_update_multiple_assignments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
);
static int plan_update_multiple_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan,
    size_t assignment_index
);
static int plan_update_scalar_subquery_assignment(
    struct mylite_db *database,
    struct planned_update *out_plan
);
static int plan_update_scalar_aggregate_subquery_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_update *out_plan,
    bool *out_handled
);
static int plan_update_scalar_aggregate_subquery_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_update *out_plan,
    enum planned_column_aggregate_function *out_function,
    const struct mylite_sql_ast_node **out_aggregate_expression,
    const struct mylite_sql_ast_node **out_rhs_expression
);
static int plan_update_scalar_aggregate_rhs_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *rhs_expression,
    struct planned_update *out_plan
);
static int plan_update_scalar_aggregate_subquery_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    enum planned_column_aggregate_function function,
    const struct mylite_sql_ast_node *aggregate_expression,
    struct planned_update *out_plan
);
static int plan_update_date_interval_assignment(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
);
static int plan_update_arithmetic_assignment(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
);
static int plan_update_date_interval_source_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor *assignment_column
);
static int plan_update_date_interval_target(
    struct mylite_db *database,
    struct planned_update *plan
);
static int plan_update_column_copy_assignment(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
);
static bool update_column_copy_source_target_types_are_compatible(
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column
);
static int plan_update_row_scalar_assignment(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
);
static int plan_update_assignment_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor *table_columns,
    struct planned_update *plan,
    struct planned_row_scalar_expression *out_expression,
    bool *out_planned
);
static int update_date_interval_input_kind_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    enum mylite_date_interval_second_input_kind *out_kind
);
static bool update_date_interval_column_family_is_supported(
    const struct mylite_catalog_column_descriptor *column
);
static int validate_update_date_interval_string_target(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static void set_update_date_interval_column_mismatch_error(struct mylite_db *database);
static void set_update_date_interval_target_error(struct mylite_db *database);
static void set_update_date_interval_date_time_unit_error(struct mylite_db *database);
static void set_update_date_interval_keyed_target_error(struct mylite_db *database);
static int update_value_is_constant_arithmetic_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    bool *out_is_constant
);
static int update_constant_arithmetic_visit_node(
    struct mylite_db *database,
    struct scalar_arithmetic_node_stack *stack,
    const struct mylite_sql_ast_node *value_node,
    bool *in_out_admitted
);
static bool update_constant_arithmetic_literal_is_admitted(
    enum mylite_sql_ast_literal_kind literal_kind
);
static bool update_constant_arithmetic_unary_operator_is_admitted(
    enum mylite_sql_ast_operator operator_kind
);
static bool update_constant_arithmetic_binary_operator_is_admitted(
    enum mylite_sql_ast_operator operator_kind
);
static int update_constant_arithmetic_stack_push(
    struct mylite_db *database,
    struct scalar_arithmetic_node_stack *stack,
    const struct mylite_sql_ast_node *value_node
);
static int validate_update_arithmetic_assignment_target(
    struct mylite_db *database,
    const struct planned_update *plan
);
static int validate_update_unix_timestamp_arithmetic_assignment_target(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static bool update_assignment_column_is_keyed(const struct planned_update *plan);
static bool update_column_id_is_keyed(const struct planned_update *plan, int64_t column_id);
static bool update_assignment_value_is_multi_constant_supported(
    const struct mylite_sql_ast_node *value_node
);
static bool update_assignment_unary_value_is_multi_constant_supported(
    const struct mylite_sql_ast_node *value_node
);
static int validate_update_multiple_auto_update_columns(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
);
static void set_update_multiple_assignment_unsupported_error(struct mylite_db *database);
static void set_update_duplicate_assignment_unsupported_error(struct mylite_db *database);
static int validate_update_ignore_assignment_support(
    struct mylite_db *database,
    const struct planned_update *plan
);
static int validate_update_default_function_sources(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
);
static int validate_update_default_function_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct planned_update *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
);
static int validate_update_scalar_subquery_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int convert_update_value(
    struct mylite_db *database,
    const struct planned_update *plan,
    struct planned_value *out_value
);
static int convert_update_value_for_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_update_constant_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_update_unix_timestamp_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int validate_update_constant_arithmetic_assignment_target(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int convert_update_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_update_date_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_update_datetime_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int convert_update_timestamp_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int materialize_update_scalar_subquery_value(
    struct mylite_db *database,
    const struct planned_update *plan,
    struct planned_value *out_value
);
static void cap_update_scalar_subquery_limit(struct planned_select_limit *limit);
static int step_update_scalar_subquery_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_update *plan,
    struct planned_value *out_value
);
static int materialize_update_scalar_subquery_sqlite_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_update *plan,
    struct planned_value *out_value
);
static int materialize_update_scalar_aggregate_subquery_value(
    struct mylite_db *database,
    const struct planned_update *plan,
    struct planned_value *out_value
);
static int materialize_update_scalar_aggregate_sqlite_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_update *plan,
    struct planned_value *out_value
);
static int build_update_scalar_aggregate_assignment_sql(
    const struct planned_update *plan,
    char **out_sql
);
static int append_update_scalar_aggregate_assignment_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
static int bind_update_scalar_aggregate_assignment_parameters(
    sqlite3_stmt *statement,
    const struct planned_update *plan
);
static bool update_scalar_subquery_target_uses_text_storage(
    const struct mylite_catalog_column_descriptor *target_column
);
static bool update_scalar_subquery_target_uses_blob_storage(
    const struct mylite_catalog_column_descriptor *target_column
);
static int materialize_update_scalar_subquery_text_storage_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct mylite_catalog_column_descriptor *target_column,
    struct planned_value *out_value
);
static int materialize_update_scalar_subquery_blob_storage_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct mylite_catalog_column_descriptor *target_column,
    struct planned_value *out_value
);
static int validate_update_scalar_subquery_text_storage_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct mylite_catalog_column_descriptor *target_column
);
static int materialize_update_scalar_subquery_approximate_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct mylite_catalog_column_descriptor *target_column,
    struct planned_value *out_value
);
static int materialize_update_scalar_subquery_integer_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int sqlite_type,
    const struct mylite_catalog_column_descriptor *target_column,
    struct planned_value *out_value
);
static int copy_update_scalar_subquery_text_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    struct planned_value *out_value
);
static bool update_scalar_subquery_source_target_types_are_compatible(
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column
);
static const char *update_scalar_subquery_implicit_conversion_message(
    const struct mylite_catalog_column_descriptor *target_column
);
static int convert_update_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
);
static int plan_update_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
);
static int convert_limit_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    int64_t *out_value
);
static int integer_range_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const char *unsupported_message,
    struct integer_column_range *out_range
);
static int integer_range_for_logical_type(
    struct mylite_db *database,
    struct integer_logical_type_range_request request,
    struct integer_column_range *out_range
);
