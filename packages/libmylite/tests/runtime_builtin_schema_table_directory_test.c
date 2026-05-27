#include <mylite/mylite.h>

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
    test_path_suffix_capacity = 16,
    test_show_table_status_column_count = 18,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static int test_builtin_schema_table_directory(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);

int main(void) {
    return test_builtin_schema_table_directory() == 0 ? 0 : 1;
}

static int test_builtin_schema_table_directory(void) {
    static const char *const count_columns[] = {"COUNT(*)"};
    static const char *const count_78[] = {"78"};
    static const char *const count_38[] = {"38"};
    static const char *const count_114[] = {"114"};
    static const char *const count_101[] = {"101"};
    static const char *const count_331[] = {"331"};
    static const char *const representative_columns[] = {
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "VERSION",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "DATA_LENGTH",
        "TABLE_COLLATION",
        "CREATE_OPTIONS",
        "TABLE_COMMENT",
    };
    static const char *const information_schema_tables_values[] = {
        "information_schema",
        "TABLES",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
        NULL,
        "",
        "",
    };
    static const char *const mysql_user_values[] = {
        "mysql",
        "user",
        "BASE TABLE",
        "InnoDB",
        "10",
        "Dynamic",
        "5",
        "16384",
        "utf8mb3_bin",
        "row_format=DYNAMIC stats_persistent=0",
        "Users and global privileges",
    };
    static const char *const performance_setup_actors_values[] = {
        "performance_schema",
        "setup_actors",
        "BASE TABLE",
        "PERFORMANCE_SCHEMA",
        "10",
        "Fixed",
        "128",
        "0",
        "utf8mb4_0900_ai_ci",
        "",
        "",
    };
    static const char *const sys_values[] = {
        "sys",     "sys_config", "BASE TABLE",
        "InnoDB",  "10",         "Dynamic",
        "6",       "16384",      "utf8mb4_0900_ai_ci",
        "",        "",           "sys",
        "version", "VIEW",       NULL,
        NULL,      NULL,         NULL,
        NULL,      NULL,         NULL,
        "VIEW",
    };
    static const char *const show_full_sys_columns[] = {"Tables_in_sys", "Table_type"};
    static const char *const show_full_sys_values[] = {
        "sys_config",
        "BASE TABLE",
        "version",
        "VIEW",
    };
    static const char *const show_performance_columns[] = {
        "Tables_in_performance_schema (setup_actors)",
    };
    static const char *const show_performance_values[] = {"setup_actors"};
    static const char *const show_user_columns[] = {"Tables_in_app", "Table_type"};
    static const char *const show_user_values[] = {"t", "BASE TABLE"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "builtin-schema-table-directory") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE t (id INT)");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema'",
            .column_names = count_columns,
            .column_count = sizeof(count_columns) / sizeof(count_columns[0]),
            .values = count_78,
            .row_count = 1U,
            .context = "information schema directory count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'mysql'",
            .column_names = count_columns,
            .column_count = sizeof(count_columns) / sizeof(count_columns[0]),
            .values = count_38,
            .row_count = 1U,
            .context = "mysql directory count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'performance_schema'",
            .column_names = count_columns,
            .column_count = sizeof(count_columns) / sizeof(count_columns[0]),
            .values = count_114,
            .row_count = 1U,
            .context = "performance schema directory count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'sys'",
            .column_names = count_columns,
            .column_count = sizeof(count_columns) / sizeof(count_columns[0]),
            .values = count_101,
            .row_count = 1U,
            .context = "sys directory count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql =
                "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                "WHERE TABLE_SCHEMA IN ('information_schema','mysql','performance_schema','sys') "
                "AND CREATE_TIME IS NOT NULL",
            .column_names = count_columns,
            .column_count = sizeof(count_columns) / sizeof(count_columns[0]),
            .values = count_331,
            .row_count = 1U,
            .context = "built-in directory create time count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, TABLE_COLLATION, CREATE_OPTIONS, TABLE_COMMENT "
                   "FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'TABLES'",
            .column_names = representative_columns,
            .column_count = sizeof(representative_columns) / sizeof(representative_columns[0]),
            .values = information_schema_tables_values,
            .row_count = 1U,
            .context = "information schema tables row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, TABLE_COLLATION, CREATE_OPTIONS, TABLE_COMMENT "
                   "FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'mysql' "
                   "AND TABLE_NAME = 'user'",
            .column_names = representative_columns,
            .column_count = sizeof(representative_columns) / sizeof(representative_columns[0]),
            .values = mysql_user_values,
            .row_count = 1U,
            .context = "mysql user row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, TABLE_COLLATION, CREATE_OPTIONS, TABLE_COMMENT "
                   "FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'setup_actors'",
            .column_names = representative_columns,
            .column_count = sizeof(representative_columns) / sizeof(representative_columns[0]),
            .values = performance_setup_actors_values,
            .row_count = 1U,
            .context = "performance schema setup actors row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, TABLE_COLLATION, CREATE_OPTIONS, TABLE_COMMENT "
                   "FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('sys_config','version') ORDER BY TABLE_NAME",
            .column_names = representative_columns,
            .column_count = sizeof(representative_columns) / sizeof(representative_columns[0]),
            .values = sys_values,
            .row_count = 2U,
            .context = "sys representative rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL TABLES FROM sys "
                   "WHERE Tables_in_sys IN ('sys_config', 'version')",
            .column_names = show_full_sys_columns,
            .column_count = sizeof(show_full_sys_columns) / sizeof(show_full_sys_columns[0]),
            .values = show_full_sys_values,
            .row_count = 2U,
            .context = "show full tables sys directory",
        }
    );
    failures += expect_statement_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLES LIKE 'setup_actors'",
            .column_names = show_performance_columns,
            .column_count = sizeof(show_performance_columns) / sizeof(show_performance_columns[0]),
            .values = show_performance_values,
            .row_count = 1U,
            .context = "selected performance schema show tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM information_schema "
                   "WHERE Name = 'TABLES' AND Create_time IS NOT NULL",
            .column_names = NULL,
            .column_count = test_show_table_status_column_count,
            .values = NULL,
            .row_count = 1U,
            .context = "show table status information schema timestamp",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM sys WHERE Comment = 'VIEW' AND Name = 'version'",
            .column_names = NULL,
            .column_count = test_show_table_status_column_count,
            .values = NULL,
            .row_count = 1U,
            .context = "show table status sys view",
        }
    );
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL TABLES",
            .column_names = show_user_columns,
            .column_count = sizeof(show_user_columns) / sizeof(show_user_columns[0]),
            .values = show_user_values,
            .row_count = 1U,
            .context = "user schema show tables remains descriptor driven",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = expect_int(rc, MYLITE_OK, sql);

    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
    } else {
        (void)fprintf(
            stderr,
            "%s: expected success, got %d/%s/%s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    if (rc != MYLITE_OK) {
        (void)fprintf(
            stderr,
            "%s: expected success, got %d/%s/%s\n",
            expected.context,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    if (expected.column_names != NULL) {
        for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
            failures += expect_text_or_null(
                mylite_result_column_name(result, column_index),
                expected.column_names[column_index],
                expected.context
            );
        }
    }
    if (expected.values != NULL) {
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
    }

    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(path, path_size, "/tmp/mylite-%s-%d.mylite", name, current_process_id());

    if (written < 0 || (size_t)written >= path_size) {
        (void)fprintf(stderr, "failed to format test path\n");
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
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-shm");
    remove_with_suffix(path, "-wal");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return;
    }
    (void)remove(buffer);
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    (void)fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    (void)fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    (void)fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}
