#include <mylite/mylite.h>

#include "runtime/mylite_mysql_server_identity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

#ifndef P_tmpdir
#  define P_tmpdir "."
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    mixed_scalar_column_count = 5,
    show_databases_with_builtins_and_app_count = 5,
    row_count_text_capacity = 32,
    mysql_error_parse = 1064,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_scalar_result {
    const char *const *columns;
    const char *const *values;
    size_t count;
    const char *context;
};

static int test_row_count_function_transitions(void);
static int test_row_count_function_reopen_and_independent_handles(void);
static int test_row_count_function_unsupported_forms(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_non_query_result(
    const mylite_result *result,
    int64_t affected_rows,
    const char *context
);
static int expect_row_count(mylite_db *database, int64_t expected, const char *context);
static int expect_scalar_result(
    const mylite_result *result,
    struct expected_scalar_result expected
);
static int expect_single_column_rows(
    const mylite_result *result,
    const char *const *values,
    size_t row_count,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);
static int expect_true(int condition, const char *context);

int main(void) {
    int failures = 0;

    failures += test_row_count_function_transitions();
    failures += test_row_count_function_reopen_and_independent_handles();
    failures += test_row_count_function_unsupported_forms();

    return failures == 0 ? 0 : 1;
}

static int test_row_count_function_transitions(void) {
    static const char *const row_count_columns[] = {"ROW_COUNT()"};
    static const char *const row_count_initial_values[] = {"-1"};
    static const char *const label_columns[] =
        {"row_count()", "Row_Count()", "ROW_COUNT ()", "(ROW_COUNT())"};
    static const char *const label_values[] = {"-1", "-1", "-1", "-1"};
    static const char *const mixed_columns[] =
        {"ROW_COUNT()", "DATABASE()", "USER()", "CURRENT_USER", "VERSION()"};
    static const char *const mixed_values[] =
        {"0", "app", "root@%", "root@%", MYLITE_MYSQL_SERVER_VERSION_STRING};
    static const char *const dropped_columns[] = {"ROW_COUNT()", "DATABASE()"};
    static const char *const dropped_values[] = {"-1", NULL};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "transitions") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open transitions database");

    failures += execute_ok(database, "SELECT ROW_COUNT()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = row_count_columns,
            .values = row_count_initial_values,
            .count = 1U,
            .context = "initial row count",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "", &result);
    failures += expect_non_query_result(result, 0, "empty statement result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(database, 0, "empty statement row count");

    failures += execute_ok(
        database,
        "SELECT row_count(), Row_Count(), ROW_COUNT (), (ROW_COUNT())",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = label_columns,
            .values = label_values,
            .count = 4U,
            .context = "row count labels",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "CREATE DATABASE app", &result);
    failures += expect_non_query_result(result, 1, "create database result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(database, 1, "create database row count");

    failures += execute_ok(database, "USE app", &result);
    failures += expect_non_query_result(result, 0, "use database result");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "SELECT ROW_COUNT(), DATABASE(), USER(), CURRENT_USER, VERSION()",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = mixed_columns,
            .values = mixed_values,
            .count = mixed_scalar_column_count,
            .context = "mixed row count scalar functions",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "CREATE TABLE t (id INT NOT NULL, v INT NULL)", &result);
    failures += expect_non_query_result(result, 0, "create table result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(database, 0, "create table row count");

    failures += execute_ok(database, "INSERT INTO t VALUES (1, 10), (2, 20), (3, 30)", &result);
    failures += expect_non_query_result(result, 3, "insert result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(database, 3, "insert row count");

    failures += execute_ok(database, "SELECT id, v FROM t ORDER BY id", &result);
    failures += expect_size(mylite_result_row_count(result), 3U, "select table row count");
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(database, -1, "select result-set row count");
    failures += expect_row_count(database, -1, "repeated row count select row count");

    failures += execute_ok(database, "UPDATE t SET v = 10 WHERE id = 1", &result);
    failures += expect_non_query_result(result, 0, "noop update result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(database, 0, "noop update row count");

    failures += execute_ok(database, "UPDATE t SET v = 11 WHERE id = 1", &result);
    failures += expect_non_query_result(result, 1, "changed update result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(database, 1, "changed update row count");

    failures += execute_ok(database, "DELETE FROM t WHERE id = 999", &result);
    failures += expect_non_query_result(result, 0, "delete no match result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(database, 0, "delete no match row count");

    failures += execute_ok(database, "DELETE FROM t WHERE id = 2", &result);
    failures += expect_non_query_result(result, 1, "delete one result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(database, 1, "delete one row count");

    failures += execute_ok(database, "TRUNCATE TABLE t", &result);
    failures += expect_non_query_result(result, 0, "truncate table result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(database, 0, "truncate table row count");

    failures += execute_ok(database, "RENAME TABLE t TO renamed", &result);
    failures += expect_non_query_result(result, 0, "rename table result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(database, 0, "rename table row count");

    failures += execute_ok(database, "SHOW TABLES", &result);
    failures += expect_size(mylite_result_row_count(result), 1U, "show tables row count");
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(database, -1, "show tables row count state");

    failures += execute_ok(database, "DROP TABLE renamed", &result);
    failures += expect_non_query_result(result, 0, "drop table result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(database, 0, "drop table row count");

    failures += execute_ok(database, "SHOW DATABASES", &result);
    failures += expect_size(
        mylite_result_row_count(result),
        show_databases_with_builtins_and_app_count,
        "show databases row count"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(database, -1, "show databases row count state");

    failures += execute_ok(database, "DROP DATABASE app", &result);
    failures += expect_non_query_result(result, 0, "drop database result");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT ROW_COUNT(), DATABASE()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = dropped_columns,
            .values = dropped_values,
            .count = 2U,
            .context = "drop database row count and selected schema",
        }
    );
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_row_count_function_reopen_and_independent_handles(void) {
    static const char *const selected_rows[] = {"1", "2"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "reopen") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open file database");
    failures += execute_ok(first, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "CREATE TABLE t (id INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "INSERT INTO t VALUES (1), (2)", &result);
    failures += expect_non_query_result(result, 2, "file insert result");
    mylite_result_free(result);
    result = NULL;
    mylite_close(first);
    first = NULL;

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "reopen file database");
    failures += expect_row_count(first, -1, "reopened handle initial row count");
    failures += execute_ok(first, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "SELECT id FROM t ORDER BY id", &result);
    failures += expect_single_column_rows(result, selected_rows, 2U, "reopened stored rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(first, -1, "reopened select row count state");

    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open independent handle");
    failures += execute_ok(second, "CREATE DATABASE second_app", &result);
    failures += expect_non_query_result(result, 1, "second handle create result");
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(second, 1, "second handle row count");
    failures += expect_row_count(first, -1, "first handle remains independent");

    mylite_close(second);
    mylite_close(first);
    remove_related_files(first_path);
    remove_related_files(second_path);

    return failures;
}

static int test_row_count_function_unsupported_forms(void) {
    static const char *const row_count_alias_columns[] = {"rc"};
    static const char *const row_count_alias_values[] = {"-1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open unsupported database");

    failures += execute_error(
        database,
        "SELECT ROW_COUNT(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += expect_row_count(database, -1, "row count after integer argument error");

    failures += execute_error(
        database,
        "SELECT ROW_COUNT(NULL)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += expect_row_count(database, -1, "row count after null argument error");

    failures += execute_error(
        database,
        "SELECT ROW_COUNT(1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += expect_row_count(database, -1, "row count after multiple arguments error");

    failures += execute_error(
        database,
        "SELECT ROW_COUNT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT",
        }
    );
    failures += expect_row_count(database, -1, "row count after bare name error");

    failures += execute_ok(database, "SELECT ROW_COUNT() AS rc", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = row_count_alias_columns,
            .values = row_count_alias_values,
            .count = 1U,
            .context = "row count alias",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(database, -1, "row count after alias read");

    failures += execute_ok(database, "SELECT ROW_COUNT() LIMIT 1", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = (const char *const[]){"ROW_COUNT()"},
            .values = row_count_alias_values,
            .count = 1U,
            .context = "row count with limit",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(database, -1, "row count after limit read");

    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE t (id INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO t VALUES (1), (2)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "SELECT ROW_COUNT() FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only",
        }
    );
    failures += expect_row_count(database, -1, "row count after table-backed error");

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        (void)fprintf(
            stderr,
            "%s: expected success, got rc=%d err=%d state=%s message=%s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }

    *out_result = result;
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        (void)fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, sql);
    failures += expect_true(result == NULL, "error result is null");

    return failures;
}

static int expect_non_query_result(
    const mylite_result *result,
    int64_t affected_rows,
    const char *context
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), 0U, context);
    failures += expect_size(mylite_result_row_count(result), 0U, context);
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    return failures;
}

static int expect_row_count(mylite_db *database, int64_t expected, const char *context) {
    static const char *const columns[] = {"ROW_COUNT()"};
    char expected_text[row_count_text_capacity];
    const char *values[] = {expected_text};
    mylite_result *result = NULL;
    int failures = 0;
    int written = snprintf(expected_text, sizeof(expected_text), "%lld", (long long)expected);

    if (written < 0 || (size_t)written >= sizeof(expected_text)) {
        (void)fprintf(stderr, "%s: failed to format expected row count\n", context);
        return 1;
    }

    failures += execute_ok(database, "SELECT ROW_COUNT()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = columns,
            .values = values,
            .count = 1U,
            .context = context,
        }
    );
    mylite_result_free(result);

    return failures;
}

static int expect_scalar_result(
    const mylite_result *result,
    struct expected_scalar_result expected
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), expected.count, expected.context);
    failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t column = 0U; column < expected.count; ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, column),
            expected.values[column],
            expected.context
        );
    }

    return failures;
}

static int expect_single_column_rows(
    const mylite_result *result,
    const char *const *values,
    size_t row_count,
    const char *context
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), 1U, context);
    failures += expect_size(mylite_result_row_count(result), row_count, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    for (size_t row = 0U; row < row_count; ++row) {
        failures +=
            expect_text_or_null(mylite_result_value_text(result, row, 0U), values[row], context);
    }

    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "%s/mylite_row_count_function_%d_%s.mylite",
        P_tmpdir,
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        (void)fprintf(stderr, "failed to build test path for %s\n", name);
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
    char related[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    (void)remove(related);
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    (void)fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    (void)fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
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

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }

    (void)fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "NULL" : actual,
        needle == NULL ? "NULL" : needle
    );
    return 1;
}

static int expect_true(int condition, const char *context) {
    if (condition) {
        return 0;
    }

    (void)fprintf(stderr, "%s: expected condition to be true\n", context);
    return 1;
}
