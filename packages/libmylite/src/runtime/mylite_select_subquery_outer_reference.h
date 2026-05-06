#ifndef MYLITE_RUNTIME_MYLITE_SELECT_SUBQUERY_OUTER_REFERENCE_H
#define MYLITE_RUNTIME_MYLITE_SELECT_SUBQUERY_OUTER_REFERENCE_H

#include <stdbool.h>

struct mylite_select_plan;
struct mylite_sql_ast_node;

bool mylite_select_subquery_references_outer_plan(
    const struct mylite_sql_ast_node *node,
    const struct mylite_select_plan *outer_plan,
    const struct mylite_sql_ast_node *select_statement
);
bool mylite_select_subquery_has_unqualified_outer_column_reference(
    const struct mylite_sql_ast_node *node,
    const struct mylite_select_plan *outer_plan
);

#endif
