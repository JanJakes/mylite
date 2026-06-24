#include <mylite/mylite.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <process.h>
#  include <windows.h>
#else
#  include <time.h>
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    connection_id_text_capacity = 32,
    timeout_wait_minimum_milliseconds = 750,
    milliseconds_per_second = 1000,
    nanoseconds_per_millisecond = 1000000,
    mysql_error_native_function_parameter_count = 1582,
    mysql_error_user_lock_wrong_name = 3057,
    mysql_error_user_lock_name_too_long = 4163,
};

struct expected_result {
    const char *const *values;
    size_t column_count;
    size_t row_count;
    size_t warning_count;
    const char *context;
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_warning {
    const char *code;
    const char *message_part;
};

static int test_info_and_benchmark_functions(void);
static int test_same_session_named_locks(void);
static int test_cross_connection_named_locks(void);
static int test_named_locks_in_row_and_if_contexts(void);
static int test_named_lock_diagnostics(void);
static int expect_query_result(
    mylite_db *database,
    const char *sql,
    struct expected_result expected
);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int capture_connection_id(
    mylite_db *database,
    char *out_text,
    size_t out_text_size,
    const char *context
);
static int expect_result_values(const mylite_result *result, struct expected_result expected);
static int expect_warning_rows(
    mylite_db *database,
    size_t row_count,
    struct expected_warning expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static int64_t monotonic_milliseconds(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_int64_at_least(int64_t actual, int64_t minimum, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);
static int expect_true(int condition, const char *context);

int main(void) {
    int failures = 0;

    failures += test_info_and_benchmark_functions();
    failures += test_same_session_named_locks();
    failures += test_cross_connection_named_locks();
    failures += test_named_locks_in_row_and_if_contexts();
    failures += test_named_lock_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_info_and_benchmark_functions(void) {
    static const char *const info_values[] = {"77.1", "0", "0", NULL};
    static const char *const negative_benchmark_values[] = {NULL};
    static const char *const zero_benchmark_side_effect_values[] = {"0", NULL};
    static const char *const repeat_benchmark_side_effect_values[] = {"0", "0", "3"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open info memory db");
    failures += expect_query_result(
        database,
        "SELECT ICU_VERSION(), BENCHMARK(0, 1 + 1), BENCHMARK(3, 1 + 1), "
        "BENCHMARK(NULL, 1)",
        (struct expected_result){
            .values = info_values,
            .column_count = 4U,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "ICU_VERSION and BENCHMARK values",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT BENCHMARK(-1, 1)",
        (struct expected_result){
            .values = negative_benchmark_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "negative BENCHMARK result",
        }
    );
    failures += expect_warning_rows(
        database,
        1U,
        (struct expected_warning){
            .code = "1411",
            .message_part = "Incorrect count value",
        },
        "negative BENCHMARK warning"
    );
    failures += expect_query_result(
        database,
        "SELECT BENCHMARK(0, GET_LOCK('mylite_benchmark_zero_side_effect', 0)), "
        "IS_USED_LOCK('mylite_benchmark_zero_side_effect')",
        (struct expected_result){
            .values = zero_benchmark_side_effect_values,
            .column_count = 2U,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "zero-count BENCHMARK skips expression side effects",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT RELEASE_ALL_LOCKS(), "
        "BENCHMARK(3, GET_LOCK('mylite_benchmark_repeat_side_effect', 0)), "
        "RELEASE_ALL_LOCKS()",
        (struct expected_result){
            .values = repeat_benchmark_side_effect_values,
            .column_count = 3U,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "positive-count BENCHMARK repeats expression side effects",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_same_session_named_locks(void) {
    enum {
        recursive_connection_id_column = 3,
        recursive_after_first_release_connection_id_column = 6,
        recursive_column_count = 10,
    };

    static const char *const recursive_release_values[] = {
        "1",
        "1",
        "0",
        NULL,
        "1",
        "0",
        NULL,
        "1",
        NULL,
        "0",
    };
    static const char *const release_all_values[] = {"1", "1", "1", "3"};
    char connection_id[connection_id_text_capacity];
    const char
        *recursive_values[sizeof(recursive_release_values) / sizeof(recursive_release_values[0])];
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open same-session db");
    failures += capture_connection_id(
        database,
        connection_id,
        sizeof(connection_id),
        "same-session connection id"
    );
    for (size_t index = 0U; index < sizeof(recursive_values) / sizeof(recursive_values[0]);
         ++index) {
        recursive_values[index] = recursive_release_values[index];
    }
    recursive_values[recursive_connection_id_column] = connection_id;
    recursive_values[recursive_after_first_release_connection_id_column] = connection_id;

    failures += expect_query_result(
        database,
        "SELECT GET_LOCK('mylite_same', 0), GET_LOCK('mylite_same', 0), "
        "IS_FREE_LOCK('mylite_same'), IS_USED_LOCK('mylite_same'), "
        "RELEASE_LOCK('mylite_same'), IS_FREE_LOCK('mylite_same'), "
        "IS_USED_LOCK('mylite_same'), RELEASE_LOCK('mylite_same'), "
        "RELEASE_LOCK('mylite_same'), RELEASE_ALL_LOCKS()",
        (struct expected_result){
            .values = recursive_values,
            .column_count = recursive_column_count,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "same-session recursive named lock lifecycle",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT GET_LOCK('mylite_a', 0), GET_LOCK('mylite_a', 0), "
        "GET_LOCK('mylite_b', 0), RELEASE_ALL_LOCKS()",
        (struct expected_result){
            .values = release_all_values,
            .column_count = 4U,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "RELEASE_ALL_LOCKS recursive count",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_cross_connection_named_locks(void) {
    static const char *const first_lock_values[] = {"1"};
    static const char *const second_owner_template[] = {NULL, "0"};
    static const char *const second_timeout_values[] = {"0"};
    static const char *const second_get_values[] = {"1", "1"};
    char first_connection_id[connection_id_text_capacity];
    const char *second_owner_values[2];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int64_t timeout_started = 0;
    int64_t timeout_elapsed = 0;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first db");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second db");
    failures += capture_connection_id(
        first,
        first_connection_id,
        sizeof(first_connection_id),
        "first connection id"
    );
    failures += expect_query_result(
        first,
        "SELECT GET_LOCK('mylite_cross', 0)",
        (struct expected_result){
            .values = first_lock_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "first connection acquires cross lock",
        }
    );

    second_owner_values[0] = first_connection_id;
    second_owner_values[1] = second_owner_template[1];
    failures += expect_query_result(
        second,
        "SELECT IS_USED_LOCK('mylite_cross'), GET_LOCK('mylite_cross', 0)",
        (struct expected_result){
            .values = second_owner_values,
            .column_count = 2U,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "second connection sees cross lock owner",
        }
    );
    timeout_started = monotonic_milliseconds();
    failures += expect_query_result(
        second,
        "SELECT GET_LOCK('mylite_cross', 1)",
        (struct expected_result){
            .values = second_timeout_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "second connection waits for contended named lock timeout",
        }
    );
    timeout_elapsed = monotonic_milliseconds() - timeout_started;
    failures += expect_int64_at_least(
        timeout_elapsed,
        timeout_wait_minimum_milliseconds,
        "contended named lock timeout waits before returning"
    );

    mylite_close(first);
    first = NULL;
    failures += expect_query_result(
        second,
        "SELECT GET_LOCK('mylite_cross', 0), RELEASE_ALL_LOCKS()",
        (struct expected_result){
            .values = second_get_values,
            .column_count = 2U,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "close releases cross lock",
        }
    );

    mylite_close(second);
    return failures;
}

static int test_named_locks_in_row_and_if_contexts(void) {
    enum {
        first_row_connection_id_index = 2,
        second_row_connection_id_index = 6,
    };

    static const char *const if_values[] = {"10"};
    static const char *const if_release_values[] = {"1"};
    static const char *const benchmark_values[] = {"1", "0", "2", "0"};
    char path[test_path_capacity];
    char connection_id[connection_id_text_capacity];
    const char *row_values[] = {"1", "1", NULL, "1", "2", "1", NULL, "1"};
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "row_context") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open row-context db");
    failures += capture_connection_id(
        database,
        connection_id,
        sizeof(connection_id),
        "row-context connection id"
    );
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE lock_rows (id INT NOT NULL)");
    failures += execute_statement_ok(database, "INSERT INTO lock_rows VALUES (1), (2)");

    row_values[first_row_connection_id_index] = connection_id;
    row_values[second_row_connection_id_index] = connection_id;
    failures += expect_query_result(
        database,
        "SELECT id, GET_LOCK(CONCAT('mylite_row_', id), 0), "
        "IS_USED_LOCK(CONCAT('mylite_row_', id)), "
        "RELEASE_LOCK(CONCAT('mylite_row_', id)) FROM lock_rows ORDER BY id",
        (struct expected_result){
            .values = row_values,
            .column_count = 4U,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "row-backed named lock functions",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT id, BENCHMARK(id, 1) FROM lock_rows ORDER BY id",
        (struct expected_result){
            .values = benchmark_values,
            .column_count = 2U,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "row-backed BENCHMARK function",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT IF(GET_LOCK('mylite_if', 0), 10, 20)",
        (struct expected_result){
            .values = if_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "GET_LOCK in IF condition",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT RELEASE_ALL_LOCKS()",
        (struct expected_result){
            .values = if_release_values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "RELEASE_ALL_LOCKS after IF",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_named_lock_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open diagnostics db");
    failures += execute_error(
        database,
        "SELECT GET_LOCK(NULL, 0)",
        (struct expected_sql_error){
            .code = mysql_error_user_lock_wrong_name,
            .sqlstate = "42000",
            .message_part = "lock name",
        }
    );
    failures += execute_error(
        database,
        "SELECT GET_LOCK('', 0)",
        (struct expected_sql_error){
            .code = mysql_error_user_lock_wrong_name,
            .sqlstate = "42000",
            .message_part = "lock name",
        }
    );
    failures += execute_error(
        database,
        "SELECT GET_LOCK('aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', 0)",
        (struct expected_sql_error){
            .code = mysql_error_user_lock_name_too_long,
            .sqlstate = "42000",
            .message_part = "too long",
        }
    );
    failures += execute_error(
        database,
        "SELECT RELEASE_ALL_LOCKS(1)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );

    mylite_close(database);
    return failures;
}

static int expect_query_result(
    mylite_db *database,
    const char *sql,
    struct expected_result expected
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    failures += expect_result_values(result, expected);
    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = MYLITE_OK;

    if (out_result == NULL) {
        return 1;
    }
    *out_result = NULL;
    rc = mylite_execute(database, sql, strlen(sql), out_result);
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

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
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

static int capture_connection_id(
    mylite_db *database,
    char *out_text,
    size_t out_text_size,
    const char *context
) {
    mylite_result *result = NULL;
    const char *value = NULL;
    int failures = 0;
    int written = 0;

    failures += execute_ok(database, "SELECT CONNECTION_ID()", &result);
    failures += expect_size(mylite_result_column_count(result), 1U, context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    value = mylite_result_value_text(result, 0U, 0U);
    failures += expect_true(value != NULL && value[0] != '\0', context);
    written = snprintf(out_text, out_text_size, "%s", value == NULL ? "" : value);
    if (written < 0 || (size_t)written >= out_text_size) {
        fprintf(stderr, "%s: connection id text buffer too small\n", context);
        failures += 1;
    }
    mylite_result_free(result);
    return failures;
}

static int expect_result_values(const mylite_result *result, struct expected_result expected) {
    int failures = 0;

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            failures += expect_text_or_null(
                mylite_result_value_text(result, row, column),
                expected.values[(row * expected.column_count) + column],
                expected.context
            );
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    return failures;
}

static int expect_warning_rows(
    mylite_db *database,
    size_t row_count,
    struct expected_warning expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, "SHOW WARNINGS", &result);
    failures += expect_size(mylite_result_column_count(result), 3U, context);
    failures += expect_size(mylite_result_row_count(result), row_count, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    for (size_t row = 0U; row < row_count; ++row) {
        failures +=
            expect_text_or_null(mylite_result_value_text(result, row, 0U), "Warning", context);
        failures +=
            expect_text_or_null(mylite_result_value_text(result, row, 1U), expected.code, context);
        failures += expect_text_contains(
            mylite_result_value_text(result, row, 2U),
            expected.message_part,
            context
        );
    }
    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_named_lock_and_info_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path buffer too small\n");
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

static int64_t monotonic_milliseconds(void) {
#ifdef _WIN32
    return (int64_t)GetTickCount64();
#else
    struct timespec timestamp;

    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0) {
        return 0;
    }
    return ((int64_t)timestamp.tv_sec * milliseconds_per_second) +
           ((int64_t)timestamp.tv_nsec / nanoseconds_per_millisecond);
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

    if (written >= 0 && (size_t)written < sizeof(related)) {
        (void)remove(related);
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
        fprintf(stderr, "%s: expected %" PRId64 ", got %" PRId64 "\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_int64_at_least(int64_t actual, int64_t minimum, const char *context) {
    if (actual < minimum) {
        fprintf(
            stderr,
            "%s: expected at least %" PRId64 ", got %" PRId64 "\n",
            context,
            minimum,
            actual
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

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
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

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }
    return 0;
}
