#include <mylite/mylite.h>

#include <inttypes.h>
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
    path_suffix_capacity = 16,
    connection_id_text_capacity = 32,
    mysql_error_parse = 1064,
    mysql_error_incorrect_parameter_count = 1582,
    mixed_connection_id_column_count = 8,
    mixed_connection_id_value_index = 0,
    mixed_row_count_value_index = 1,
    mixed_database_value_index = 2,
    mixed_user_value_index = 3,
    mixed_current_user_value_index = 4,
    mixed_session_user_value_index = 5,
    mixed_system_user_value_index = 6,
    mixed_version_value_index = 7,
    label_connection_id_column_count = 6,
    decimal_digit_base = 10,
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

static int test_connection_id_function_values(void);
static int test_connection_id_function_independent_handles(void);
static int test_connection_id_function_unsupported_forms(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_scalar_result(
    const mylite_result *result,
    struct expected_scalar_result expected
);
static int capture_connection_id(
    const mylite_result *result,
    const char *context,
    uint64_t *out_value,
    char *out_text,
    size_t out_text_size
);
static int parse_connection_id(const char *text, uint64_t *out_value, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_uint64_not_equal(uint64_t left, uint64_t right, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);
static int expect_true(int condition, const char *context);

int main(void) {
    int failures = 0;

    failures += test_connection_id_function_values();
    failures += test_connection_id_function_independent_handles();
    failures += test_connection_id_function_unsupported_forms();

    return failures == 0 ? 0 : 1;
}

static int test_connection_id_function_values(void) {
    static const char *const connection_id_columns[] = {"CONNECTION_ID()"};
    static const char *const label_columns[] = {
        "connection_id()",
        "Connection_Id()",
        "CONNECTION_ID ()",
        "CONNECTION_ID/**/()",
        "CONNECTION_ID(/* inside */)",
        "(CONNECTION_ID())",
    };
    static const char *const mixed_columns[] = {
        "CONNECTION_ID()",
        "ROW_COUNT()",
        "DATABASE()",
        "USER()",
        "CURRENT_USER",
        "SESSION_USER()",
        "SYSTEM_USER()",
        "VERSION()",
    };
    char path[test_path_capacity];
    char connection_id_text[connection_id_text_capacity];
    char reopened_connection_id_text[connection_id_text_capacity];
    const char *label_values[label_connection_id_column_count];
    const char *mixed_values[mixed_connection_id_column_count];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    uint64_t connection_id = 0U;
    uint64_t reopened_connection_id = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");

    failures += execute_ok(database, "SELECT CONNECTION_ID()", &result);
    failures += capture_connection_id(
        result,
        "initial connection id",
        &connection_id,
        connection_id_text,
        sizeof(connection_id_text)
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = connection_id_columns,
            .values = (const char *const[]){connection_id_text},
            .count = 1U,
            .context = "connection id value",
        }
    );
    mylite_result_free(result);
    result = NULL;

    for (size_t index = 0U; index < label_connection_id_column_count; ++index) {
        label_values[index] = connection_id_text;
    }
    failures += execute_ok(
        database,
        "SELECT connection_id(), Connection_Id(), CONNECTION_ID (), CONNECTION_ID/**/(), "
        "CONNECTION_ID(/* inside */), (CONNECTION_ID()) FROM DUAL",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = label_columns,
            .values = label_values,
            .count = label_connection_id_column_count,
            .context = "connection id labels",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;

    mixed_values[mixed_connection_id_value_index] = connection_id_text;
    mixed_values[mixed_row_count_value_index] = "0";
    mixed_values[mixed_database_value_index] = "app";
    mixed_values[mixed_user_value_index] = "root@%";
    mixed_values[mixed_current_user_value_index] = "root@%";
    mixed_values[mixed_session_user_value_index] = "root@%";
    mixed_values[mixed_system_user_value_index] = "root@%";
    mixed_values[mixed_version_value_index] = MYLITE_VERSION_STRING;
    failures += execute_ok(
        database,
        "SELECT CONNECTION_ID(), ROW_COUNT(), DATABASE(), USER(), CURRENT_USER, "
        "SESSION_USER(), SYSTEM_USER(), VERSION()",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = mixed_columns,
            .values = mixed_values,
            .count = mixed_connection_id_column_count,
            .context = "mixed connection id scalar functions",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "CREATE TABLE t (id INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO t VALUES (1), (2)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT CONNECTION_ID()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = connection_id_columns,
            .values = (const char *const[]){connection_id_text},
            .count = 1U,
            .context = "connection id after ddl and dml",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "SELECT CONNECTION_ID(1)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'CONNECTION_ID'",
        }
    );
    failures += execute_ok(database, "SELECT CONNECTION_ID(), ROW_COUNT()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = (const char *const[]){"CONNECTION_ID()", "ROW_COUNT()"},
            .values = (const char *const[]){connection_id_text, "-1"},
            .count = 2U,
            .context = "connection id after failed statement",
        }
    );
    mylite_result_free(result);
    result = NULL;

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen values file");
    failures += execute_ok(database, "SELECT CONNECTION_ID()", &result);
    failures += capture_connection_id(
        result,
        "reopened connection id",
        &reopened_connection_id,
        reopened_connection_id_text,
        sizeof(reopened_connection_id_text)
    );
    failures += expect_uint64_not_equal(
        reopened_connection_id,
        connection_id,
        "reopened handle connection id"
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = connection_id_columns,
            .values = (const char *const[]){reopened_connection_id_text},
            .count = 1U,
            .context = "connection id after reopen",
        }
    );
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_connection_id_function_independent_handles(void) {
    static const char *const columns[] = {"CONNECTION_ID()"};
    char path[test_path_capacity];
    char first_text[connection_id_text_capacity];
    char second_text[connection_id_text_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    uint64_t first_id = 0U;
    uint64_t second_id = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "independent") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(path, &second), MYLITE_OK, "open second handle");

    failures += execute_ok(first, "SELECT CONNECTION_ID()", &result);
    failures += capture_connection_id(
        result,
        "first handle connection id",
        &first_id,
        first_text,
        sizeof(first_text)
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = columns,
            .values = (const char *const[]){first_text},
            .count = 1U,
            .context = "first handle connection id",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(second, "SELECT CONNECTION_ID()", &result);
    failures += capture_connection_id(
        result,
        "second handle connection id",
        &second_id,
        second_text,
        sizeof(second_text)
    );
    failures += expect_uint64_not_equal(first_id, second_id, "independent handle ids");
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = columns,
            .values = (const char *const[]){second_text},
            .count = 1U,
            .context = "second handle connection id",
        }
    );
    mylite_result_free(result);

    mylite_close(first);
    mylite_close(second);
    remove_related_files(path);

    return failures;
}

