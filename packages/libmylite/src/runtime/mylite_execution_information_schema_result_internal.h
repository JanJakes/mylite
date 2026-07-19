#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_RESULT_INTERNAL_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_RESULT_INTERNAL_H

static int information_schema_append_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_query *query
);
static int information_schema_append_result_rows(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
);
static int information_schema_matching_row_indexes(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    struct information_schema_row_index_set *out_indexes
);
static int information_schema_make_source_column_descriptor(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t projection_index,
    struct mylite_result_column_descriptor *out_descriptor
);
static int information_schema_result_projection_buffers_init(
    struct mylite_db *database,
    const struct information_schema_query *query,
    struct information_schema_result_projection_buffers *buffers
);
static void information_schema_result_projection_buffers_deinit(
    struct information_schema_result_projection_buffers *buffers
);
static int information_schema_append_projected_result_row(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    const struct information_schema_result_projection_buffers *buffers,
    mylite_result *result
);
static int information_schema_append_count_result(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
);
static int information_schema_append_grouped_statistics_result_rows(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
);
static int information_schema_integer_literal_projection_value(
    struct mylite_db *database,
    const struct information_schema_projection_expression *expression,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
static int information_schema_includes_connection_control_failed_login_attempts(
    struct mylite_db *database,
    const struct information_schema_query *query,
    bool *out_includes_table
);
static bool information_schema_row_is_connection_control_failed_login_attempts(
    const char *const *row
);
static void information_schema_row_index_set_deinit(struct information_schema_row_index_set *indexes
);
static int information_schema_sort_tables_default_row_indexes(
    struct mylite_db *database,
    const struct information_schema_row_set *rows,
    size_t *indexes,
    size_t index_count
);
static int information_schema_make_result_column_descriptor(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t projection_index,
    struct mylite_result_column_descriptor *out_descriptor
);
static int information_schema_initialize_catalog_column_descriptor(
    struct mylite_db *database,
    const struct mylite_execution_catalog_column_definition *column_definition,
    struct mylite_catalog_column_descriptor *out_column
);
static int information_schema_copy_descriptor_text(
    struct mylite_db *database,
    const char *source,
    char *destination,
    size_t destination_size
);
static void information_schema_apply_connection_text_metadata(
    const struct mylite_db *database,
    const struct mylite_execution_catalog_column_definition *column_definition,
    struct mylite_result_column_descriptor *descriptor
);
static void information_schema_apply_temporal_precision_metadata(
    const struct mylite_execution_catalog_column_definition *column_definition,
    struct mylite_result_column_descriptor *descriptor
);
static void information_schema_apply_system_key_flags(
    const struct information_schema_query *query,
    size_t column_index,
    struct mylite_result_column_descriptor *descriptor
);
static bool information_schema_source_column_has_binary_numeric_flag(
    const struct information_schema_query *query,
    const struct mylite_result_column_descriptor *descriptor
);
static void information_schema_make_count_result_column_descriptor(
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
);
static void information_schema_make_unsigned_result_column_descriptor(
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
);
static void information_schema_make_group_concat_result_column_descriptor(
    const struct mylite_db *database,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
);
static void information_schema_make_logical_result_column_descriptor(
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
);
static int information_schema_append_streamed_result_rows(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
);
static bool information_schema_result_rows_can_stream(const struct information_schema_query *query);
static int information_schema_sort_grouped_statistics_row_indexes(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t *indexes,
    size_t index_count
);
static int information_schema_sort_row_indexes_by(
    struct mylite_db *database,
    const struct information_schema_row_index_sort *sort,
    size_t *indexes,
    size_t index_count
);
static void information_schema_insertion_sort_row_indexes_by(
    const struct information_schema_row_index_sort *sort,
    size_t *indexes,
    size_t index_count
);
static void information_schema_merge_row_index_runs(
    const struct information_schema_row_index_sort *sort,
    size_t *indexes,
    size_t *scratch,
    struct information_schema_merge_range range
);
static int information_schema_compare_row_index_sort(
    const struct information_schema_row_index_sort *sort,
    size_t left_row,
    size_t right_row
);
static int information_schema_compare_grouped_statistics_rows(
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t left_row,
    size_t right_row
);
static bool information_schema_grouped_statistics_rows_same_group(
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t left_row,
    size_t right_row
);
static int information_schema_compare_row_values(
    const struct mylite_execution_catalog_table_definition *definition,
    size_t column_index,
    const char *left,
    const char *right
);
static int information_schema_group_concat_ordered_column_value(
    struct mylite_db *database,
    const struct information_schema_row_set *rows,
    const size_t *indexes,
    struct information_schema_group_concat_order order,
    char **out_value
);
static int information_schema_append_grouped_statistics_projected_row(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    const size_t *indexes,
    size_t start_index,
    size_t end_index,
    const struct information_schema_result_projection_buffers *buffers,
    mylite_result *result
);
static bool information_schema_query_needs_computed_projection_buffers(
    const struct information_schema_query *query
);
static bool information_schema_projection_needs_computed_buffer(
    enum information_schema_projection_kind kind
);
static char *information_schema_projection_computed_buffer(
    const struct information_schema_result_projection_buffers *buffers,
    size_t projection_index
);
static int information_schema_projection_value(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    size_t projection_index,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
static int information_schema_logical_not_column_projection_value(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    size_t projection_index,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
static int information_schema_unsigned_projection_expression_value(
    struct mylite_db *database,
    const char *const *row,
    const struct information_schema_projection_expression *expression,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
static int information_schema_numeric_projection_expression_value(
    struct mylite_db *database,
    const char *const *row,
    const struct information_schema_projection_expression *expression,
    double *out_value,
    bool *out_is_null
);
static int information_schema_numeric_projection_apply_divide_frame(
    struct mylite_db *database,
    struct information_schema_numeric_eval_value_stack *values
);
static int information_schema_numeric_eval_value_stack_push(
    struct mylite_db *database,
    struct information_schema_numeric_eval_value_stack *stack,
    struct information_schema_numeric_eval_value value
);
static void information_schema_numeric_eval_value_stack_deinit(
    struct information_schema_numeric_eval_value_stack *stack
);
static int information_schema_numeric_text_value(
    struct mylite_db *database,
    const char *text,
    double *out_value,
    bool *out_is_null
);
static void information_schema_row_index_set_init(struct information_schema_row_index_set *indexes);
static int information_schema_row_index_set_append(
    struct mylite_db *database,
    struct information_schema_row_index_set *indexes,
    size_t value
);
static int information_schema_sort_row_indexes(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t *indexes,
    size_t index_count
);
static int information_schema_compare_tables_default_rows(
    const struct information_schema_row_set *rows,
    size_t left_row,
    size_t right_row
);
static int information_schema_tables_default_schema_priority(const char *schema_name);
static int information_schema_tables_default_table_priority(
    const char *schema_name,
    const char *table_name
);
static int information_schema_compare_rows(
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    struct information_schema_row_order_pair pair
);
static int information_schema_compare_rows_by_column(
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    struct information_schema_row_order_pair pair,
    struct information_schema_row_order_column order
);

#endif
