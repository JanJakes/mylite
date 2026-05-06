#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_INDEX_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_INDEX_H

#include "mylite_table_ddl_alter.h"

int mylite_table_ddl_apply_alter_table_index_action(
    mylite_db *database,
    const struct mylite_alter_table_action *action,
    struct mylite_alter_table_model *model,
    const struct mylite_table_ddl_alter_callbacks *callbacks
);
int mylite_table_ddl_validate_alter_table_added_index(
    mylite_db *database,
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_index *index,
    const char *index_name,
    bool is_primary
);
int mylite_table_ddl_validate_alter_table_primary_key_values(
    const struct mylite_table_ddl_alter_callbacks *callbacks,
    const struct mylite_alter_table_model *model,
    const struct mylite_create_table_index *index
);
int mylite_table_ddl_apply_alter_table_primary_key_column_nullability(
    mylite_db *database,
    struct mylite_alter_table_model *model,
    const struct mylite_create_table_index *index
);
int mylite_table_ddl_set_alter_table_duplicate_key_name_error(
    mylite_db *database,
    const char *index_name
);
int mylite_table_ddl_set_alter_table_primary_invisible_error(mylite_db *database);

#endif
