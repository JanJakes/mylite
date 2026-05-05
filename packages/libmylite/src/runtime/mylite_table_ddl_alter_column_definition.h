#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_COLUMN_DEFINITION_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_COLUMN_DEFINITION_H

#include <mylite/mylite.h>

#include "mylite_table_ddl_types.h"

#include <stdbool.h>

struct mylite_schema_default;

int mylite_table_ddl_replace_alter_table_column_from_definition(
    mylite_db *database, const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_column *definition, const char *source_name, bool added,
    struct mylite_alter_table_column *target);
int mylite_table_ddl_init_alter_table_column_from_definition(
    mylite_db *database, const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_column *definition, const char *source_name, bool added,
    struct mylite_alter_table_column *out_column);
bool mylite_table_ddl_alter_table_column_definition_has_deferred_features(
    const struct mylite_create_table_column *column);

#endif
