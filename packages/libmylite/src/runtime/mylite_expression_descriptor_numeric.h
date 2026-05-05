#ifndef MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_NUMERIC_H
#define MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_NUMERIC_H

#include "mylite_field_descriptor.h"

#include <stdbool.h>

struct mylite_sql_ast_node;

bool mylite_expression_descriptor_infer_fixed_integer_function(
    const struct mylite_sql_ast_node *name, bool result_nullable,
    struct mylite_field_descriptor *out_descriptor);
bool mylite_expression_descriptor_infer_math_function(
    const struct mylite_sql_ast_node *name, bool result_nullable,
    struct mylite_field_descriptor *out_descriptor);

#endif
