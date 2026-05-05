#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_INDEX_WARNINGS_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_INDEX_WARNINGS_H

#include "mylite_table_ddl_types.h"

#include <mylite/mylite.h>

int mylite_table_ddl_append_create_index_warnings(mylite_db *database,
                                                  const struct mylite_alter_table_model *model,
                                                  const struct mylite_create_table_index *index);

#endif
