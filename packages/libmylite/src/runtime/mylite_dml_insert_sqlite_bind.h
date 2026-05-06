#ifndef MYLITE_RUNTIME_MYLITE_DML_INSERT_SQLITE_BIND_H
#define MYLITE_RUNTIME_MYLITE_DML_INSERT_SQLITE_BIND_H

#include "mylite_dml_types.h"

int mylite_dml_bind_insert_row_values(
    mylite_db *database,
    sqlite3_stmt *insert,
    const struct mylite_insert_bound_value *values,
    size_t value_count
);
int mylite_dml_bind_insert_bound_value(
    sqlite3_stmt *stmt,
    int index,
    const struct mylite_insert_bound_value *value
);

#endif
