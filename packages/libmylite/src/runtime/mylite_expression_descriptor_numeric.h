#ifndef MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_NUMERIC_H
#define MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_NUMERIC_H

#include <mylite/mylite.h>

#include "mylite_field_descriptor.h"

#include <stdbool.h>

struct mylite_expression_value;
struct mylite_select_plan;
struct mylite_sql_ast_node;

struct mylite_expression_descriptor_numeric_callbacks {
    int (*infer_expression_descriptor)(
        mylite_db *database,
        const struct mylite_select_plan *plan,
        const struct mylite_sql_ast_node *expression,
        const struct mylite_expression_value *value,
        struct mylite_field_descriptor *out_descriptor
    );
};

bool mylite_expression_descriptor_infer_fixed_integer_function(
    const struct mylite_sql_ast_node *name,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor
);
bool mylite_expression_descriptor_infer_math_function(
    const struct mylite_sql_ast_node *name,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor
);
int mylite_expression_descriptor_infer_scalar_numeric_function(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks
);
struct mylite_field_descriptor mylite_expression_descriptor_numeric_double_function(bool nullable);
int mylite_expression_descriptor_infer_numeric_variadic_function(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks
);

#endif
