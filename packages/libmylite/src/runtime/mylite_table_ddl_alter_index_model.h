#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_INDEX_MODEL_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_INDEX_MODEL_H

#include "mylite_table_ddl_types.h"

#include <stdbool.h>
#include <stddef.h>

bool mylite_table_ddl_alter_table_index_name_exists(
    const struct mylite_alter_table_model *model,
    const char *name
);
size_t mylite_table_ddl_alter_table_index_index(
    const struct mylite_alter_table_model *model,
    const char *name
);
int mylite_table_ddl_insert_alter_table_index(
    struct mylite_alter_table_model *model,
    struct mylite_alter_table_index table_index,
    size_t position
);
int mylite_table_ddl_remove_alter_table_index(struct mylite_alter_table_model *model, size_t index);

#endif
