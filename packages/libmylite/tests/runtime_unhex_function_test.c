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
    const char *const *columns;
    size_t column_count;
    const struct expected_cell *values;
    size_t row_count;
    size_t warning_count;
    const char *context;
};

static int test_no_source_dual_and_do_unhex(void);
static int test_table_backed_unhex_and_reopen(void);
static int test_unhex_diagnostics(void);
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

    failures += test_no_source_dual_and_do_unhex();
    failures += test_table_backed_unhex_and_reopen();
    failures += test_unhex_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_unhex(void) {
    static const unsigned char mysql_bytes[] = {0x4d, 0x79, 0x53, 0x51, 0x4c};
    static const unsigned char nul_bytes[] = {0x41, 0x00};
    static const unsigned char odd_bytes[] = {0x0f};
    static const unsigned char abc_bytes[] = {0x0a, 0xbc};
    static const unsigned char binary_literal_bytes[] = {0x0a};
    static const unsigned char pos_bytes[] = {0x01};
    static const unsigned char true_bytes[] = {0x01};
    static const unsigned char false_bytes[] = {0x00};
    static const char *const columns_scalar[] = {
        "word",
        "nul",
        "odd",
        "abc",
        "empty_value",
        "binary_literal",
        "pos",
        "t",
        "f",
        "n",
        "@@warning_count",
    };
    static const struct expected_cell values_scalar[] = {
        {mysql_bytes, sizeof(mysql_bytes), false},
        {nul_bytes, sizeof(nul_bytes), false},
        {odd_bytes, sizeof(odd_bytes), false},
        {abc_bytes, sizeof(abc_bytes), false},
        {NULL, 0U, false},
        {binary_literal_bytes, sizeof(binary_literal_bytes), false},
        {pos_bytes, sizeof(pos_bytes), false},
        {true_bytes, sizeof(true_bytes), false},
        {false_bytes, sizeof(false_bytes), false},
        {NULL, 0U, true},
        {(const unsigned char *)"0", 1U, false},
    };
    static const char *const columns_dual[] = {"u"};
    static const unsigned char dual_bytes[] = {0x41};
    static const struct expected_cell values_dual[] = {{dual_bytes, sizeof(dual_bytes), false}};
    static const char *const columns_invalid[] = {"bad", "@@warning_count"};
    static const struct expected_cell values_invalid[] = {
        {NULL, 0U, true},
        {(const unsigned char *)"0", 1U, false},
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += execute_ok(database, "SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT UNHEX('4D7953514C') AS word, UNHEX('4100') AS nul, "
                   "UNHEX('F') AS odd, UNHEX('ABC') AS abc, UNHEX('') AS empty_value, "
                   "UNHEX(X'41') AS binary_literal, UNHEX(+1) AS pos, UNHEX(TRUE) AS t, "
                   "UNHEX(FALSE) AS f, UNHEX(NULL) AS n, @@warning_count",
            .columns = columns_scalar,
            .column_count = sizeof(columns_scalar) / sizeof(columns_scalar[0]),
            .values = values_scalar,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "no-source unhex values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT UNHEX ('41') AS u FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "dual unhex whitespace",
        }
    );
    failures += execute_ok(database, "DO UNHEX('41'), UNHEX(NULL)", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "unhex do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "unhex do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "unhex do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "unhex do warnings");
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT UNHEX('GG') AS bad, @@warning_count",
            .columns = columns_invalid,
            .column_count = sizeof(columns_invalid) / sizeof(columns_invalid[0]),
            .values = values_invalid,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "invalid scalar unhex warning",
        }
    );
    failures += execute_ok(database, "DO UNHEX('-15')", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_warning_count(result), 1U, "invalid do warning");
    }
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_unhex_and_reopen(void) {
    static const char *const columns_table[] = {"id", "uv", "uvb", "ubi"};
    static const unsigned char value_41[] = {0x41};
    static const unsigned char value_1267[] = {0x12, 0x67};
    static const struct expected_cell values_table[] = {
        {(const unsigned char *)"1", 1U, false},
        {value_41, sizeof(value_41), false},
        {value_41, sizeof(value_41), false},
        {value_1267, sizeof(value_1267), false},
        {(const unsigned char *)"2", 1U, false},
        {NULL, 0U, true},
        {NULL, 0U, true},
        {NULL, 0U, true},
        {(const unsigned char *)"3", 1U, false},
        {NULL, 0U, true},
        {NULL, 0U, true},
        {NULL, 0U, true},
    };
    static const char *const columns_limited[] = {"id", "uv"};
    static const struct expected_cell values_limited[] = {
        {(const unsigned char *)"3", 1U, false},
        {NULL, 0U, true},
        {(const unsigned char *)"2", 1U, false},
        {NULL, 0U, true},
    };
    static const char *const columns_reopen[] = {"uv", "ubi"};
    static const struct expected_cell values_reopen[] = {
        {value_41, sizeof(value_41), false},
        {value_1267, sizeof(value_1267), false},
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
        "CREATE TABLE t(id INT, v VARCHAR(10), vb VARBINARY(10), bi BIGINT)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '41', X'3431', 1267), "
        "(2, 'GG', X'4747', -15), "
        "(3, NULL, NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, UNHEX(v) AS uv, UNHEX(vb) AS uvb, UNHEX(bi) AS ubi "
                   "FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .warning_count = 3U,
            .context = "table unhex values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, UNHEX(v) AS uv FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .warning_count = 1U,
            .context = "table unhex row envelope",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "unhex preamble unchanged"
    );

    mylite_close(database);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen unhex file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT UNHEX(v) AS uv, UNHEX(bi) AS ubi FROM t WHERE id = 1",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "unhex reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_unhex_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, v VARCHAR(20), d DECIMAL(5,2), bitcol BIT(4))",
        NULL
    );
    failures += execute_ok(database, "INSERT INTO t VALUES (1, '41', 12.30, b'1010')", NULL);
    failures += execute_error(
        database,
        "SELECT UNHEX()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'UNHEX'",
        }
    );
    failures += execute_error(
        database,
        "SELECT UNHEX('a', 'b')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'UNHEX'",
        }
    );
    failures += execute_error(
        database,
        "SELECT UNHEX(missing)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT UNHEX(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT UNHEX(12.30)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UNHEX() supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT UNHEX(d) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "UNHEX() supports only integer, nonbinary string, and binary string columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT UNHEX(bitcol) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "UNHEX() supports only integer, nonbinary string, and binary string columns",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t SET v = UNHEX(v)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '('",
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
    for (size_t column_index = 0U; failures == 0 && column_index < expected.column_count;
         ++column_index) {
        failures += expect_text(
            mylite_result_column_name(result, column_index),
            expected.columns[column_index],
            expected.context
        );
    }
    for (size_t row_index = 0U; failures == 0 && row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
            size_t offset = (row_index * expected.column_count) + column_index;

            failures += expect_result_cell(
                result,
                row_index,
                column_index,
                expected.values[offset],
                expected.context
            );
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
    int failures = 0;

    if (make_test_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "runtime_unhex_function_%s_%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "%s: failed to build path\n", name);
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
    remove(path);
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
    remove(related);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek file\n", path);
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    fclose(file);
    if (bytes_read != size) {
        fprintf(stderr, "%s: expected %zu bytes, read %zu\n", path, size, bytes_read);
        return 1;
    }
    return 0;
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
            fprintf(stderr, "%s: row %zu column %zu expected NULL\n", context, row, column);
            return 1;
        }
        return 0;
    }
    if (actual == NULL) {
        fprintf(stderr, "%s: row %zu column %zu expected non-NULL\n", context, row, column);
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
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected text [%s], got [%s]\n",
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

static int expect_bytes(
    const void *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if ((actual == NULL || expected == NULL || memcmp(actual, expected, size) != 0) && size != 0U) {
        fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }
    return 0;
}
