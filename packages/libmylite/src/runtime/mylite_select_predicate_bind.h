#ifndef MYLITE_RUNTIME_MYLITE_SELECT_PREDICATE_BIND_H
#define MYLITE_RUNTIME_MYLITE_SELECT_PREDICATE_BIND_H

#include <mylite/mylite.h>

#include "mylite_select_subquery.h"
#include "mylite_select_types.h"

struct mylite_select_predicate_bind_callbacks {
    const struct mylite_select_subquery_bind_callbacks *subquery_callbacks;
    int (*set_invalid_group_function_error)(mylite_db *database);
    int (*set_unsupported_where_error)(mylite_db *database);
};

int mylite_select_bind_where_clause(mylite_db *database,
                                    const struct mylite_sql_ast_node *where_clause,
                                    const struct mylite_select_plan *plan,
                                    const struct mylite_select_predicate_bind_callbacks *callbacks);
int mylite_select_bind_join_predicates(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_select_predicate_bind_callbacks *callbacks);
int mylite_select_bind_predicate_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_select_plan *plan,
    const struct mylite_select_predicate_bind_callbacks *callbacks);

#endif
