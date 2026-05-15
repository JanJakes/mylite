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
    mysql_error_database_access_denied = 1044,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
};

struct expected_sql_error {
    const char *sql;
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_information_schema_write_access(void);
static int seed_app_schema(mylite_db *database);
static int expect_statement_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_error(mylite_db *database, struct expected_sql_error expected);
static int expect_access_denied(mylite_db *database, const char *sql);
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
    return test_information_schema_write_access() == 0 ? 0 : 1;
}

static int test_information_schema_write_access(void) {
    static const char *const database_value[] = {"information_schema"};
    static const char *const information_schema_schemata[] = {"information_schema"};
    static const char *const app_table_values[] = {"1", "10"};
    static const char *const access_denied_statements[] = {
        "CREATE DATABASE information_schema",
        "CREATE DATABASE IF NOT EXISTS information_schema",
        "DROP DATABASE information_schema",
        "DROP DATABASE IF EXISTS information_schema",
        "CREATE TABLE information_schema.t (id INT)",
        "CREATE TEMPORARY TABLE information_schema.t (id INT)",
        "CREATE TABLE IF NOT EXISTS information_schema.t (id INT)",
        "CREATE TABLE information_schema.copy LIKE app.t",
        "CREATE TABLE information_schema.copy AS SELECT id FROM app.t",
        "DROP TABLE information_schema.t",
        "DROP TABLE IF EXISTS information_schema.t",
        "CREATE INDEX idx_info ON information_schema.TABLES (TABLE_NAME)",
        "DROP INDEX idx_info ON information_schema.TABLES",
        "ALTER TABLE information_schema.TABLES ADD COLUMN x INT",
        "TRUNCATE TABLE information_schema.TABLES",
        "INSERT INTO information_schema.SCHEMATA (SCHEMA_NAME) VALUES ('x')",
        "REPLACE INTO information_schema.SCHEMATA (SCHEMA_NAME) VALUES ('x')",
        "UPDATE information_schema.SCHEMATA SET SCHEMA_NAME = 'x'",
        "DELETE FROM information_schema.SCHEMATA",
        "RENAME TABLE app.t TO information_schema.t",
        "RENAME TABLE information_schema.TABLES TO app.tables_copy",
        "ALTER TABLE app.t RENAME TO information_schema.t",
    };
    static const char *const selected_schema_denied_statements[] = {
        "CREATE TABLE t (id INT)",
        "CREATE TEMPORARY TABLE t (id INT)",
        "CREATE TABLE t LIKE app.t",
        "CREATE TABLE t AS SELECT id FROM app.t",
        "DROP TABLE t",
        "DROP TABLE IF EXISTS t",
        "CREATE INDEX idx_selected ON SCHEMATA (SCHEMA_NAME)",
        "DROP INDEX idx_selected ON SCHEMATA",
        "ALTER TABLE SCHEMATA ADD COLUMN x INT",
        "TRUNCATE TABLE SCHEMATA",
        "INSERT INTO SCHEMATA (SCHEMA_NAME) VALUES ('x')",
        "REPLACE INTO SCHEMATA (SCHEMA_NAME) VALUES ('x')",
        "UPDATE SCHEMATA SET SCHEMA_NAME = 'x'",
        "DELETE FROM SCHEMATA",
        "RENAME TABLE SCHEMATA TO app.schemata_copy",
        "ALTER TABLE app.t RENAME TO t",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path)) != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open database");
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "CREATE TABLE t (id INT)",
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "CREATE TABLE missing.t (id INT)",
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing'",
        }
    );
    failures += seed_app_schema(database);

    for (size_t index = 0U;
         index < sizeof(access_denied_statements) / sizeof(access_denied_statements[0]);
         ++index) {
        failures += expect_access_denied(database, access_denied_statements[index]);
    }
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM app.t ORDER BY id",
            .values = app_table_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "app table survives denied information_schema writes",
        }
    );

    failures += expect_statement_ok(database, "USE information_schema", -1);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATABASE()",
            .values = database_value,
            .column_count = 1U,
            .row_count = 1U,
            .context = "selected information_schema database function",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SCHEMA_NAME FROM SCHEMATA WHERE SCHEMA_NAME = 'information_schema'",
            .values = information_schema_schemata,
            .column_count = 1U,
            .row_count = 1U,
            .context = "selected information_schema unqualified read",
        }
    );

    for (size_t index = 0U; index < sizeof(selected_schema_denied_statements) /
                                        sizeof(selected_schema_denied_statements[0]);
         ++index) {
        failures += expect_access_denied(database, selected_schema_denied_statements[index]);
    }

    failures += expect_statement_ok(database, "USE app", -1);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = app_table_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "selected app table survives denied writes",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int seed_app_schema(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(database, "CREATE DATABASE app", 1);
    failures += expect_statement_ok(database, "USE app", -1);
    failures += expect_statement_ok(database, "CREATE TABLE t (id INT, v INT)", 0);
    failures += expect_statement_ok(database, "INSERT INTO t VALUES (1, 10)", 1);

    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
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
    if (affected_rows >= 0) {
        failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
    }
    failures += expect_size(mylite_result_warning_count(result), 0U, sql);
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

    mylite_result_free(result);
    return failures;
}

static int expect_access_denied(mylite_db *database, const char *sql) {
    return expect_error(
        database,
        (struct expected_sql_error){
            .sql = sql,
            .code = mysql_error_database_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied for user 'root'@'%' to database 'information_schema'",
        }
    );
}

static int expect_error(mylite_db *database, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "%s: expected error, got %d\n", expected.sql, rc);
        failures += 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, expected.sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, expected.sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, expected.sql);
    failures += expect_size(mylite_result_column_count(result), 0U, expected.sql);
    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }
    written = snprintf(
        path,
        path_size,
        "%s/mylite_information_schema_write_access_%d.mylite",
        directory,
        current_process_id()
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long\n");
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
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
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
            fprintf(stderr, "%s: expected %s, got %s\n", context, expected, actual);
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected '%s', got '%s'\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(stderr, "%s: expected '%s' to contain '%s'\n", context, actual, needle);
        return 1;
    }
    return 0;
}
