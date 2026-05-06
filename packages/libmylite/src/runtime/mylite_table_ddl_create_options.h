#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_CREATE_OPTIONS_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_CREATE_OPTIONS_H

#include "mylite_table_ddl_types.h"

#include <mylite/mylite.h>

struct mylite_schema_default;

int mylite_table_ddl_normalize_create_table_options(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_schema_default *schema_default,
    struct mylite_create_table_options *options
);

#endif
