#ifndef MYLITE_RUNTIME_MYLITE_SELECT_AGGREGATE_COUNT_DISTINCT_BIND_H
#define MYLITE_RUNTIME_MYLITE_SELECT_AGGREGATE_COUNT_DISTINCT_BIND_H

#include "mylite_select_aggregate_bind.h"

int mylite_select_bind_count_distinct_arguments(
    mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    struct mylite_select_plan *plan,
    const struct mylite_select_aggregate_bind_callbacks *callbacks
);
int mylite_select_infer_count_distinct_argument_descriptors(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    struct mylite_select_aggregate_binding *binding,
    const struct mylite_select_aggregate_bind_callbacks *callbacks
);

#endif
