#include "mylite_execution_sql_normalization.h"

#include <mylite/mylite.h>

#include <stdlib.h>

int mylite_execution_normalize_mysql_compat_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    struct mylite_execution_normalized_sql *out_sql
) {
    (void)database;

    if (out_sql == NULL) {
        return MYLITE_MISUSE;
    }

    *out_sql = (struct mylite_execution_normalized_sql){
        .sql = sql,
        .sql_size = sql_size,
    };
    return MYLITE_OK;
}

void mylite_execution_normalized_sql_deinit(struct mylite_execution_normalized_sql *sql) {
    if (sql == NULL) {
        return;
    }

    free(sql->owned_sql);
    *sql = (struct mylite_execution_normalized_sql){0};
}
