#ifndef MYLITE_RUNTIME_MYLITE_SELECT_GROUP_BIND_H
#define MYLITE_RUNTIME_MYLITE_SELECT_GROUP_BIND_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

struct mylite_select_aggregate_bind_callbacks;
struct mylite_select_predicate_bind_callbacks;

struct mylite_select_group_bind_callbacks {
    const struct mylite_select_aggregate_bind_callbacks *aggregate_callbacks;
    const struct mylite_select_predicate_bind_callbacks *predicate_callbacks;
    int (*set_invalid_group_function_error)(mylite_db *database);
    int (*set_unsupported_where_error)(mylite_db *database);
};

int mylite_select_bind_group_by_clause(mylite_db *database,
                                       const struct mylite_sql_ast_node *group_by_clause,
                                       struct mylite_select_plan *plan,
                                       const struct mylite_select_group_bind_callbacks *callbacks);
int mylite_select_bind_having_clause(mylite_db *database,
                                     const struct mylite_sql_ast_node *having_clause,
                                     struct mylite_select_plan *plan,
                                     const struct mylite_select_group_bind_callbacks *callbacks);

#endif
