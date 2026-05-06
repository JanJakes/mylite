#ifndef MYLITE_RUNTIME_MYLITE_SELECT_ORDER_RESOLVE_H
#define MYLITE_RUNTIME_MYLITE_SELECT_ORDER_RESOLVE_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

#include <stddef.h>

int mylite_select_set_unknown_order_column_error(mylite_db *database, const char *column_name);
int mylite_select_set_ambiguous_order_column_error(mylite_db *database, const char *column_name);
int mylite_select_resolve_order_reference(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    enum mylite_select_order_key_kind *out_kind,
    size_t *out_index
);

#endif
