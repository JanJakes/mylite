#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_UNIQUE_VALIDATE_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_UNIQUE_VALIDATE_H

#include "mylite_table_ddl_types.h"

#include <mylite/mylite.h>

int mylite_table_ddl_validate_alter_table_unique_indexes(
    mylite_db *database, const struct mylite_alter_table_model *model);

#endif
