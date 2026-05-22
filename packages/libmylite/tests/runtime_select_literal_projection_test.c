#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

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
    sql_buffer_capacity = 160,
    literal_significant_digit_count = 81,
    literal_rejected_digit_count = literal_significant_digit_count + 1,
    literal_significant_text_capacity = literal_significant_digit_count + 1,
    literal_rejected_text_capacity = literal_rejected_digit_count + 1,
    core_literal_column_count = 6,
    core_literal_with_string_column_count = 8,
    mysql_error_parse = 1064,
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
    size_t row_count;
    const char *context;
};

static int test_literal_projection_values_and_file_safety(void);
static int test_literal_projection_diagnostics_and_table_selects(void);
static int test_independent_literal_projection_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
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

    failures += test_literal_projection_values_and_file_safety();
    failures += test_literal_projection_diagnostics_and_table_selects();
    failures += test_independent_literal_projection_handles();

    return failures == 0 ? 0 : 1;
}

static int test_literal_projection_values_and_file_safety(void) {
    static const char *const columns_core[] =
        {"0001", "0001", "-0001", "NULL", "TRUE", "false", "'x'", "\"y\""};
    static const char *const values_core[] = {"1", "1", "-1", NULL, "1", "0", "x", "y"};
    static const char *const columns_dual_all[] = {"1", "1", "-1", "NULL", "TRUE", "FALSE"};
    static const char *const values_dual_all[] = {"1", "1", "-1", NULL, "1", "0"};
    static const char *const columns_aliases[] = {"one", "plus_one", "neg", "n", "t", "f"};
    static const char *const values_aliases[] = {"1", "1", "-1", NULL, "1", "0"};
    static const char *const columns_zero[] = {"000000", "000000", "-000000"};
    static const char *const values_zero[] = {"0", "0", "0"};
    static const char *const columns_parenthesized[] =
        {"1", "NULL", "(TRUE)", "(FALSE)", "2", "(-3)"};
    static const char *const values_parenthesized[] = {"1", NULL, "1", "0", "2", "-3"};
    static const char *const column_row_count[] = {"ROW_COUNT()"};
    static const char *const value_negative_one[] = {"-1"};
    char path[test_path_capacity];
    char digits81[literal_significant_text_capacity];
    char select_digits81[sql_buffer_capacity];
    const char *columns_digits81[] = {digits81};
    const char *values_digits81[] = {digits81};
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    memset(digits81, '9', literal_significant_digit_count);
    digits81[literal_significant_digit_count] = '\0';
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 0001, +0001, -0001, NULL, TRUE, false, 'x', \"y\"",
            .columns = columns_core,
            .column_count = core_literal_with_string_column_count,
            .values = values_core,
            .row_count = 1U,
            .context = "core no-source literals",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ALL 1, +1, -1, NULL, TRUE, FALSE FROM DUAL",
            .columns = columns_dual_all,
            .column_count = core_literal_column_count,
            .values = values_dual_all,
            .row_count = 1U,
            .context = "dual all literals",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 AS one, +1 plus_one, -1 AS neg, NULL n, TRUE t, false f",
            .columns = columns_aliases,
            .column_count = core_literal_column_count,
            .values = values_aliases,
            .row_count = 1U,
            .context = "literal aliases",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 000000, +000000, -000000",
            .columns = columns_zero,
            .column_count = 3U,
            .values = values_zero,
            .row_count = 1U,
            .context = "signed zero normalization",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT (1), (NULL), (TRUE), (FALSE), (+2), (-3)",
            .columns = columns_parenthesized,
            .column_count = core_literal_column_count,
            .values = values_parenthesized,
            .row_count = 1U,
            .context = "parenthesized literal projection",
        }
    );

    if (snprintf(select_digits81, sizeof(select_digits81), "SELECT %s", digits81) < 0) {
        failures += expect_int(1, 0, "format 81 digit query");
    } else {
        failures += expect_query(
            database,
            (struct expected_query){
                .sql = select_digits81,
                .columns = columns_digits81,
                .column_count = 1U,
                .values = values_digits81,
                .row_count = 1U,
                .context = "81 significant digits",
            }
        );
    }

    failures += execute_ok(database, "SELECT 1", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .columns = column_row_count,
            .column_count = 1U,
            .values = value_negative_one,
            .row_count = 1U,
            .context = "row count after literal select",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read values preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "literal select leaves preamble unchanged"
    );

    remove_related_files(path);
    return failures;
}

static int test_literal_projection_diagnostics_and_table_selects(void) {
    static const char *const column_id[] = {"id"};
    static const char *const values_id[] = {"1", "2"};
    char path[test_path_capacity];
    char digits82[literal_rejected_text_capacity];
    char select_digits82[sql_buffer_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "table") != 0) {
        return 1;
    }
    memset(digits82, '9', literal_rejected_digit_count);
    digits82[literal_rejected_digit_count] = '\0';

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open diagnostics memory");
    failures += execute_error(
        database,
        "SELECT 1.0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT scalar projection supports only session scalar values",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 ORDER BY 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'ORDER'",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 LIMIT 0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'LIMIT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'LIMIT'",
        }
    );
    if (snprintf(select_digits82, sizeof(select_digits82), "SELECT %s", digits82) < 0) {
        failures += expect_int(1, 0, "format 82 digit query");
    } else {
        failures += execute_error(
            database,
            select_digits82,
            (struct expected_sql_error){
                .code = mysql_error_parse,
                .sqlstate = "42000",
                .message_part = "at most 81 significant decimal digits",
            }
        );
    }

    mylite_close(database);
    database = NULL;

    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open table diagnostics file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE t(id INT NOT NULL)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1), (2)", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t ORDER BY id",
            .columns = column_id,
            .column_count = 1U,
            .values = values_id,
            .row_count = 2U,
            .context = "descriptor table select still works",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_literal_projection_handles(void) {
    static const char *const column_one[] = {"one"};
    static const char *const value_one[] = {"1"};
    static const char *const column_two[] = {"two"};
    static const char *const value_two[] = {"2"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first memory");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second memory");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT 1 AS one",
            .columns = column_one,
            .column_count = 1U,
            .values = value_one,
            .row_count = 1U,
            .context = "first handle literal",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT 2 AS two",
            .columns = column_two,
            .column_count = 1U,
            .values = value_two,
            .row_count = 1U,
            .context = "second handle literal",
        }
    );

    mylite_close(second);
    mylite_close(first);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
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
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[value_index],
                expected.context
            );
        }
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

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-select-literal-%s-%d.mylite",
        name,
        current_process_id()
    );

    return written < 0 || (size_t)written >= path_size ? 1 : 0;
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
    char related_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_size = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    read_size = fread(buffer, 1U, size, file);
    fclose(file);

    return read_size == size ? 0 : 1;
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
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected == NULL ? "(null)" : expected,
                actual == NULL ? "(null)" : actual
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected %s, got %s\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected message containing %s, got %s\n",
            context,
            needle == NULL ? "(null)" : needle,
            actual == NULL ? "(null)" : actual
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
