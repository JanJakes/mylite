#ifndef MYLITE_RUNTIME_MYLITE_SELECT_GROUP_INVARIANT_H
#define MYLITE_RUNTIME_MYLITE_SELECT_GROUP_INVARIANT_H

#include "mylite_select_types.h"

#include <stdbool.h>
#include <stddef.h>

bool mylite_select_expression_contains_aggregate(const struct mylite_sql_ast_node *expression);
bool mylite_select_output_is_group_invariant(
    const struct mylite_select_plan *plan,
    size_t output_index
);
bool mylite_select_expression_is_group_invariant(
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    enum mylite_select_grouping_reference_policy reference_policy
);

#endif
