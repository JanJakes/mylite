#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_CREATE_CATALOG_INDEX_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_CREATE_CATALOG_INDEX_H

#include <mylite/mylite.h>

struct mylite_create_table_plan;

int mylite_table_ddl_insert_create_table_index_catalog_rows(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_create_table_plan *plan
);

#endif
