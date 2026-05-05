#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_CREATE_VALIDATE_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_CREATE_VALIDATE_H

#include "mylite_table_ddl_types.h"

#include <mylite/mylite.h>

#include <stdbool.h>

struct mylite_schema_default;

int mylite_table_ddl_validate_create_table_plan(mylite_db *database, const char *schema_name,
                                                struct mylite_create_table_plan *plan,
                                                bool if_not_exists,
                                                struct mylite_schema_default *schema_default,
                                                bool *out_skip_create);

#endif
