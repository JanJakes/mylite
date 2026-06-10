#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

#include <stdbool.h>
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
    path_suffix_capacity = 16,
    base64_sql_capacity = 256,
    base64_table_column_count = 6,
    mysql_error_unknown_column = 1054,
    mysql_error_parse = 1064,
    mysql_error_native_function_count = 1582,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_cell {
    const void *bytes;
    size_t size;
    bool is_null;
};

struct expected_query {
    const char *sql;
    size_t column_count;
    const struct expected_cell *values;
    size_t row_count;
    size_t warning_count;
    const char *context;
};

static int test_no_source_dual_and_do_base64(void);
static int test_table_backed_base64_and_reopen(void);
static int test_base64_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_result_cell(
    const mylite_result *result,
    size_t row,
    size_t column,
    struct expected_cell expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(const void *actual, const void *expected, size_t size, const char *context);

int main(void) {
    int failures = 0;

    failures += test_no_source_dual_and_do_base64();
    failures += test_table_backed_base64_and_reopen();
    failures += test_base64_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_base64(void) {
    static const unsigned char abc_bytes[] = {0x61, 0x62, 0x63};
    static const char exact_76_expected[] =
        "YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh";
    static const char long_expected[] =
        "YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh\n"
        "YQ==";
    static const char *exact_76_literal =
        "'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'";
    static const char *long_literal =
        "'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'";
    static const struct expected_cell scalar_values[] = {
        {(const unsigned char *)"YQ==", 4U, false},
        {(const unsigned char *)"YWI=", 4U, false},
        {(const unsigned char *)"YWJj", 4U, false},
        {(const unsigned char *)"YWJjZA==", 8U, false},
        {(const unsigned char *)"YWJjZGVm", 8U, false},
        {NULL, 0U, false},
        {NULL, 0U, true},
        {(const unsigned char *)"MTIz", 4U, false},
        {(const unsigned char *)"MQ==", 4U, false},
        {(const unsigned char *)"MA==", 4U, false},
        {(const unsigned char *)"LTE=", 4U, false},
        {(const unsigned char *)"MQ==", 4U, false},
        {(const unsigned char *)"LTE=", 4U, false},
        {abc_bytes, sizeof(abc_bytes), false},
        {abc_bytes, sizeof(abc_bytes), false},
        {NULL, 0U, false},
        {NULL, 0U, true},
        {NULL, 0U, true},
        {NULL, 0U, true},
        {(const unsigned char *)"0", 1U, false},
        {(const unsigned char *)"dGVzdA==", 8U, false},
        {(const unsigned char *)"test", 4U, false},
    };
    static const struct expected_cell exact_76_value[] = {
        {(const unsigned char *)exact_76_expected, sizeof(exact_76_expected) - 1U, false},
    };
    static const struct expected_cell long_value[] = {
        {(const unsigned char *)long_expected, sizeof(long_expected) - 1U, false},
    };
    char path[test_path_capacity];
    char sql[base64_sql_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += execute_ok(database, "SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TO_BASE64('a'), TO_BASE64('ab'), TO_BASE64('abc'), "
                   "TO_BASE64('abcd'), TO_BASE64('abcdef'), TO_BASE64(''), TO_BASE64(NULL), "
                   "TO_BASE64(123), TO_BASE64(TRUE), TO_BASE64(FALSE), TO_BASE64(-1), "
                   "TO_BASE64(+1), TO_BASE64(- 1), FROM_BASE64('Y W J j'), "
                   "FROM_BASE64('Y\\tW\\rJ\\nj'), "
                   "FROM_BASE64(''), FROM_BASE64('bad!'), "
                   "FROM_BASE64('Y'), FROM_BASE64('Y==='), @@warning_count, "
                   "TO_BASE64(FROM_BASE64('dGVzdA==')), FROM_BASE64(TO_BASE64('test'))",
            .column_count = sizeof(scalar_values) / sizeof(scalar_values[0]),
            .values = scalar_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "scalar base64 values",
        }
    );

    snprintf(sql, sizeof(sql), "SELECT TO_BASE64(%s)", exact_76_literal);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = sql,
            .column_count = 1U,
            .values = exact_76_value,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "exact line width base64 wrapping",
        }
    );

    snprintf(sql, sizeof(sql), "SELECT TO_BASE64(%s)", long_literal);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = sql,
            .column_count = 1U,
            .values = long_value,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "long base64 wrapping",
        }
    );

    failures += execute_ok(database, "DO TO_BASE64('abc'), FROM_BASE64('YWJj')", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "base64 do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "base64 do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "base64 do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "base64 do warnings");
    }
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_base64_and_reopen(void) {
    static const unsigned char abc_bytes[] = {0x61, 0x62, 0x63};
    static const struct expected_cell table_values[] = {
        {(const unsigned char *)"1", 1U, false},
        {(const unsigned char *)"YWJj", 4U, false},
        {(const unsigned char *)"YQBi", 4U, false},
        {(const unsigned char *)"MTIz", 4U, false},
        {abc_bytes, sizeof(abc_bytes), false},
        {abc_bytes, sizeof(abc_bytes), false},
        {(const unsigned char *)"2", 1U, false},
        {NULL, 0U, true},
        {NULL, 0U, true},
        {NULL, 0U, true},
        {NULL, 0U, true},
        {NULL, 0U, true},
        {(const unsigned char *)"3", 1U, false},
        {NULL, 0U, false},
        {NULL, 0U, false},
        {(const unsigned char *)"MA==", 4U, false},
        {NULL, 0U, true},
        {NULL, 0U, true},
    };
    static const struct expected_cell reopen_values[] = {
        {(const unsigned char *)"YWJj", 4U, false},
        {abc_bytes, sizeof(abc_bytes), false},
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    mylite_file_preamble_init(expected_preamble);
    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT, v VARCHAR(20), vb VARBINARY(20), bi BIGINT, body TEXT, blobv BLOB, "
        "d DECIMAL(5,2))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 'abc', X'610062', 123, 'YWJj', X'59574A6A', 12.30), "
        "(2, NULL, NULL, NULL, NULL, NULL, 12.30), "
        "(3, '', X'', 0, 'bad!', X'62616421', 12.30)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, TO_BASE64(v), TO_BASE64(vb), TO_BASE64(bi), "
                   "FROM_BASE64(body), FROM_BASE64(blobv) FROM t ORDER BY id",
            .column_count = base64_table_column_count,
            .values = table_values,
            .row_count = 3U,
            .warning_count = 0U,
            .context = "table base64 values",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "base64 preamble unchanged"
    );

    mylite_close(database);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen base64 file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TO_BASE64(v), FROM_BASE64(body) FROM t WHERE id = 1",
            .column_count = 2U,
            .values = reopen_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "base64 reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_base64_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, v VARCHAR(20), d DECIMAL(5,2), bitcol BIT(4))",
        NULL
    );
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 'YWJj', 12.30, b'1010')", NULL);
    failures += execute_error(
        database,
        "SELECT TO_BASE64()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'TO_BASE64'",
        }
    );
    failures += execute_error(
        database,
        "SELECT FROM_BASE64('a', 'b')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'FROM_BASE64'",
        }
    );
    failures += execute_error(
        database,
        "SELECT TO_BASE64(missing)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT FROM_BASE64(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT TO_BASE64(12.30)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "TO_BASE64() supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT TO_BASE64(d) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "TO_BASE64() supports only integer, nonbinary string, and binary string columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT FROM_BASE64(bitcol) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "FROM_BASE64() supports only integer, nonbinary string, and binary string columns",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t SET v = TO_BASE64(v)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
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

    if (failures == 0) {
        failures += expect_size(
            mylite_result_column_count(result),
            expected.column_count,
            expected.context
        );
        failures +=
            expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
        failures += expect_size(
            mylite_result_warning_count(result),
            expected.warning_count,
            expected.context
        );
    }
    for (size_t row = 0U; failures == 0 && row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t index = (row * expected.column_count) + column;

            failures +=
                expect_result_cell(result, row, column, expected.values[index], expected.context);
        }
    }
    mylite_result_free(result);
    return failures;
}

