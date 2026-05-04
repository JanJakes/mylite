#ifndef MYLITE_RUNTIME_MYLITE_SELECT_PROJECTION_H
#define MYLITE_RUNTIME_MYLITE_SELECT_PROJECTION_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

int mylite_select_append_wildcard_outputs(mylite_db *database,
                                          const struct mylite_sql_ast_node *wildcard,
                                          struct mylite_select_plan *plan);

#endif
