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
    sql_buffer_capacity = 512,
    mysql_error_database_access_denied = 1044,
    mysql_error_column_ambiguous = 1052,
    mysql_error_system_schema_access = 3552,
};

struct built_in_schema_expectation {
    const char *name;
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

static int test_builtin_schema_write_access(void);
static int seed_app_schema(mylite_db *database);
static int expect_statement_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_error(
    mylite_db *database,
    const char *sql,
    int code,
    const char *sqlstate,
    const char *message_part
);
static int expect_write_rejected(
    mylite_db *database,
    const struct built_in_schema_expectation *schema,
    const char *sql
);
static int expect_formatted_write_rejected(
    mylite_db *database,
    const struct built_in_schema_expectation *schema,
    const char *format
);
static int format_sql(
    char *sql,
    size_t sql_size,
    const char *format,
    const struct built_in_schema_expectation *schema
);
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
    return test_builtin_schema_write_access() == 0 ? 0 : 1;
}

static int test_builtin_schema_write_access(void) {
    static const struct built_in_schema_expectation schemas[] = {
        {
            .name = "information_schema",
            .code = mysql_error_database_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied for user 'root'@'%' to database 'information_schema'",
        },
        {
            .name = "mysql",
            .code = mysql_error_system_schema_access,
            .sqlstate = "HY000",
            .message_part = "Access to system schema 'mysql' is rejected.",
        },
        {
            .name = "performance_schema",
            .code = mysql_error_database_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied for user 'root'@'%' to database 'performance_schema'",
        },
        {
            .name = "sys",
            .code = mysql_error_system_schema_access,
            .sqlstate = "HY000",
            .message_part = "Access to system schema 'sys' is rejected.",
        },
    };
    static const char *const schema_write_formats[] = {
        "CREATE DATABASE %s",
        "CREATE DATABASE IF NOT EXISTS %s",
        "CREATE SCHEMA %s",
        "CREATE SCHEMA IF NOT EXISTS %s",
        "ALTER DATABASE %s DEFAULT COLLATE utf8mb4_bin",
        "ALTER SCHEMA %s DEFAULT COLLATE utf8mb4_bin",
        "DROP DATABASE %s",
        "DROP DATABASE IF EXISTS %s",
        "DROP SCHEMA %s",
        "DROP SCHEMA IF EXISTS %s",
    };
    static const char *const qualified_write_formats[] = {
        "CREATE TABLE %s._mylite_denied (id INT)",
        "CREATE TEMPORARY TABLE %s._mylite_denied (id INT)",
        "CREATE TABLE IF NOT EXISTS %s._mylite_denied (id INT)",
        "CREATE TABLE %s.copy LIKE app.t",
        "CREATE TEMPORARY TABLE %s.copy LIKE app.t",
        "CREATE TABLE %s.copy AS SELECT id FROM app.t",
        "DROP TABLE %s._mylite_denied",
        "DROP TABLE IF EXISTS %s._mylite_denied",
        "DROP TEMPORARY TABLE %s._mylite_denied",
        "DROP TEMPORARY TABLE IF EXISTS %s._mylite_denied",
        "CREATE INDEX idx_builtin_write ON %s.SCHEMATA (SCHEMA_NAME)",
        "DROP INDEX idx_builtin_write ON %s.SCHEMATA",
        "ALTER TABLE %s.SCHEMATA ADD COLUMN x INT",
        "TRUNCATE TABLE %s.SCHEMATA",
        "LOCK TABLES %s.SCHEMATA READ",
        "ANALYZE TABLE %s.SCHEMATA",
        "CHECK TABLE %s.SCHEMATA",
        "OPTIMIZE TABLE %s.SCHEMATA",
        "REPAIR TABLE %s.SCHEMATA",
        "INSERT INTO %s.SCHEMATA (SCHEMA_NAME) VALUES ('x')",
        "REPLACE INTO %s.SCHEMATA (SCHEMA_NAME) VALUES ('x')",
        "UPDATE %s.SCHEMATA SET SCHEMA_NAME = 'x'",
        "DELETE FROM %s.SCHEMATA",
        "DELETE s FROM app.t, %s.SCHEMATA s WHERE s.SCHEMA_NAME = 'app'",
        "RENAME TABLE app.t TO %s._mylite_denied",
        "RENAME TABLE %s.SCHEMATA TO app.schemata_copy",
        "ALTER TABLE app.t RENAME TO %s._mylite_denied",
    };
    static const char *const selected_write_statements[] = {
        "CREATE TABLE _mylite_selected_denied (id INT)",
        "CREATE TEMPORARY TABLE _mylite_selected_denied (id INT)",
        "CREATE TABLE IF NOT EXISTS _mylite_selected_denied (id INT)",
        "CREATE TABLE selected_copy LIKE app.t",
        "CREATE TEMPORARY TABLE selected_copy LIKE app.t",
        "CREATE TABLE selected_copy AS SELECT id FROM app.t",
        "ALTER DATABASE DEFAULT COLLATE utf8mb4_bin",
        "ALTER SCHEMA DEFAULT COLLATE utf8mb4_bin",
        "DROP TABLE _mylite_selected_denied",
        "DROP TABLE IF EXISTS _mylite_selected_denied",
        "DROP TEMPORARY TABLE _mylite_selected_denied",
        "DROP TEMPORARY TABLE IF EXISTS _mylite_selected_denied",
        "CREATE INDEX idx_selected_builtin_write ON SCHEMATA (SCHEMA_NAME)",
        "DROP INDEX idx_selected_builtin_write ON SCHEMATA",
        "ALTER TABLE SCHEMATA ADD COLUMN x INT",
        "TRUNCATE TABLE SCHEMATA",
        "LOCK TABLES SCHEMATA READ",
        "ANALYZE TABLE SCHEMATA",
        "CHECK TABLE SCHEMATA",
        "OPTIMIZE TABLE SCHEMATA",
        "REPAIR TABLE SCHEMATA",
        "INSERT INTO SCHEMATA (SCHEMA_NAME) VALUES ('x')",
        "REPLACE INTO SCHEMATA (SCHEMA_NAME) VALUES ('x')",
        "UPDATE SCHEMATA SET SCHEMA_NAME = 'x'",
        "DELETE FROM SCHEMATA",
        "DELETE s FROM app.t, SCHEMATA s WHERE s.SCHEMA_NAME = 'app'",
        "RENAME TABLE SCHEMATA TO app.schemata_copy",
        "ALTER TABLE app.t RENAME TO _mylite_selected_denied",
    };
    static const char *const app_table_values[] = {"1", "10"};
    char path[test_path_capacity];
    char use_sql[sql_buffer_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path)) != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open database");
    failures += seed_app_schema(database);

    for (size_t schema_index = 0U; schema_index < sizeof(schemas) / sizeof(schemas[0]);
         ++schema_index) {
        const struct built_in_schema_expectation *schema = &schemas[schema_index];

        for (size_t format_index = 0U;
             format_index < sizeof(schema_write_formats) / sizeof(schema_write_formats[0]);
             ++format_index) {
            failures += expect_formatted_write_rejected(
                database,
                schema,
                schema_write_formats[format_index]
            );
        }
        for (size_t format_index = 0U;
             format_index < sizeof(qualified_write_formats) / sizeof(qualified_write_formats[0]);
             ++format_index) {
            failures += expect_formatted_write_rejected(
                database,
                schema,
                qualified_write_formats[format_index]
            );
        }
        failures += expect_query(
            database,
            (struct expected_query){
                .sql = "SELECT id, v FROM app.t ORDER BY id",
                .values = app_table_values,
                .column_count = 2U,
                .row_count = 1U,
                .context = "app table survives qualified built-in write rejections",
            }
        );
    }

    for (size_t schema_index = 0U; schema_index < sizeof(schemas) / sizeof(schemas[0]);
         ++schema_index) {
        const struct built_in_schema_expectation *schema = &schemas[schema_index];
        const char *const selected_schema_values[] = {schema->name};
        int written = snprintf(use_sql, sizeof(use_sql), "USE %s", schema->name);

        if (written < 0 || (size_t)written >= sizeof(use_sql)) {
            fprintf(stderr, "USE statement is too long for %s\n", schema->name);
            failures += 1;
            continue;
        }

        failures += expect_statement_ok(database, use_sql, -1);
        failures += expect_query(
            database,
            (struct expected_query){
                .sql = "SELECT DATABASE()",
                .values = selected_schema_values,
                .column_count = 1U,
                .row_count = 1U,
                .context = "selected built-in schema database function",
            }
        );
        for (size_t statement_index = 0U;
             statement_index <
             sizeof(selected_write_statements) / sizeof(selected_write_statements[0]);
             ++statement_index) {
            failures +=
                expect_write_rejected(database, schema, selected_write_statements[statement_index]);
        }
        failures += expect_statement_ok(database, "USE app", -1);
        failures += expect_query(
            database,
            (struct expected_query){
                .sql = "SELECT id, v FROM t ORDER BY id",
                .values = app_table_values,
                .column_count = 2U,
                .row_count = 1U,
                .context = "app table survives selected built-in write rejections",
            }
        );
    }

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
    failures += expect_error(
        database,
        "UPDATE information_schema.tables, information_schema.columns "
        "SET table_name = 'new_t' WHERE table_name = 't'",
        mysql_error_column_ambiguous,
        "23000",
        "Column 'table_name' in where clause is ambiguous"
    );
    failures += expect_statement_ok(database, "USE information_schema", -1);
    failures += expect_error(
        database,
        "UPDATE tables, columns SET table_name = 'new_t' WHERE table_name = 't'",
        mysql_error_column_ambiguous,
        "23000",
        "Column 'table_name' in where clause is ambiguous"
    );
    failures += expect_statement_ok(database, "USE app", -1);

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

static int expect_error(
    mylite_db *database,
    const char *sql,
    int code,
    const char *sqlstate,
    const char *message_part
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "%s: expected error, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_int(mylite_errcode(database), code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), message_part, sql);
    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_write_rejected(
    mylite_db *database,
    const struct built_in_schema_expectation *schema,
    const char *sql
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "%s: expected built-in schema write rejection, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_int(mylite_errcode(database), schema->code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), schema->sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), schema->message_part, sql);
    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_formatted_write_rejected(
    mylite_db *database,
    const struct built_in_schema_expectation *schema,
    const char *format
) {
    char sql[sql_buffer_capacity];

    if (format_sql(sql, sizeof(sql), format, schema) != 0) {
        return 1;
    }
    return expect_write_rejected(database, schema, sql);
}

static int format_sql(
    char *sql,
    size_t sql_size,
    const char *format,
    const struct built_in_schema_expectation *schema
) {
    int written = snprintf(sql, sql_size, format, schema->name);

    if (written < 0 || (size_t)written >= sql_size) {
        fprintf(stderr, "formatted SQL is too long for %s\n", schema->name);
        return 1;
    }
    return 0;
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
        "%s/mylite_builtin_schema_write_access_%d.mylite",
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
