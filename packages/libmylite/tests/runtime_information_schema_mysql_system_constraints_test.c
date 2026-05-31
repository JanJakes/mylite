#include <mylite/mylite.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    table_constraints_column_count = 7,
    key_column_usage_column_count = 12,
    table_constraints_extensions_column_count = 6,
    mysql_system_key_column_usage_row_count = 6,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static int test_information_schema_mysql_system_constraints(void);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);

static const char *const table_constraints_columns[table_constraints_column_count] = {
    "CONSTRAINT_CATALOG",
    "CONSTRAINT_SCHEMA",
    "CONSTRAINT_NAME",
    "TABLE_SCHEMA",
    "TABLE_NAME",
    "CONSTRAINT_TYPE",
    "ENFORCED",
};

static const char *const key_column_usage_columns[key_column_usage_column_count] = {
    "CONSTRAINT_CATALOG",
    "CONSTRAINT_SCHEMA",
    "CONSTRAINT_NAME",
    "TABLE_CATALOG",
    "TABLE_SCHEMA",
    "TABLE_NAME",
    "COLUMN_NAME",
    "ORDINAL_POSITION",
    "POSITION_IN_UNIQUE_CONSTRAINT",
    "REFERENCED_TABLE_SCHEMA",
    "REFERENCED_TABLE_NAME",
    "REFERENCED_COLUMN_NAME",
};

static const char *const
    table_constraints_extensions_columns[table_constraints_extensions_column_count] = {
        "CONSTRAINT_CATALOG",
        "CONSTRAINT_SCHEMA",
        "CONSTRAINT_NAME",
        "TABLE_NAME",
        "ENGINE_ATTRIBUTE",
        "SECONDARY_ENGINE_ATTRIBUTE",
};

int main(void) {
    return test_information_schema_mysql_system_constraints() == 0 ? 0 : 1;
}

static int test_information_schema_mysql_system_constraints(void) {
    static const char *const table_constraints_values[] = {
        "def",
        "mysql",
        "PRIMARY",
        "mysql",
        "innodb_index_stats",
        "PRIMARY KEY",
        "YES",
        "def",
        "mysql",
        "PRIMARY",
        "mysql",
        "innodb_table_stats",
        "PRIMARY KEY",
        "YES",
    };
    static const char *const key_column_usage_values[] = {
        "def",           "mysql", "PRIMARY", "def", "mysql", "innodb_index_stats",
        "database_name", "1",     NULL,      NULL,  NULL,    NULL,
        "def",           "mysql", "PRIMARY", "def", "mysql", "innodb_index_stats",
        "table_name",    "2",     NULL,      NULL,  NULL,    NULL,
        "def",           "mysql", "PRIMARY", "def", "mysql", "innodb_index_stats",
        "index_name",    "3",     NULL,      NULL,  NULL,    NULL,
        "def",           "mysql", "PRIMARY", "def", "mysql", "innodb_index_stats",
        "stat_name",     "4",     NULL,      NULL,  NULL,    NULL,
        "def",           "mysql", "PRIMARY", "def", "mysql", "innodb_table_stats",
        "database_name", "1",     NULL,      NULL,  NULL,    NULL,
        "def",           "mysql", "PRIMARY", "def", "mysql", "innodb_table_stats",
        "table_name",    "2",     NULL,      NULL,  NULL,    NULL,
    };
    static const char *const table_constraints_extensions_values[] = {
        "def",
        "mysql",
        "PRIMARY",
        "innodb_index_stats",
        NULL,
        NULL,
        "def",
        "mysql",
        "PRIMARY",
        "innodb_table_stats",
        NULL,
        NULL,
    };
    static const char *const key_tail_columns[] = {"COLUMN_NAME", "ORDINAL_POSITION"};
    static const char *const key_tail_values[] = {
        "index_name",
        "3",
        "stat_name",
        "4",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_six[] = {"6"};
    static const char *const count_zero[] = {"0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "information-schema-mysql-system-constraints") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql =
                "SELECT CONSTRAINT_CATALOG, CONSTRAINT_SCHEMA, CONSTRAINT_NAME, TABLE_SCHEMA, "
                "TABLE_NAME, CONSTRAINT_TYPE, ENFORCED FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME IN ('innodb_table_stats', "
                "'innodb_index_stats')",
            .column_names = table_constraints_columns,
            .column_count = table_constraints_column_count,
            .values = table_constraints_values,
            .row_count = 2U,
            .context = "mysql system table constraints rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_CATALOG, CONSTRAINT_SCHEMA, CONSTRAINT_NAME, "
                   "TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, "
                   "POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA, "
                   "REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME FROM "
                   "INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA = 'mysql' AND "
                   "TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats')",
            .column_names = key_column_usage_columns,
            .column_count = key_column_usage_column_count,
            .values = key_column_usage_values,
            .row_count = mysql_system_key_column_usage_row_count,
            .context = "mysql system key column usage rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'innodb_index_stats' AND "
                   "ORDINAL_POSITION >= 3 ORDER BY ORDINAL_POSITION",
            .column_names = key_tail_columns,
            .column_count = sizeof(key_tail_columns) / sizeof(key_tail_columns[0]),
            .values = key_tail_values,
            .row_count = 2U,
            .context = "mysql system key column usage filtered tail",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_CATALOG, CONSTRAINT_SCHEMA, CONSTRAINT_NAME, TABLE_NAME, "
                   "ENGINE_ATTRIBUTE, SECONDARY_ENGINE_ATTRIBUTE FROM "
                   "INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS WHERE CONSTRAINT_SCHEMA = "
                   "'mysql' AND TABLE_NAME IN ('innodb_table_stats', 'innodb_index_stats')",
            .column_names = table_constraints_extensions_columns,
            .column_count = table_constraints_extensions_column_count,
            .values = table_constraints_extensions_values,
            .row_count = 2U,
            .context = "mysql system table constraints extensions rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE "
                   "TABLE_SCHEMA = 'mysql' AND TABLE_NAME IN ('innodb_table_stats', "
                   "'innodb_index_stats')",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_six,
            .row_count = 1U,
            .context = "mysql system key column usage count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE "
                   "TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'password_history'",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "unsupported mysql password_history table constraints omitted",
        }
    );
    failures += expect_row_count_status(database, "row count after mysql system constraints");

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK, got %d / %d %s %s\n",
            expected.context,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (result == NULL) {
        fprintf(stderr, "%s: expected result object\n", expected.context);
        return 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
    for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
            size_t value_index = (row_index * expected.column_count) + column_index;

            failures += expect_text_or_null(
                mylite_result_value_text(result, row_index, column_index),
                expected.values[value_index],
                expected.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_row_count_status(mylite_db *database, const char *context) {
    static const char *const status_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const status_values[] = {"0", "-1"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .column_names = status_columns,
            .column_count = sizeof(status_columns) / sizeof(status_columns[0]),
            .values = status_values,
            .row_count = 1U,
            .context = context,
        }
    );
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(path, path_size, "/tmp/mylite_%s_%d.mylite", name, current_process_id());

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path for %s\n", name);
        return 1;
    }
    return 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written > 0 && (size_t)written < sizeof(related)) {
        remove(related);
    }
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %lld, got %lld\n",
            context,
            (long long)expected,
            (long long)actual
        );
        return 1;
    }
    return 0;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected text %s, got %s\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
}
