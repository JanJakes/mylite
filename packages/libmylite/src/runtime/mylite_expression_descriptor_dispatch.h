#ifndef MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_DISPATCH_H
#define MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_DISPATCH_H

#include <mylite/mylite.h>

#include "mylite_field_descriptor.h"

struct mylite_expression_value;
struct mylite_select_plan;
struct mylite_sql_ast_node;

int mylite_expression_descriptor_infer_select(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor
);
int mylite_expression_descriptor_infer_scalar(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor
);
int mylite_expression_descriptor_infer_collation(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor
);
int mylite_expression_descriptor_infer_aggregate(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor
);
int mylite_expression_descriptor_infer(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor
);

#endif
