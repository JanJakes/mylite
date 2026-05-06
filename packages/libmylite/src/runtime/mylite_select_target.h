#ifndef MYLITE_RUNTIME_MYLITE_SELECT_TARGET_H
#define MYLITE_RUNTIME_MYLITE_SELECT_TARGET_H

#include "mylite_select_types.h"

#include <mylite/mylite.h>

int mylite_select_resolve_table_target(mylite_db *database, struct mylite_select_table *table);
int mylite_select_resolve_query_table_target(
    mylite_db *database,
    struct mylite_select_table *table
);
bool mylite_select_schema_name_is_system(const char *schema_name);

#endif
