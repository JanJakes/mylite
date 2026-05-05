#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_CREATE_SQL_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_CREATE_SQL_H

#include "mylite_table_ddl_types.h"

#include <mylite/mylite.h>

struct mylite_schema_default;

int mylite_table_ddl_create_physical_table(mylite_db *database, const char *schema_name,
                                           const struct mylite_schema_default *schema_default,
                                           const struct mylite_create_table_plan *plan);

#endif
