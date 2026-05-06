#ifndef MYLITE_RUNTIME_MYLITE_DML_INSERT_SET_EXPRESSION_H
#define MYLITE_RUNTIME_MYLITE_DML_INSERT_SET_EXPRESSION_H

#include "mylite_dml_types.h"

int mylite_dml_evaluate_insert_set_expression(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    const struct mylite_insert_value *value,
    const struct mylite_insert_bound_value *values,
    struct mylite_insert_bound_value *out_value
);

#endif
