#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_H

#include "mylite_table_ddl_types.h"

#include <mylite/mylite.h>

#include <stddef.h>

struct mylite_schema_default;

struct mylite_table_ddl_alter_callbacks {
    void *user_data;
    int (*validate_primary_key_part_not_null)(void *user_data,
                                              const struct mylite_alter_table_model *model,
                                              const struct mylite_create_table_key_part *part);
};

int mylite_table_ddl_apply_alter_table_column_action(
    mylite_db *database, const struct mylite_schema_default *schema_default,
    const struct mylite_alter_table_action *action, struct mylite_alter_table_model *model);
int mylite_table_ddl_refresh_alter_table_index_metadata(mylite_db *database,
                                                        struct mylite_alter_table_model *model);
int mylite_table_ddl_validate_alter_table_final_model(mylite_db *database,
                                                      struct mylite_alter_table_model *model);
const struct mylite_alter_table_column *
mylite_table_ddl_find_alter_table_column(const struct mylite_alter_table_model *model,
                                         const char *name);
size_t mylite_table_ddl_alter_table_column_index(const struct mylite_alter_table_model *model,
                                                 const char *name);
int mylite_table_ddl_set_alter_table_duplicate_column_error(mylite_db *database,
                                                            const char *column_name);
int mylite_table_ddl_set_alter_table_unknown_column_error(mylite_db *database,
                                                          const char *table_name,
                                                          const char *column_name);
int mylite_table_ddl_set_alter_table_cant_drop_column_error(mylite_db *database,
                                                            const char *column_name);
int mylite_table_ddl_set_alter_table_cant_remove_all_columns_error(mylite_db *database);
int mylite_table_ddl_set_alter_table_wrong_auto_increment_error(mylite_db *database);

#endif
