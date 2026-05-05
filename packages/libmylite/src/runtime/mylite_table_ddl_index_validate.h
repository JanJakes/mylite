#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_INDEX_VALIDATE_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_INDEX_VALIDATE_H

#include "mylite_table_ddl_types.h"

#include <mylite/mylite.h>

int mylite_table_ddl_validate_create_index_plan(mylite_db *database, const char *selected_schema,
                                                struct mylite_index_ddl_plan *plan,
                                                struct mylite_alter_table_model *model);
int mylite_table_ddl_validate_drop_index_plan(mylite_db *database, const char *selected_schema,
                                              struct mylite_index_ddl_plan *plan,
                                              struct mylite_alter_table_model *model);
int mylite_table_ddl_validate_create_index_columns(mylite_db *database,
                                                   const struct mylite_alter_table_model *model,
                                                   const struct mylite_create_table_index *index);
int mylite_table_ddl_validate_create_index_supported_features(
    mylite_db *database, const struct mylite_index_ddl_plan *plan);
int mylite_table_ddl_validate_create_unique_index_values(
    mylite_db *database, const struct mylite_alter_table_model *model,
    const struct mylite_create_table_index *index);

#endif
