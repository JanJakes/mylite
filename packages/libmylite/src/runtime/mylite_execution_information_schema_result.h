#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_RESULT_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_RESULT_H

#include "mylite_result.h"

#include <stdbool.h>
#include <stddef.h>

struct information_schema_projection_expression;
struct information_schema_query;
struct information_schema_row_index_set;
struct information_schema_row_set;
struct information_schema_result_projection_buffers;
struct mylite_db;
struct mylite_result_column_descriptor;

int mylite_execution_information_schema_append_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_query *query
);
int mylite_execution_information_schema_append_result_rows(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
);
int mylite_execution_information_schema_matching_row_indexes(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    struct information_schema_row_index_set *out_indexes
);
int mylite_execution_information_schema_make_source_column_descriptor(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t projection_index,
    struct mylite_result_column_descriptor *out_descriptor
);
int mylite_execution_information_schema_result_projection_buffers_init(
    struct mylite_db *database,
    const struct information_schema_query *query,
    struct information_schema_result_projection_buffers *buffers
);
void mylite_execution_information_schema_result_projection_buffers_deinit(
    struct information_schema_result_projection_buffers *buffers
);
int mylite_execution_information_schema_append_projected_result_row(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    const struct information_schema_result_projection_buffers *buffers,
    mylite_result *result
);
int mylite_execution_information_schema_append_count_result(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
);
int mylite_execution_information_schema_append_grouped_statistics_result_rows(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
);
int mylite_execution_information_schema_integer_literal_projection_value(
    struct mylite_db *database,
    const struct information_schema_projection_expression *expression,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
int mylite_execution_information_schema_includes_connection_control_failed_login_attempts(
    struct mylite_db *database,
    const struct information_schema_query *query,
    bool *out_includes_table
);
bool mylite_execution_information_schema_row_is_failed_login_attempt(const char *const *row);
void mylite_execution_information_schema_row_index_set_deinit(
    struct information_schema_row_index_set *indexes
);
int mylite_execution_information_schema_sort_tables_default_row_indexes(
    struct mylite_db *database,
    const struct information_schema_row_set *rows,
    size_t *indexes,
    size_t index_count
);

#endif
