#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_INDEX_CATALOG_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_INDEX_CATALOG_H

#include "mylite_table_ddl_types.h"

#include <mylite/mylite.h>

int mylite_table_ddl_create_index_catalog_transaction(mylite_db *database,
                                                      const struct mylite_alter_table_model *model,
                                                      const struct mylite_alter_table_index *index);
int mylite_table_ddl_drop_index_catalog_transaction(mylite_db *database,
                                                    const struct mylite_index_ddl_plan *plan);

#endif
