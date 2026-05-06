#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_CATALOG_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_CATALOG_H

#include <mylite/mylite.h>

struct mylite_create_table_plan;
struct mylite_schema_default;

int mylite_table_ddl_insert_create_table_catalog_rows(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_plan *plan
);

#endif
