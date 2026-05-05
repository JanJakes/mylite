#ifndef MYLITE_RUNTIME_MYLITE_SELECT_FROM_USING_H
#define MYLITE_RUNTIME_MYLITE_SELECT_FROM_USING_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

int mylite_select_from_add_using_request(mylite_db *database, struct mylite_select_plan *plan,
                                         const struct mylite_sql_ast_node *condition,
                                         size_t left_first_table, size_t left_table_count,
                                         size_t right_first_table, size_t right_table_count,
                                         enum mylite_sql_ast_join_type join_type);
int mylite_select_from_resolve_using_requests(mylite_db *database, struct mylite_select_plan *plan);

#endif
