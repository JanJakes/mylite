#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_CATALOG_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_CATALOG_H

#include <mylite/mylite.h>

struct mylite_alter_table_model;

int mylite_table_ddl_rewrite_alter_table_catalog(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const struct mylite_alter_table_model *model
);

#endif
