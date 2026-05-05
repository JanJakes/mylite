#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_INDEX_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_INDEX_H

#include "mylite_table_ddl_alter.h"

int mylite_table_ddl_apply_alter_table_index_action(
    mylite_db *database, const struct mylite_alter_table_action *action,
    struct mylite_alter_table_model *model,
    const struct mylite_table_ddl_alter_callbacks *callbacks);

#endif
