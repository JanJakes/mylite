#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_H

#include "mylite_table_ddl_types.h"
#include "sql/mylite_ast.h"

#include <mylite/mylite.h>

struct mylite_schema_default;

int mylite_table_ddl_validate_create_table_plan(mylite_db *database, const char *schema_name,
                                                struct mylite_create_table_plan *plan,
                                                bool if_not_exists,
                                                struct mylite_schema_default *schema_default,
                                                bool *out_skip_create);
int mylite_table_ddl_copy_create_table_statement(const struct mylite_sql_ast_node *statement,
                                                 struct mylite_create_table_plan *plan);
int mylite_table_ddl_copy_create_table_name(const struct mylite_sql_ast_node *table_name,
                                            struct mylite_create_table_plan *plan);
int mylite_table_ddl_copy_create_table_column(const struct mylite_sql_ast_node *column_node,
                                              struct mylite_create_table_plan *plan);
int mylite_table_ddl_copy_create_table_index(const struct mylite_sql_ast_node *index_node,
                                             struct mylite_create_table_plan *plan);
int mylite_table_ddl_copy_create_table_key_parts(const struct mylite_sql_ast_node *key_parts,
                                                 struct mylite_create_table_index *index);
int mylite_table_ddl_copy_create_table_index_options(const struct mylite_sql_ast_node *options,
                                                     struct mylite_create_table_index *index);
char *mylite_table_ddl_generated_index_name_candidate(const char *base, unsigned int suffix);
const struct mylite_create_table_column *
mylite_table_ddl_find_create_table_column(const struct mylite_create_table_plan *plan,
                                          const char *name);
void mylite_table_ddl_create_table_plan_deinit(struct mylite_create_table_plan *plan);
void mylite_table_ddl_drop_table_plan_deinit(struct mylite_drop_table_plan *plan);
void mylite_table_ddl_rename_table_plan_deinit(struct mylite_rename_table_plan *plan);
void mylite_table_ddl_truncate_table_plan_deinit(struct mylite_truncate_table_plan *plan);
void mylite_table_ddl_alter_table_plan_deinit(struct mylite_alter_table_plan *plan);
void mylite_table_ddl_alter_table_action_deinit(struct mylite_alter_table_action *action);
void mylite_table_ddl_index_ddl_plan_deinit(struct mylite_index_ddl_plan *plan);
void mylite_table_ddl_drop_table_target_deinit(struct mylite_drop_table_target *target);
void mylite_table_ddl_rename_table_target_deinit(struct mylite_rename_table_target *target);
void mylite_table_ddl_alter_table_model_deinit(struct mylite_alter_table_model *model);
void mylite_table_ddl_alter_table_column_deinit(struct mylite_alter_table_column *column);
void mylite_table_ddl_alter_table_index_deinit(struct mylite_alter_table_index *index);
void mylite_table_ddl_alter_table_index_part_deinit(struct mylite_alter_table_index_part *part);
void mylite_table_ddl_create_table_column_deinit(struct mylite_create_table_column *column);
void mylite_table_ddl_create_table_index_deinit(struct mylite_create_table_index *index);
void mylite_table_ddl_create_table_key_part_deinit(struct mylite_create_table_key_part *part);

#endif
