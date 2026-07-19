#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_connection.h"
#include "mylite_date_interval_second.h"
#include "mylite_dynamic_string.h"
#include "mylite_execution_ast_internal.h"
#include "mylite_execution_catalog.h"
#include "mylite_execution_diagnostics.h"
#include "mylite_execution_information_schema_predicate.h"
#include "mylite_execution_information_schema_result.h"
#include "mylite_execution_information_schema_result_support.h"
#include "mylite_execution_information_schema_values.h"
#include "mylite_execution_plan_types.h"
#include "mylite_execution_result_rows.h"
#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_regexp.h"
#include "mylite_execution_scalar_string_position.h"
#include "mylite_execution_select_order_plan.h"
#include "mylite_execution_sql_normalization.h"
#include "mylite_execution_statement_transaction.h"
#include "mylite_execution_text_internal.h"
#include "mylite_numeric_locale.h"
#include "mylite_result.h"
#include "mylite_spatial.h"
#include "mylite_statement_completion.h"
#include "mylite_statement_context.h"
#include "mylite_string_bitmask.h"
#include "mylite_sys_functions.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mylite_execution_declarations_00_constants.inc"
#include "mylite_execution_declarations_01_types.inc"
#include "mylite_execution_information_schema_result_internal.h"

#include <errno.h>

int mylite_execution_information_schema_append_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_query *query
) {
    return information_schema_append_result_columns(database, result, query);
}

int mylite_execution_information_schema_append_result_rows(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
) {
    return information_schema_append_result_rows(database, query, rows, result, out_read_row_count);
}

int mylite_execution_information_schema_matching_row_indexes(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    struct information_schema_row_index_set *out_indexes
) {
    return information_schema_matching_row_indexes(database, query, rows, out_indexes);
}

int mylite_execution_information_schema_make_source_column_descriptor(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t projection_index,
    struct mylite_result_column_descriptor *out_descriptor
) {
    return information_schema_make_source_column_descriptor(
        database,
        query,
        projection_index,
        out_descriptor
    );
}

int mylite_execution_information_schema_result_projection_buffers_init(
    struct mylite_db *database,
    const struct information_schema_query *query,
    struct information_schema_result_projection_buffers *buffers
) {
    return information_schema_result_projection_buffers_init(database, query, buffers);
}

void mylite_execution_information_schema_result_projection_buffers_deinit(
    struct information_schema_result_projection_buffers *buffers
) {
    information_schema_result_projection_buffers_deinit(buffers);
}

int mylite_execution_information_schema_append_projected_result_row(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    const struct information_schema_result_projection_buffers *buffers,
    mylite_result *result
) {
    return information_schema_append_projected_result_row(database, query, row, buffers, result);
}

int mylite_execution_information_schema_append_count_result(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
) {
    return information_schema_append_count_result(
        database,
        query,
        rows,
        result,
        out_read_row_count
    );
}

int mylite_execution_information_schema_append_grouped_statistics_result_rows(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
) {
    return information_schema_append_grouped_statistics_result_rows(
        database,
        query,
        rows,
        result,
        out_read_row_count
    );
}

int mylite_execution_information_schema_integer_literal_projection_value(
    struct mylite_db *database,
    const struct information_schema_projection_expression *expression,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    return information_schema_integer_literal_projection_value(
        database,
        expression,
        buffer,
        buffer_size,
        out_value
    );
}

int mylite_execution_information_schema_includes_connection_control_failed_login_attempts(
    struct mylite_db *database,
    const struct information_schema_query *query,
    bool *out_includes_table
) {
    return information_schema_includes_connection_control_failed_login_attempts(
        database,
        query,
        out_includes_table
    );
}

bool mylite_execution_information_schema_row_is_failed_login_attempt(const char *const *row) {
    return information_schema_row_is_connection_control_failed_login_attempts(row);
}

void mylite_execution_information_schema_row_index_set_deinit(
    struct information_schema_row_index_set *indexes
) {
    information_schema_row_index_set_deinit(indexes);
}

int mylite_execution_information_schema_sort_tables_default_row_indexes(
    struct mylite_db *database,
    const struct information_schema_row_set *rows,
    size_t *indexes,
    size_t index_count
) {
    return information_schema_sort_tables_default_row_indexes(database, rows, indexes, index_count);
}

static struct mylite_result_column_descriptor unknown_result_column_descriptor(const char *label) {
    return mylite_execution_information_schema_unknown_result_column_descriptor(label);
}

static int populate_select_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    const struct result_column_metadata_context *metadata_context,
    struct mylite_result_column_descriptor *descriptor
) {
    (void)table;
    (void)metadata_context;
    return mylite_execution_information_schema_populate_result_column_descriptor(
        database,
        column,
        descriptor
    );
}

static struct result_column_metadata_context result_column_metadata_context_init(void) {
    return (struct result_column_metadata_context){0};
}

static uint64_t character_set_max_bytes_per_character(const char *character_set_name) {
    return mylite_execution_information_schema_character_set_max_bytes_per_character(
        character_set_name
    );
}

static uint64_t result_metadata_collation_max_bytes_per_character(const char *collation_name) {
    return mylite_execution_information_schema_result_collation_max_bytes_per_character(
        collation_name
    );
}

static uint64_t result_metadata_display_length_cap(uint64_t display_length) {
    return mylite_execution_information_schema_result_display_length_cap(display_length);
}

static uint32_t result_metadata_collation_id(const char *collation_name) {
    return mylite_execution_information_schema_result_collation_id(collation_name);
}

static bool text_equals_ascii_case_insensitive(const char *left, const char *right) {
    return mylite_execution_text_equals_ascii_case_insensitive(left, right);
}

#include "mylite_execution_information_schema_result_rows.inc"
