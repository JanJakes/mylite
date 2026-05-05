#ifndef MYLITE_RUNTIME_MYLITE_DML_INSERT_UPDATE_EXPRESSION_H
#define MYLITE_RUNTIME_MYLITE_DML_INSERT_UPDATE_EXPRESSION_H

#include "mylite_dml_types.h"

int mylite_dml_evaluate_insert_update_expression(
    mylite_db *database, const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan, const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    const struct mylite_insert_value *value, const struct mylite_insert_bound_value *target_values,
    const struct mylite_insert_bound_value *candidate_values,
    struct mylite_insert_bound_value *out_value);

#endif