static int test_connection_id_function_unsupported_forms(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open unsupported file");
    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE t (id INT)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "SELECT CONNECTION_ID(1)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'CONNECTION_ID'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONNECTION_ID(NULL)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'CONNECTION_ID'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONNECTION_ID('x')",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'CONNECTION_ID'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONNECTION_ID(1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'CONNECTION_ID'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONNECTION_ID(1), VERSION(1)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'CONNECTION_ID'",
        }
    );
    failures += execute_error(
        database,
        "SELECT VERSION(1), CONNECTION_ID(1)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'VERSION'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONNECTION_ID",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor-backed table reads",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONNECTION_ID() LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONNECTION_ID(), 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor-backed table reads",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONNECTION_ID() FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only unqualified table columns",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
            sql,
            rc,
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
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, "error");
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
    for (size_t index = 0U; index < expected.count; ++index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, index),
            expected.columns[index],
            expected.context
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, index),
            expected.values[index],
            expected.context
        );
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    return failures;
}

static int capture_connection_id(
    const mylite_result *result,
    const char *context,
    uint64_t *out_value,
    char *out_text,
    size_t out_text_size
) {
    const char *text = mylite_result_value_text(result, 0U, 0U);
    int failures = 0;
    int written = 0;

    failures += parse_connection_id(text, out_value, context);
    written = snprintf(out_text, out_text_size, "%s", text == NULL ? "" : text);
    if (written < 0 || (size_t)written >= out_text_size) {
        fprintf(stderr, "%s: connection id text buffer too small\n", context);
        failures += 1;
    }

    return failures;
}

static int parse_connection_id(const char *text, uint64_t *out_value, const char *context) {
    uint64_t value = 0U;

    if (text == NULL || text[0] == '\0' || out_value == NULL) {
        fprintf(stderr, "%s: expected non-empty connection id text\n", context);
        return 1;
    }
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        unsigned char byte = (unsigned char)*cursor;

        if (byte < '0' || byte > '9') {
            fprintf(stderr, "%s: expected decimal connection id, got [%s]\n", context, text);
            return 1;
        }
        value = (value * (uint64_t)decimal_digit_base) + (uint64_t)(byte - '0');
    }
    if (value == 0U) {
        fprintf(stderr, "%s: expected nonzero connection id\n", context);
        return 1;
    }

    *out_value = value;
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
        "%s/mylite_connection_id_function_%d_%s.mylite",
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
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
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

static int expect_uint64_not_equal(uint64_t left, uint64_t right, const char *context) {
    if (left == right) {
        fprintf(stderr, "%s: expected different ids, got %" PRIu64 "\n", context, left);
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
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
        );
        return 1;
    }

    return 0;
}

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}
