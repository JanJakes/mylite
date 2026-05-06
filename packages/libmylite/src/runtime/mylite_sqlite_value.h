#ifndef MYLITE_RUNTIME_MYLITE_SQLITE_VALUE_H
#define MYLITE_RUNTIME_MYLITE_SQLITE_VALUE_H

#include "mylite_expression.h"
#include "sqlite3.h"

#include <stddef.h>

int mylite_sqlite_copy_column_value(
    sqlite3_stmt *sqlite_stmt,
    size_t column_index,
    struct mylite_expression_value *out_value
);
int mylite_sqlite_copy_column_text_value(
    sqlite3_stmt *sqlite_stmt,
    size_t column_index,
    struct mylite_expression_value *out_value
);

#endif
