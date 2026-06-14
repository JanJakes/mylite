#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SQL_NORMALIZATION_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SQL_NORMALIZATION_H

#include <stddef.h>

struct mylite_db;

struct mylite_execution_normalized_sql {
    const char *sql;
    size_t sql_size;
    char *owned_sql;
};

int mylite_execution_normalize_mysql_compat_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    struct mylite_execution_normalized_sql *out_sql
);
void mylite_execution_normalized_sql_deinit(struct mylite_execution_normalized_sql *sql);

#endif
