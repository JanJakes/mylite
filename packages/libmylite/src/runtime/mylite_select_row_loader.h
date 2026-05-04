#ifndef MYLITE_RUNTIME_MYLITE_SELECT_ROW_LOADER_H
#define MYLITE_RUNTIME_MYLITE_SELECT_ROW_LOADER_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

#include <stddef.h>

int mylite_select_load_join_rowsets(mylite_stmt *stmt,
                                    struct mylite_table_select_table_rowset *rowsets);
int mylite_select_copy_sqlite_row(mylite_stmt *stmt, struct mylite_table_select_row *out_row);
int mylite_select_copy_current_sqlite_column_value(mylite_stmt *stmt, size_t column_index,
                                                   struct mylite_expression_value *out_value);

#endif
