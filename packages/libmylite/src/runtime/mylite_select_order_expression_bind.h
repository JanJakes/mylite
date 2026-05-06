#ifndef MYLITE_RUNTIME_MYLITE_SELECT_ORDER_EXPRESSION_BIND_H
#define MYLITE_RUNTIME_MYLITE_SELECT_ORDER_EXPRESSION_BIND_H

#include <mylite/mylite.h>

struct mylite_select_order_bind_callbacks;
struct mylite_select_plan;
struct mylite_sql_ast_node;

int mylite_select_bind_order_expression(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan,
    const struct mylite_select_order_bind_callbacks *callbacks
);

#endif
