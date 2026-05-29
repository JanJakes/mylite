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
    database_column_count = 1,
    status_column_count = 3,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_database_query {
    const char *sql;
    const char *expected_column_name;
    const char *const *expected_rows;
    size_t expected_row_count;
    const char *context;
};

struct expected_status_row {
    const char *warning_count;
    const char *error_count;
    const char *row_count;
    const char *context;
};

static int test_show_databases_where_filters(void);
static int test_show_databases_where_diagnostics(void);
static int expect_database_rows(mylite_db *database, struct expected_database_query expected);
static int expect_status_row(mylite_db *database, struct expected_status_row expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_show_databases_where_filters();
    failures += test_show_databases_where_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_show_databases_where_filters(void) {
    static const char *const mysql_row[] = {"mysql"};
    static const char *const app_row[] = {"app"};
    static const char *const user_like_rows[] = {"app", "app_other"};
    static const char *const in_rows[] = {"app", "mysql", "sys"};
    static const char *const sys_row[] = {"sys"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "filters") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open filters database");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "CREATE DATABASE app_other");

    failures += expect_database_rows(
        database,
        (struct expected_database_query){
            .sql = "SHOW DATABASES WHERE `Database` = 'mysql'",
            .expected_column_name = "Database",
            .expected_rows = mysql_row,
            .expected_row_count = sizeof(mysql_row) / sizeof(mysql_row[0]),
            .context = "show databases where equality",
        }
    );
    failures += expect_database_rows(
        database,
        (struct expected_database_query){
            .sql = "SHOW SCHEMAS WHERE `database` = 'mysql'",
            .expected_column_name = "Database",
            .expected_rows = mysql_row,
            .expected_row_count = sizeof(mysql_row) / sizeof(mysql_row[0]),
            .context = "show schemas where synonym",
        }
    );
    failures += expect_database_rows(
        database,
        (struct expected_database_query){
            .sql = "SHOW DATABASES WHERE `Database` = 'MYSQL'",
            .expected_column_name = "Database",
            .expected_rows = NULL,
            .expected_row_count = 0U,
            .context = "show databases where case-sensitive equality",
        }
    );
    failures += expect_database_rows(
        database,
        (struct expected_database_query){
            .sql = "SHOW DATABASES WHERE `Database` LIKE 'app%'",
            .expected_column_name = "Database",
            .expected_rows = user_like_rows,
            .expected_row_count = sizeof(user_like_rows) / sizeof(user_like_rows[0]),
            .context = "show databases where like",
        }
    );
    failures += expect_database_rows(
        database,
        (struct expected_database_query){
            .sql = "SHOW DATABASES WHERE `Database` LIKE 'APP%'",
            .expected_column_name = "Database",
            .expected_rows = NULL,
            .expected_row_count = 0U,
            .context = "show databases where case-sensitive like",
        }
    );
    failures += expect_database_rows(
        database,
        (struct expected_database_query){
            .sql = "SHOW DATABASES WHERE `Database` RLIKE '^s'",
            .expected_column_name = "Database",
            .expected_rows = sys_row,
            .expected_row_count = sizeof(sys_row) / sizeof(sys_row[0]),
            .context = "show databases where rlike",
        }
    );
    failures += expect_database_rows(
        database,
        (struct expected_database_query){
            .sql = "SHOW DATABASES WHERE `Database` IN ('mysql','sys','app')",
            .expected_column_name = "Database",
            .expected_rows = in_rows,
            .expected_row_count = sizeof(in_rows) / sizeof(in_rows[0]),
            .context = "show databases where in",
        }
    );
    failures += expect_database_rows(
        database,
        (struct expected_database_query){
            .sql = "SHOW DATABASES WHERE (`Database` = 'app' OR `Database` = 'app_other') "
                   "AND NOT `Database` = 'app_other'",
            .expected_column_name = "Database",
            .expected_rows = app_row,
            .expected_row_count = sizeof(app_row) / sizeof(app_row[0]),
            .context = "show databases where boolean",
        }
    );

    failures += execute_statement_ok(
        database,
        "SHOW DATABASES WHERE `Database` NOT IN (NULL, 'mysql') "
        "AND `Database` IN ('mysql','sys')"
    );
    failures += expect_status_row(
        database,
        (struct expected_status_row){
            .warning_count = "0",
            .error_count = "0",
            .row_count = "-1",
            .context = "no-match status",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_show_databases_where_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics database");

    failures += execute_error(
        database,
        "SHOW DATABASES WHERE missing = 'mysql'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SHOW DATABASES WHERE `schemas`.`Database` = 'mysql'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'schemas.Database' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SHOW DATABASES WHERE `Database` = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW DATABASES WHERE supports only string literal predicates",
        }
    );
    failures += execute_error(
        database,
        "SHOW DATABASES WHERE `Database` IN ('mysql', 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW DATABASES WHERE IN supports only string and NULL literals",
        }
    );
    failures += execute_error(
        database,
        "SHOW DATABASES WHERE Database = 'mysql'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW DATABASES LIKE 'mysql' WHERE `Database` = 'mysql'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW DATABASES WHERE `Database` = 'mysql' ORDER BY `Database`",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int expect_database_rows(mylite_db *database, struct expected_database_query expected) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, expected.sql, &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), database_column_count, expected.context);
    failures += expect_text_or_null(
        mylite_result_column_name(result, 0U),
        expected.expected_column_name,
        expected.context
    );
    failures +=
        expect_size(mylite_result_row_count(result), expected.expected_row_count, expected.context);

    for (size_t row_index = 0U;
         row_index < expected.expected_row_count && row_index < mylite_result_row_count(result);
         ++row_index) {
        failures += expect_text_or_null(
            mylite_result_value_text(result, row_index, 0U),
            expected.expected_rows[row_index],
            expected.context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int expect_status_row(mylite_db *database, struct expected_status_row expected) {
    const char *const expected_values[status_column_count] = {
        expected.warning_count,
        expected.error_count,
        expected.row_count,
    };
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, "SELECT @@warning_count, @@error_count, ROW_COUNT()", &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), status_column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
    for (size_t column_index = 0U; column_index < status_column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, column_index),
            expected_values[column_index],
            expected.context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_show_databases_where_%s_%d.mylite",
        name,
        current_process_id()
    );

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
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s/%s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
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
        fprintf(stderr, "%s: expected \"%s\", got \"%s\"\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected \"%s\" to contain \"%s\"\n",
            context,
            actual == NULL ? "NULL" : actual,
            needle == NULL ? "NULL" : needle
        );
        return 1;
    }

    return 0;
}
