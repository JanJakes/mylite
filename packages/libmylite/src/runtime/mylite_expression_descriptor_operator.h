#ifndef MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_OPERATOR_H
#define MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_OPERATOR_H

#include <mylite/mylite.h>

#include "mylite_field_descriptor.h"

struct mylite_expression_descriptor_subquery_callbacks;
struct mylite_expression_value;
struct mylite_select_plan;
struct mylite_sql_ast_node;

struct mylite_expression_descriptor_operator_callbacks {
    int (*infer_expression_descriptor)(mylite_db *database, const struct mylite_select_plan *plan,
                                       const struct mylite_sql_ast_node *expression,
                                       const struct mylite_expression_value *value,
                                       struct mylite_field_descriptor *out_descriptor);
    const struct mylite_expression_descriptor_subquery_callbacks *subquery_callbacks;
};

int mylite_expression_descriptor_infer_unary_expression(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_operator_callbacks *callbacks);
int mylite_expression_descriptor_infer_binary_expression(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_operator_callbacks *callbacks);
int mylite_expression_descriptor_infer_ternary_expression(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_operator_callbacks *callbacks);

#endif
