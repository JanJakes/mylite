#ifndef MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_CAST_H
#define MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_CAST_H

#include <mylite/mylite.h>

#include "mylite_field_descriptor.h"

struct mylite_expression_value;
struct mylite_select_plan;
struct mylite_sql_ast_node;

struct mylite_expression_descriptor_cast_callbacks {
    int (*infer_expression_descriptor)(mylite_db *database, const struct mylite_select_plan *plan,
                                       const struct mylite_sql_ast_node *expression,
                                       const struct mylite_expression_value *value,
                                       struct mylite_field_descriptor *out_descriptor);
};

int mylite_expression_descriptor_infer_cast_expression(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_cast_callbacks *callbacks);

#endif
