#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_WARNINGS_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_ALTER_WARNINGS_H

#include <mylite/mylite.h>

struct mylite_alter_table_model;

int mylite_table_ddl_append_alter_table_warnings(mylite_db *database,
                                                 const struct mylite_alter_table_model *model);

#endif
