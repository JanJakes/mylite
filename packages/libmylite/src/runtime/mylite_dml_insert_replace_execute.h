#ifndef MYLITE_RUNTIME_MYLITE_DML_INSERT_REPLACE_EXECUTE_H
#define MYLITE_RUNTIME_MYLITE_DML_INSERT_REPLACE_EXECUTE_H

#include <mylite/mylite.h>

#include "mylite_dml_types.h"

int mylite_dml_write_replace_candidate_row(
    mylite_db *database,
    sqlite3_stmt *insert,
    sqlite3_stmt *delete_stmt,
    const char *schema_name,
    const char *table_name,
    const struct mylite_insert_table *table,
    struct mylite_insert_execution_state *state,
    const struct mylite_insert_bound_value *values
);
int mylite_dml_execute_replace_row(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const char *schema_name,
    sqlite3_stmt *insert,
    sqlite3_stmt *delete_stmt,
    const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    struct mylite_insert_execution_state *state,
    size_t row_index,
    const struct mylite_dml_expression_callbacks *callbacks
);
int mylite_dml_execute_replace_set_row(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_set_plan *set_plan,
    sqlite3_stmt *insert,
    sqlite3_stmt *delete_stmt,
    const struct mylite_insert_table *table,
    const size_t *column_indexes,
    size_t column_index_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *values,
    struct mylite_insert_set_row_state *row_state,
    const struct mylite_dml_expression_callbacks *callbacks
);

#endif
