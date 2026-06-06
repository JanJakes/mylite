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
    show_create_procedure_column_count = 6,
    warning_column_count = 3,
    mysql_error_routine_exists = 1304,
    mysql_error_routine_does_not_exist = 1305,
};

struct expected_sql_error {
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

static int test_limited_stored_procedure_select_call(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_statement_warning_count(
    mylite_db *database,
    const char *sql,
    size_t expected,
    const char *context
);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    int failures = 0;

    failures += test_limited_stored_procedure_select_call();

    return failures == 0 ? 0 : 1;
}

static int test_limited_stored_procedure_select_call(void) {
    static const char *const show_create_rows[] = {
        "test_mysqli_flush_sync_procedure",
        "ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,"
        "ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION",
        "CREATE DEFINER=`root`@`%` PROCEDURE `test_mysqli_flush_sync_procedure`()\n"
        "BEGIN SELECT ID FROM posts LIMIT 1; END",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const call_rows[] = {"42"};
    static const char *const row_count_rows[] = {"0"};
    static const char *const warning_rows[] = {
        "Note",
        "1305",
        "PROCEDURE app.test_mysqli_flush_sync_procedure does not exist",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "select_call") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open procedure file");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE posts (ID INT PRIMARY KEY)");
    failures += execute_statement_ok(database, "INSERT INTO posts VALUES (42)");
    failures += execute_statement_ok(
        database,
        "CREATE PROCEDURE test_mysqli_flush_sync_procedure() "
        "BEGIN SELECT ID FROM posts LIMIT 1; END"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE PROCEDURE test_mysqli_flush_sync_procedure",
            .values = show_create_rows,
            .column_count = show_create_procedure_column_count,
            .row_count = 1U,
            .context = "show create procedure",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "CALL test_mysqli_flush_sync_procedure",
            .values = call_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "call procedure",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .values = row_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row count after call",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "CALL test_mysqli_flush_sync_procedure()",
            .values = call_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "call procedure parens",
        }
    );
    failures += execute_error(
        database,
        "CREATE PROCEDURE test_mysqli_flush_sync_procedure() BEGIN SELECT ID FROM posts LIMIT 1; "
        "END",
        (struct expected_sql_error){
            .code = mysql_error_routine_exists,
            .sqlstate = "42000",
            .message_part = "PROCEDURE test_mysqli_flush_sync_procedure already exists",
        }
    );
    failures += expect_statement_warning_count(
        database,
        "DROP PROCEDURE IF EXISTS test_mysqli_flush_sync_procedure",
        0U,
        "drop existing procedure warnings"
    );
    failures += execute_error(
        database,
        "SHOW CREATE PROCEDURE test_mysqli_flush_sync_procedure",
        (struct expected_sql_error){
            .code = mysql_error_routine_does_not_exist,
            .sqlstate = "42000",
            .message_part = "PROCEDURE test_mysqli_flush_sync_procedure does not exist",
        }
    );
    failures += execute_error(
        database,
        "CALL test_mysqli_flush_sync_procedure",
        (struct expected_sql_error){
            .code = mysql_error_routine_does_not_exist,
            .sqlstate = "42000",
            .message_part = "PROCEDURE app.test_mysqli_flush_sync_procedure does not exist",
        }
    );
    failures += expect_statement_warning_count(
        database,
        "DROP PROCEDURE IF EXISTS test_mysqli_flush_sync_procedure",
        1U,
        "drop missing procedure warnings"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = warning_rows,
            .column_count = warning_column_count,
            .row_count = 1U,
            .context = "drop missing procedure note",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return 0;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
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
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    size_t value_index = 0U;
    int failures = execute_ok(database, query.sql, &result);

    if (failures == 0) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    }
    for (size_t row = 0U; failures == 0 && row < query.row_count; ++row) {
        for (size_t column = 0U; failures == 0 && column < query.column_count; ++column) {
            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
            ++value_index;
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_statement_warning_count(
    mylite_db *database,
    const char *sql,
    size_t expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_warning_count(result), expected, context);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    return expect_text(actual, expected, context);
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if ((actual == NULL && expected != NULL) || (actual != NULL && expected == NULL) ||
        (actual != NULL && expected != NULL && strcmp(actual, expected) != 0)) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "NULL" : actual,
            needle == NULL ? "NULL" : needle
        );
        return 1;
    }
    return 0;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
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
        "%s/mylite_stored_procedure_select_call_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
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
