#ifndef MYLITE_RUNTIME_MYLITE_SHOW_CREATE_TABLE_TARGET_H
#define MYLITE_RUNTIME_MYLITE_SHOW_CREATE_TABLE_TARGET_H

#include <mylite/mylite.h>

struct mylite_sql_ast_node;

struct mylite_show_create_table_target {
    char *schema_name;
    char *table_name;
};

int mylite_show_create_table_copy_target(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         struct mylite_show_create_table_target *out_target);
int mylite_show_create_table_validate_target(mylite_db *database,
                                             const struct mylite_show_create_table_target *target);
void mylite_show_create_table_target_deinit(struct mylite_show_create_table_target *target);

#endif
