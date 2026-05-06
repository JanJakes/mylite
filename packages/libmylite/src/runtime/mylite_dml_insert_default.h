#ifndef MYLITE_RUNTIME_MYLITE_DML_INSERT_DEFAULT_H
#define MYLITE_RUNTIME_MYLITE_DML_INSERT_DEFAULT_H

#include "mylite_dml_types.h"

#include <stdint.h>

int mylite_dml_resolve_insert_default_bound_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *out_value
);
int mylite_dml_resolve_insert_implicit_expression_default(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_insert_bound_value *out_value
);
int mylite_dml_resolve_insert_current_timestamp_bound_value(
    mylite_db *database,
    struct mylite_insert_bound_value *out_value
);
uint64_t mylite_dml_insert_auto_increment_next_value(
    const struct mylite_insert_execution_state *state
);
int mylite_dml_resolve_insert_explicit_default_value(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table_column *column,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *out_value
);
int mylite_dml_resolve_insert_omitted_default_value(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table_column *column,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    size_t column_index,
    struct mylite_insert_bound_value *out_value
);
int mylite_dml_resolve_insert_text_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    const char *text,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *out_value
);
int mylite_dml_resolve_insert_quoted_text_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    const char *text,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *out_value
);
bool mylite_dml_insert_auto_increment_zero_generates(
    const mylite_db *database,
    const struct mylite_insert_table_column *column
);
int mylite_dml_allocate_insert_auto_increment(
    mylite_db *database,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *out_value
);

#endif
