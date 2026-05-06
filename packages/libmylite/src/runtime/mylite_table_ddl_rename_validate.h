#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_RENAME_VALIDATE_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_RENAME_VALIDATE_H

#include "mylite_table_ddl_types.h"

#include <mylite/mylite.h>

int mylite_table_ddl_validate_rename_table_plan(
    mylite_db *database,
    const char *selected_schema,
    struct mylite_rename_table_plan *plan
);

#endif
