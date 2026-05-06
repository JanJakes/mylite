#ifndef MYLITE_RUNTIME_MYLITE_SELECT_ORDER_BIND_H
#define MYLITE_RUNTIME_MYLITE_SELECT_ORDER_BIND_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

struct mylite_select_aggregate_bind_callbacks;
struct mylite_select_subquery_bind_callbacks;

struct mylite_select_order_bind_callbacks {
    const struct mylite_select_aggregate_bind_callbacks *aggregate_callbacks;
    const struct mylite_select_subquery_bind_callbacks *subquery_callbacks;
    int (*set_unsupported_order_error)(mylite_db *database);
};

int mylite_select_bind_order_by_clause(
    mylite_db *database,
    const struct mylite_sql_ast_node *order_by_clause,
    struct mylite_select_plan *plan,
    const struct mylite_select_order_bind_callbacks *callbacks
);

#endif
