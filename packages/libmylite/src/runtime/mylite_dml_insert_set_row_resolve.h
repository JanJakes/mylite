#ifndef MYLITE_RUNTIME_MYLITE_DML_INSERT_SET_ROW_RESOLVE_H
#define MYLITE_RUNTIME_MYLITE_DML_INSERT_SET_ROW_RESOLVE_H

#include "mylite_dml_types.h"

#include <stdint.h>

int mylite_dml_resolve_insert_set_row_values(
    mylite_db *database, const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_set_plan *set_plan, const struct mylite_insert_table *table,
    const size_t *column_indexes, size_t column_index_count, uint64_t statement_row_count,
    struct mylite_insert_execution_state *state, struct mylite_insert_bound_value *values,
    struct mylite_insert_set_row_state *row_state,
    const struct mylite_dml_expression_callbacks *callbacks);

#endif
