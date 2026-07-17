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
    mysql_error_unknown_column = 1054,
    test_path_capacity = 1024,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

struct expected_sql_error {
    const char *sql;
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_information_schema_tablespaces_extensions_queries(void);
static int seed_database(mylite_db *database);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_error(mylite_db *database, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_information_schema_tablespaces_extensions_queries() == 0 ? 0 : 1;
}

static int test_information_schema_tablespaces_extensions_queries(void) {
    static const char *const tablespace_columns[] = {"TABLESPACE_NAME", "ENGINE_ATTRIBUTE"};
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const table_name_column[] = {"TABLESPACE_NAME"};
    static const char *const system_table_columns[] = {
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "VERSION",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "DATA_LENGTH",
    };
    static const char *const system_table_values[] = {
        "TABLESPACES_EXTENSIONS",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
    };
    static const char *const columns_metadata_columns[] = {
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "IS_NULLABLE",
        "DATA_TYPE",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
        "COLUMN_TYPE",
    };
    static const char *const columns_metadata_values[] = {
        "TABLESPACE_NAME",
        "1",
        "NO",
        "varchar",
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(268)",
        "ENGINE_ATTRIBUTE",
        "2",
        "YES",
        "json",
        NULL,
        NULL,
        "json",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path)) != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open database");
    failures += seed_database(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLESPACE_NAME, ENGINE_ATTRIBUTE "
                   "FROM INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS "
                   "WHERE TABLESPACE_NAME IN ('innodb_system', 'innodb_temporary', "
                   "'innodb_undo_001', 'innodb_undo_002', 'mysql', 'sys/sys_config') "
                   "ORDER BY TABLESPACE_NAME",
            .column_names = tablespace_columns,
            .column_count = sizeof(tablespace_columns) / sizeof(tablespace_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .context = "tablespaces extensions has no fabricated baseline rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLESPACE_NAME, ENGINE_ATTRIBUTE "
                   "FROM INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS "
                   "WHERE TABLESPACE_NAME IN ('app/parent', 'app/child', 'app/v_parent') "
                   "ORDER BY TABLESPACE_NAME",
            .column_names = tablespace_columns,
            .column_count = sizeof(tablespace_columns) / sizeof(tablespace_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .context = "tablespaces extensions does not model user tablespaces",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS "
                   "WHERE TABLESPACE_NAME IN ('app/parent', 'app/child')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "tablespaces extensions base table count remains zero",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS "
                   "WHERE TABLESPACE_NAME IN ('app/v_parent', 'app/tmp')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "tablespaces extensions omit views and temp tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLESPACE_NAME FROM INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS "
                   "WHERE TABLESPACE_NAME = 'app/parent' LIMIT 1",
            .column_names = table_name_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "tablespaces extensions limit remains empty",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "DATA_LENGTH FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'TABLESPACES_EXTENSIONS'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = sizeof(system_table_values) / sizeof(system_table_values[0]) /
                         (sizeof(system_table_columns) / sizeof(system_table_columns[0])),
            .context = "tablespaces extensions system table metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, DATA_TYPE, "
                   "CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'TABLESPACES_EXTENSIONS' ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .context = "tablespaces extensions columns metadata",
        }
    );
    failures += expect_statement_ok(database, "USE information_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLESPACE_NAME FROM TABLESPACES_EXTENSIONS "
                   "WHERE TABLESPACE_NAME = 'app/parent'",
            .column_names = table_name_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "selected information_schema tablespaces extensions remains empty",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT TABLESPACE_NAME FROM INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS "
                   "WHERE nope = 'x'",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'where clause'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int seed_database(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE parent (id INT PRIMARY KEY)");
    failures += expect_statement_ok(database, "CREATE TABLE child (id INT PRIMARY KEY)");
    failures += expect_statement_ok(database, "CREATE VIEW v_parent AS SELECT id FROM parent");
    failures += expect_statement_ok(database, "CREATE TEMPORARY TABLE tmp (id INT)");

    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected query OK, got %d / %d %s %s\n",
            expected.context,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            expected.column_names[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_text_or_null(
                mylite_result_value_text(result, row, column),
                expected.values[value_index],
                expected.context
            );
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    mylite_result_free(result);
    return failures;
}

static int expect_error(mylite_db *database, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got OK\n", expected.sql);
        mylite_result_free(result);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, expected.sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, expected.sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, expected.sql);
    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size) {
    int written = snprintf(
        path,
        path_size,
        "runtime_information_schema_tablespaces_extensions_%d.mylite",
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path\n");
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
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    remove(related);
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
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected == NULL ? "NULL" : expected,
                actual == NULL ? "NULL" : actual
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected [%s], got [%s]\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected message containing [%s], got [%s]\n",
            context,
            needle,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}
