#ifndef MYLITE_RUNTIME_MYLITE_SELECT_SCALAR_ORDER_VALIDATE_H
#define MYLITE_RUNTIME_MYLITE_SELECT_SCALAR_ORDER_VALIDATE_H

#include <mylite/mylite.h>

struct mylite_result_metadata;
struct mylite_select_scalar_eval_callbacks;
struct mylite_sql_ast_node;

int mylite_select_scalar_validate_order_by_clause(
    mylite_db *database,
    const struct mylite_sql_ast_node *order_by_clause,
    const struct mylite_result_metadata *metadata,
    const struct mylite_select_scalar_eval_callbacks *callbacks
);

#endif
