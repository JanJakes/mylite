#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_INDEX_BUILD_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_INDEX_BUILD_H

#include "mylite_table_ddl_types.h"

#include <mylite/mylite.h>

#include <stdbool.h>

int mylite_table_ddl_init_alter_table_index_from_create_index(
    mylite_db *database, struct mylite_alter_table_model *model,
    const struct mylite_create_table_index *source, bool is_primary,
    struct mylite_alter_table_index *out_index);
int mylite_table_ddl_assign_alter_table_generated_index_name(
    mylite_db *database, const struct mylite_alter_table_model *model,
    const struct mylite_create_table_index *source, char **out_name);

#endif
