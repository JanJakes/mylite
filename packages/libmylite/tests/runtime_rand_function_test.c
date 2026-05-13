#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

#include <stdbool.h>
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
    rand_core_column_count = 7,
    rand_alias_column_count = 2,
    rand_mixed_column_count = 3,
    diagnostic_column_count = 2,
    show_warning_column_count = 3,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
    mysql_error_native_function_arity = 1582,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    size_t column_count;
    const char *const *values;
    const bool *range_columns;
    size_t row_count;
    size_t warning_count;
    int64_t affected_rows;
    const char *context;
};

static int test_rand_values_and_file_safety(void);
static int test_rand_do_and_independent_handles(void);
static int test_rand_errors_and_unsupported_forms(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const struct expected_query *expected
);
static int expect_rand_value(const char *actual, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_rand_values_and_file_safety();
    failures += test_rand_do_and_independent_handles();
    failures += test_rand_errors_and_unsupported_forms();

    return failures == 0 ? 0 : 1;
}

static int test_rand_values_and_file_safety(void) {
    static const char *const rand_core_columns[] = {
        "RAND()",
        "rand()",
        "Rand()",
        "RAND ()",
        "(RAND())",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const rand_core_values[] = {NULL, NULL, NULL, NULL, NULL, "0", "0"};
    static const bool rand_core_ranges[] = {true, true, true, true, true, false, false};
    static const char *const rand_alias_columns[] = {"r", "q"};
    static const char *const rand_alias_values[] = {NULL, NULL};
    static const bool rand_alias_ranges[] = {true, true};
    static const char *const rand_mixed_columns[] = {"RAND()", "VERSION()", "DATABASE()"};
    static const char *const rand_mixed_values[] = {NULL, MYLITE_VERSION_STRING, "app"};
    static const bool rand_mixed_ranges[] = {true, false, false};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open rand values file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE catalog_guard(id INT)", NULL);
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += execute_ok(database, "DO 0", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT RAND(),rand(),Rand(),RAND (),(RAND()),@@warning_count,ROW_COUNT()",
            .columns = rand_core_columns,
            .column_count = rand_core_column_count,
            .values = rand_core_values,
            .range_columns = rand_core_ranges,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "core rand values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT RAND() AS r, rand() q FROM DUAL",
            .columns = rand_alias_columns,
            .column_count = rand_alias_column_count,
            .values = rand_alias_values,
            .range_columns = rand_alias_ranges,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "rand aliases",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT RAND(), VERSION(), DATABASE()",
            .columns = rand_mixed_columns,
            .column_count = rand_mixed_column_count,
            .values = rand_mixed_values,
            .range_columns = rand_mixed_ranges,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "rand mixed scalar values",
        }
    );

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "rand catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "rand sqlite schema generation unchanged"
    );
    mylite_close(database);
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures +=
        expect_bytes(actual_preamble, expected_preamble, sizeof(actual_preamble), "rand preamble");
    remove_related_files(path);
    return failures;
}

static int test_rand_do_and_independent_handles(void) {
    static const char *const diagnostic_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const do_values[] = {"0", "0"};
    static const char *const rand_columns[] = {"RAND()"};
    static const char *const rand_values[] = {NULL};
    static const bool rand_ranges[] = {true};
    mylite_db *database = NULL;
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open rand do handle");
    failures += execute_ok(database, "DO RAND(), rand()", &result);
    failures += expect_size(mylite_result_column_count(result), 0U, "rand DO column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "rand DO row count");
    failures += expect_size(mylite_result_warning_count(result), 0U, "rand DO warnings");
    failures += expect_int64(mylite_result_affected_rows(result), 0, "rand DO affected rows");
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count,ROW_COUNT()",
            .columns = diagnostic_columns,
            .column_count = diagnostic_column_count,
            .values = do_values,
            .range_columns = NULL,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "rand DO diagnostics",
        }
    );
    mylite_close(database);

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first rand handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second rand handle");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT RAND()",
            .columns = rand_columns,
            .column_count = 1U,
            .values = rand_values,
            .range_columns = rand_ranges,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first rand handle",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT RAND()",
            .columns = rand_columns,
            .column_count = 1U,
            .values = rand_values,
            .range_columns = rand_ranges,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second rand handle",
        }
    );
    mylite_close(first);
    mylite_close(second);
    return failures;
}

static int test_rand_errors_and_unsupported_forms(void) {
    static const char *const show_warning_columns[] = {"Level", "Code", "Message"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open rand unsupported file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE t(id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (7), (8)", NULL);

    failures += execute_ok(database, "SELECT RAND()", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = show_warning_columns,
            .column_count = show_warning_column_count,
            .values = NULL,
            .range_columns = NULL,
            .row_count = 0U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "rand warning list",
        }
    );
    failures += execute_error(
        database,
        "SELECT RAND(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "RAND(seed) is not supported",
        }
    );
    failures += execute_error(
        database,
        "DO RAND(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "RAND(seed) is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT RAND(1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'RAND'",
        }
    );
    failures += execute_error(
        database,
        "SELECT RAND",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'RAND' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT RAND() FROM t ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1+RAND()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT ABS(RAND())",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ABS() supports",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", sql, mylite_errmsg(database));
        return failures;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
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

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, expected.context);
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            failures += expect_result_value(result, row, column, &expected);
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const struct expected_query *expected
) {
    const char *actual = mylite_result_value_text(result, row, column);
    const char *value = expected->values[(row * expected->column_count) + column];

    if (expected->range_columns != NULL && expected->range_columns[column]) {
        return expect_rand_value(actual, expected->context);
    }
    if (value == NULL) {
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s: expected NULL at %zu,%zu, got %s\n",
                expected->context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }
    return expect_text(actual, value, expected->context);
}

static int expect_rand_value(const char *actual, const char *context) {
    char *end = NULL;
    double value = 0.0;

    if (actual == NULL || actual[0] == '\0') {
        fprintf(stderr, "%s: expected RAND() value, got NULL or empty\n", context);
        return 1;
    }
    value = strtod(actual, &end);
    if (end == actual || end == NULL || *end != '\0' || !(value >= 0.0 && value < 1.0)) {
        fprintf(stderr, "%s: expected RAND() value in [0,1), got [%s]\n", context, actual);
        return 1;
    }
    return 0;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *tmpdir = getenv("TMPDIR");
    int written = 0;

    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = "/tmp";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_rand_function_%d_%s.mylite",
        tmpdir,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path too long\n");
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
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(related)) {
        (void)remove(related);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0 || fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read file\n", path);
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
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

static int expect_text(const char *actual, const char *expected, const char *context) {
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

static int expect_contains(const char *actual, const char *needle, const char *context) {
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

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }
    return 0;
}
