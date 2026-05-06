#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_MODEL_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_MODEL_H

#include "mylite_table_ddl_types.h"

#include <mylite/mylite.h>

#include <stdbool.h>

int mylite_table_ddl_load_alter_table_model(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    bool temporary,
    struct mylite_alter_table_model *model
);
int mylite_table_ddl_add_alter_table_column(
    struct mylite_alter_table_model *model,
    struct mylite_alter_table_column column
);
int mylite_table_ddl_append_alter_table_index_part(
    struct mylite_alter_table_index *index,
    struct mylite_alter_table_index_part part
);

#endif
