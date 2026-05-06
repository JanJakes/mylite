#ifndef MYLITE_FORK_MYLITE_SQLITE_FORK_H
#define MYLITE_FORK_MYLITE_SQLITE_FORK_H

#include "sqlite3.h"

#ifdef __cplusplus
extern "C" {
#endif

enum mylite_sqlite_fork_column_type_kind {
    MYLITE_SQLITE_FORK_COLUMN_TYPE_NONE = 0,
    MYLITE_SQLITE_FORK_COLUMN_TYPE_SIGNED_INTEGER = 1,
    MYLITE_SQLITE_FORK_COLUMN_TYPE_UNSIGNED_INTEGER = 2,
    MYLITE_SQLITE_FORK_COLUMN_TYPE_DOUBLE = 3,
    MYLITE_SQLITE_FORK_COLUMN_TYPE_VARCHAR = 4,
};

struct mylite_sqlite_fork_column_type {
    enum mylite_sqlite_fork_column_type_kind kind;
    sqlite3_int64 integer_minimum;
    sqlite3_int64 integer_maximum;
    sqlite3_uint64 character_maximum_length;
};

int mylite_sqlite_fork_configure(sqlite3 *database);
int mylite_sqlite_fork_set_column_type(
    sqlite3 *database,
    const char *schema_name,
    const char *table_name,
    const char *column_name,
    const struct mylite_sqlite_fork_column_type *type
);
int mylite_sqlite_fork_clear_column_type(
    sqlite3 *database,
    const char *schema_name,
    const char *table_name,
    const char *column_name
);
int mylite_sqlite_fork_truncate_table(sqlite3 *database, const char *table_name);

#ifdef __cplusplus
}
#endif

#endif
