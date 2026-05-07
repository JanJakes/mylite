#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_REBUILD_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_REBUILD_H

#include <mylite/mylite.h>

struct mylite_alter_table_model;

int mylite_table_ddl_execute_alter_table_rebuild(
    mylite_stmt *stmt,
    struct mylite_alter_table_model *model
);
int mylite_table_ddl_execute_alter_table_rebuild_in_atomicity(
    mylite_stmt *stmt,
    struct mylite_alter_table_model *model
);

#endif
