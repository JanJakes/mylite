#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_ROW_SCALAR_SQL_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_ROW_SCALAR_SQL_H

#include <stdbool.h>
#include <stddef.h>

struct mylite_catalog_column_descriptor;
struct mylite_dynamic_string;
struct planned_row_scalar_expression;

int append_row_scalar_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);

bool row_scalar_expression_is_binary_sensitive_collate(
    const struct planned_row_scalar_expression *expression
);

int append_row_scalar_json_constructor_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *argument,
    size_t *next_parameter
);

int mylite_execution_append_descriptor_value_sql_for_source(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify
);

bool mylite_execution_column_descriptor_is_binary_value(
    const struct mylite_catalog_column_descriptor *column
);

#endif
