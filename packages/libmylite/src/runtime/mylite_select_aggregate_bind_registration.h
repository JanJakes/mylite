#ifndef MYLITE_RUNTIME_MYLITE_SELECT_AGGREGATE_BIND_REGISTRATION_H
#define MYLITE_RUNTIME_MYLITE_SELECT_AGGREGATE_BIND_REGISTRATION_H

#include <mylite/mylite.h>

#include "mylite_select_aggregate_bind.h"

#include <stdbool.h>

bool mylite_select_aggregate_bind_callbacks_are_valid(
    const struct mylite_select_aggregate_bind_callbacks *callbacks);
int mylite_select_bind_aggregate_call(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan,
    const struct mylite_select_aggregate_bind_callbacks *callbacks);

#endif
