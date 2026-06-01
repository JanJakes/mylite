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
    diagnostics_column_count = 3,
    scalar_count_column_count = 3,
    mysql_error_parse = 1064,
    mysql_error_unknown_system_variable = 1193,
    mysql_error_session_variable_only = 1238,
    mysql_error_data_too_long = 1406,
    mysql_error_bad_null = 1048,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_warning_row {
    const char *level;
    const char *code;
    const char *message_part;
};

static int test_error_codes_and_sqlstates(void);
static int test_warning_codes_count_and_order(void);
static int open_test_database(mylite_db **out_database, char *path, size_t path_size);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_show_warning_row(
    const mylite_result *result,
    size_t row_index,
    struct expected_warning_row expected,
    const char *context
);
static int expect_show_count_warnings(
    mylite_db *database,
    const char *expected,
    const char *context
);
static int expect_scalar_counts(
    mylite_db *database,
    const char *warning_count,
    const char *error_count,
    const char *row_count,
    const char *context
);
static int make_test_path(char *path, size_t path_size);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_error_codes_and_sqlstates();
    failures += test_warning_codes_count_and_order();

    return failures == 0 ? 0 : 1;
}

static int test_error_codes_and_sqlstates(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_test_database(&database, path, sizeof(path));

    failures += execute_error(
        database,
        "BAD SQL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@global.warning_count",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "SESSION variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@no_such_mylite_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_mylite_variable'",
        }
    );

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE strings (id INT NOT NULL, v VARCHAR(3), n VARCHAR(3) NOT NULL, z VARCHAR(0))"
    );
    failures += execute_error(
        database,
        "INSERT INTO strings VALUES (1, 'abcd', 'abc', '')",
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long for column 'v' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO strings VALUES (1, 'abc', NULL, '')",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'n' cannot be null",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_warning_codes_count_and_order(void) {
    static const char *const diagnostic_columns[] = {"Level", "Code", "Message"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_test_database(&database, path, sizeof(path));
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE strings (id INT NOT NULL, v VARCHAR(3), n VARCHAR(3) NOT NULL, z VARCHAR(0))"
    );
    failures += execute_ok(database, "INSERT INTO strings VALUES (9, 'abc ', 'abc ', '')", &result);
    failures += expect_size(mylite_result_warning_count(result), 2U, "insert note count");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SHOW WARNINGS", &result);
    failures +=
        expect_size(mylite_result_column_count(result), diagnostics_column_count, "columns");
    for (size_t index = 0U; index < diagnostics_column_count; ++index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, index),
            diagnostic_columns[index],
            "diagnostic column name"
        );
    }
    failures += expect_size(mylite_result_row_count(result), 2U, "show warnings row count");
    failures += expect_show_warning_row(
        result,
        0U,
        (struct expected_warning_row){
            .level = "Note",
            .code = "1265",
            .message_part = "Data truncated for column 'v' at row 1",
        },
        "first truncation note"
    );
    failures += expect_show_warning_row(
        result,
        1U,
        (struct expected_warning_row){
            .level = "Note",
            .code = "1265",
            .message_part = "Data truncated for column 'n' at row 1",
        },
        "second truncation note"
    );
    mylite_result_free(result);
    result = NULL;

    failures += expect_show_count_warnings(database, "2", "show warnings preserves count");
    failures += expect_scalar_counts(database, "2", "0", "-1", "scalar counts see warnings");
    failures += expect_show_count_warnings(database, "0", "scalar counts clear diagnostics");

    failures += execute_ok(database, "SHOW PROCESSLIST", &result);
    failures += expect_size(mylite_result_warning_count(result), 1U, "processlist warning count");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SHOW WARNINGS", &result);
    failures += expect_size(mylite_result_row_count(result), 1U, "processlist warning rows");
    failures += expect_show_warning_row(
        result,
        0U,
        (struct expected_warning_row){
            .level = "Warning",
            .code = "1287",
            .message_part = "INFORMATION_SCHEMA.PROCESSLIST",
        },
        "processlist deprecation warning"
    );
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int open_test_database(mylite_db **out_database, char *path, size_t path_size) {
    int failures = 0;

    if (make_test_path(path, path_size) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, "open diagnostics file");

    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    failures += expect_int(rc, MYLITE_OK, sql);
    failures += expect_int(mylite_errcode(database), MYLITE_OK, "public error code");
    failures += expect_text_or_null(mylite_sqlstate(database), "00000", "public SQLSTATE");

    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);

    return failures;
}

static int expect_show_warning_row(
    const mylite_result *result,
    size_t row_index,
    struct expected_warning_row expected,
    const char *context
) {
    int failures = 0;

    failures += expect_text_or_null(
        mylite_result_value_text(result, row_index, 0U),
        expected.level,
        context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, row_index, 1U),
        expected.code,
        context
    );
    failures += expect_text_contains(
        mylite_result_value_text(result, row_index, 2U),
        expected.message_part,
        context
    );

    return failures;
}

static int expect_show_count_warnings(
    mylite_db *database,
    const char *expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW COUNT(*) WARNINGS", &result);

    failures += expect_size(mylite_result_column_count(result), 1U, context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_text_or_null(mylite_result_value_text(result, 0U, 0U), expected, context);
    mylite_result_free(result);

    return failures;
}

static int expect_scalar_counts(
    mylite_db *database,
    const char *warning_count,
    const char *error_count,
    const char *row_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures =
        execute_ok(database, "SELECT @@warning_count, @@error_count, ROW_COUNT()", &result);

    failures += expect_size(mylite_result_column_count(result), scalar_count_column_count, context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures +=
        expect_text_or_null(mylite_result_value_text(result, 0U, 0U), warning_count, context);
    failures += expect_text_or_null(mylite_result_value_text(result, 0U, 1U), error_count, context);
    failures += expect_text_or_null(mylite_result_value_text(result, 0U, 2U), row_count, context);
    mylite_result_free(result);

    return failures;
}

static int make_test_path(char *path, size_t path_size) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_runtime_diagnostics_code_order_%d.mylite",
        current_process_id()
    );

    return written < 0 || (size_t)written >= path_size ? -1 : 0;
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
    char buffer[test_path_capacity + 4U];
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

    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(
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

    fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "NULL" : actual,
        needle == NULL ? "NULL" : needle
    );
    return 1;
}
