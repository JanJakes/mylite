#ifndef MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_SUBQUERY_H
#define MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_SUBQUERY_H

#include <mylite/mylite.h>

#include "mylite_field_descriptor.h"

struct mylite_expression_value;
struct mylite_select_plan;
struct mylite_select_subquery_bind_callbacks;
struct mylite_sql_ast_node;

struct mylite_expression_descriptor_subquery_callbacks {
    int (*infer_expression_descriptor)(mylite_db *database, const struct mylite_select_plan *plan,
                                       const struct mylite_sql_ast_node *expression,
                                       const struct mylite_expression_value *value,
                                       struct mylite_field_descriptor *out_descriptor);
    int (*prepare_select_subquery)(mylite_db *database, const struct mylite_sql_ast_node *statement,
                                   mylite_stmt **out_stmt);
    const struct mylite_select_subquery_bind_callbacks *bind_callbacks;
};

int mylite_expression_descriptor_infer_subquery_expression(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_subquery_callbacks *callbacks);
int mylite_expression_descriptor_infer_binary_subquery_expression(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_subquery_callbacks *callbacks);

#endif
