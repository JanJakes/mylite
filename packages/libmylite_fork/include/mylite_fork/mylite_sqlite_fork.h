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
    MYLITE_SQLITE_FORK_COLUMN_TYPE_BINARY = 5,
    MYLITE_SQLITE_FORK_COLUMN_TYPE_VARBINARY = 6,
    MYLITE_SQLITE_FORK_COLUMN_TYPE_DECIMAL = 7,
    MYLITE_SQLITE_FORK_COLUMN_TYPE_DATE = 8,
    MYLITE_SQLITE_FORK_COLUMN_TYPE_DATETIME = 9,
    MYLITE_SQLITE_FORK_COLUMN_TYPE_TIME = 10,
    MYLITE_SQLITE_FORK_COLUMN_TYPE_TEXT = 11,
    MYLITE_SQLITE_FORK_COLUMN_TYPE_BLOB = 12,
    MYLITE_SQLITE_FORK_COLUMN_TYPE_YEAR = 13,
    MYLITE_SQLITE_FORK_COLUMN_TYPE_ENUM = 14,
    MYLITE_SQLITE_FORK_COLUMN_TYPE_SET = 15,
    MYLITE_SQLITE_FORK_COLUMN_TYPE_BIT = 16,
};

enum mylite_sqlite_fork_column_type_flags {
    MYLITE_SQLITE_FORK_COLUMN_TYPE_UNSIGNED = 1U << 0U,
    MYLITE_SQLITE_FORK_COLUMN_TYPE_ALLOW_ZERO_TEMPORAL = 1U << 1U,
};

struct mylite_sqlite_fork_column_type {
    enum mylite_sqlite_fork_column_type_kind kind;
    sqlite3_int64 integer_minimum;
    sqlite3_int64 integer_maximum;
    sqlite3_uint64 character_maximum_length;
    sqlite3_uint64 byte_maximum_length;
    sqlite3_uint64 numeric_precision;
    sqlite3_uint64 numeric_scale;
    sqlite3_uint64 datetime_precision;
    unsigned int flags;
};

struct mylite_sqlite_fork_enum_value {
    const char *text;
    sqlite3_uint64 byte_length;
};

struct mylite_sqlite_fork_enum_column_type {
    const struct mylite_sqlite_fork_enum_value *values;
    sqlite3_uint64 value_count;
    unsigned int flags;
};

struct mylite_sqlite_fork_set_column_type {
    const struct mylite_sqlite_fork_enum_value *values;
    sqlite3_uint64 value_count;
    unsigned int flags;
};

enum mylite_sqlite_fork_condition_level {
    MYLITE_SQLITE_FORK_CONDITION_NONE = 0,
    MYLITE_SQLITE_FORK_CONDITION_ERROR = 1,
    MYLITE_SQLITE_FORK_CONDITION_WARNING = 2,
};

enum {
    MYLITE_SQLITE_FORK_SQLSTATE_SIZE = 6,
};

struct mylite_sqlite_fork_condition {
    enum mylite_sqlite_fork_condition_level level;
    unsigned int mysql_errno;
    char sqlstate[MYLITE_SQLITE_FORK_SQLSTATE_SIZE];
};

int mylite_sqlite_fork_configure(sqlite3 *database);
int mylite_sqlite_fork_last_condition(
    sqlite3 *database,
    struct mylite_sqlite_fork_condition *out_condition
);
int mylite_sqlite_fork_clear_condition(sqlite3 *database);
int mylite_sqlite_fork_set_column_type(
    sqlite3 *database,
    const char *schema_name,
    const char *table_name,
    const char *column_name,
    const struct mylite_sqlite_fork_column_type *type
);
int mylite_sqlite_fork_set_enum_column_type(
    sqlite3 *database,
    const char *schema_name,
    const char *table_name,
    const char *column_name,
    const struct mylite_sqlite_fork_enum_column_type *type
);
int mylite_sqlite_fork_set_set_column_type(
    sqlite3 *database,
    const char *schema_name,
    const char *table_name,
    const char *column_name,
    const struct mylite_sqlite_fork_set_column_type *type
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
