#ifndef MYLITE_RUNTIME_MYLITE_SELECT_FROM_H
#define MYLITE_RUNTIME_MYLITE_SELECT_FROM_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

int mylite_select_bind_from_clause(mylite_db *database,
                                   const struct mylite_sql_ast_node *from_clause,
                                   struct mylite_select_plan *plan);

#endif