static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
) {
    int failures = make_test_path(path, path_size, name);

    if (failures != 0) {
        return failures;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, "open test database");
    failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    failures += execute_ok(*out_database, "USE app", NULL);
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-runtime-base64-%d-%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path\n");
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
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    int failures = 0;

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek\n", path);
        failures = 1;
    } else if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read\n", path);
        failures = 1;
    }
    fclose(file);
    return failures;
}

static int expect_result_cell(
    const mylite_result *result,
    size_t row,
    size_t column,
    struct expected_cell expected,
    const char *context
) {
    const void *actual = mylite_result_value_bytes(result, row, column);
    size_t actual_size = mylite_result_value_size(result, row, column);
    int failures = 0;

    if (expected.is_null) {
        if (actual != NULL) {
            fprintf(stderr, "%s row %zu column %zu: expected NULL\n", context, row, column);
            return 1;
        }
        return 0;
    }
    if (actual == NULL) {
        fprintf(stderr, "%s row %zu column %zu: expected non-NULL\n", context, row, column);
        return 1;
    }
    failures += expect_size(actual_size, expected.size, context);
    if (failures == 0 && expected.size != 0U) {
        failures += expect_bytes(actual, expected.bytes, expected.size, context);
    }
    return failures;
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
        fprintf(stderr, "%s: expected size %zu, got %zu\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "NULL" : actual,
            needle
        );
        return 1;
    }
    return 0;
}

static int expect_bytes(
    const void *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }
    return 0;
}
