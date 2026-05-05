#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_COLUMN_VALUE_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_COLUMN_VALUE_H

#include "mylite_table_ddl_types.h"

#include <mylite/mylite.h>

struct mylite_insert_bound_value;

int mylite_table_ddl_resolve_alter_table_added_column_value(
    mylite_db *database, const struct mylite_alter_table_column *column,
    struct mylite_insert_bound_value *out_value);

#endif
