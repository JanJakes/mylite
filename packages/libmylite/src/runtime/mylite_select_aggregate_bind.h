#ifndef MYLITE_RUNTIME_MYLITE_SELECT_AGGREGATE_BIND_H
#define MYLITE_RUNTIME_MYLITE_SELECT_AGGREGATE_BIND_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

struct mylite_expression_value;
struct mylite_field_descriptor;
struct mylite_select_predicate_bind_callbacks;
struct mylite_select_subquery_bind_callbacks;

struct mylite_select_aggregate_bind_callbacks {
    const struct mylite_select_predicate_bind_callbacks *predicate_callbacks;
    const struct mylite_select_subquery_bind_callbacks *subquery_callbacks;
    int (*infer_aggregate_expression_descriptor)(mylite_db *database,
                                                 const struct mylite_select_plan *plan,
                                                 const struct mylite_sql_ast_node *expression,
                                                 struct mylite_field_descriptor *out_descriptor);
    int (*infer_expression_descriptor)(mylite_db *database, const struct mylite_select_plan *plan,
                                       const struct mylite_sql_ast_node *expression,
                                       const struct mylite_expression_value *value,
                                       struct mylite_field_descriptor *out_descriptor);
    int (*set_invalid_group_function_error)(mylite_db *database);
    int (*set_unsupported_projection_error)(mylite_db *database);
};

int mylite_select_bind_aggregate_aware_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan, const char *clause_context,
    const struct mylite_select_aggregate_bind_callbacks *callbacks);
int mylite_select_collect_aggregate_bindings(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan,
    const struct mylite_select_aggregate_bind_callbacks *callbacks);

#endif
