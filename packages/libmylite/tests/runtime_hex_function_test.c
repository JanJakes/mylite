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
    mysql_error_unknown_column = 1054,
    mysql_error_parse = 1064,
    mysql_error_native_function_count = 1582,
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

static int test_no_source_dual_and_do_hex(void);
static int test_table_backed_hex_and_reopen(void);
static int test_hex_diagnostics(void);
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
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
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

    failures += test_no_source_dual_and_do_hex();
    failures += test_table_backed_hex_and_reopen();
    failures += test_hex_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_hex(void) {
    static const char *const columns_scalar[] = {
        "s",
        "utf8",
        "nul",
        "empty_hex",
        "quoted_hex",
        "zero_x",
        "cast_bin",
        "conv",
        "convc",
        "num",
        "neg",
        "t",
        "f",
        "n",
        "pnum",
        "pnull",
        "ptrue",
        "pneg",
    };
    static const char *const values_scalar[] = {
        "616263",
        "C3A9",
        "610062",
        "",
        "0061",
        "6162",
        "414243",
        "414243",
        "414243",
        "FF",
        "FFFFFFFFFFFFFFFF",
        "1",
        "0",
        NULL,
        "FF",
        NULL,
        "1",
        "FFFFFFFFFFFFFFFF",
    };
    static const char *const columns_dual[] = {"h"};
    static const char *const values_dual[] = {"61"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    static const char *const columns_numeric_scalars[] = {
        "row_hex",
        "last_hex",
        "warn_hex",
        "auto_hex",
        "safe_hex",
        "limit_hex",
        "timestamp_hex",
    };
    static const char *const values_numeric_scalars[] = {
        "0",
        "0",
        "0",
        "1",
        "0",
        "FFFFFFFFFFFFFFFF",
        "6553F100",
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
            .sql = "SELECT HEX('abc') AS s, HEX('\xC3\xA9') AS utf8, HEX('a\\0b') AS nul, "
                   "HEX('') AS empty_hex, HEX(X'0061') AS quoted_hex, HEX(0x6162) AS zero_x, "
                   "HEX(CAST('ABC' AS BINARY)) AS cast_bin, "
                   "HEX(CONVERT('ABC', BINARY)) AS conv, "
                   "HEX(CONVERT('ABC' USING utf8mb4)) AS convc, HEX(255) AS num, "
                   "HEX(-1) AS neg, HEX(TRUE) AS t, HEX(FALSE) AS f, HEX(NULL) AS n, "
                   "HEX((255)) AS pnum, HEX((NULL)) AS pnull, HEX((TRUE)) AS ptrue, "
                   "HEX((-1)) AS pneg",
            .columns = columns_scalar,
            .column_count = sizeof(columns_scalar) / sizeof(columns_scalar[0]),
            .values = values_scalar,
            .row_count = 1U,
            .context = "no-source hex values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT HEX ('a') AS h FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual hex whitespace",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_row_status,
            .column_count = sizeof(columns_row_status) / sizeof(columns_row_status[0]),
            .values = values_after_select,
            .row_count = 1U,
            .context = "row count after hex select",
        }
    );

    failures += execute_ok(database, "DO HEX('abc'), HEX(NULL), HEX(255)", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "hex do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "hex do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "hex do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "hex do warnings");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_row_status,
            .column_count = sizeof(columns_row_status) / sizeof(columns_row_status[0]),
            .values = values_after_do,
            .row_count = 1U,
            .context = "row count after hex do",
        }
    );
    failures += execute_ok(database, "SET timestamp = 1700000000", NULL);
    failures += execute_ok(database, "DO 0", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT HEX(ROW_COUNT()) AS row_hex, HEX(LAST_INSERT_ID()) AS last_hex, "
                   "HEX(@@warning_count) AS warn_hex, HEX(@@autocommit) AS auto_hex, "
                   "HEX(@@sql_safe_updates) AS safe_hex, HEX(@@sql_select_limit) AS limit_hex, "
                   "HEX(@@timestamp) AS timestamp_hex",
            .columns = columns_numeric_scalars,
            .column_count = sizeof(columns_numeric_scalars) / sizeof(columns_numeric_scalars[0]),
            .values = values_numeric_scalars,
            .row_count = 1U,
            .context = "numeric scalar hex values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_hex_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "hv",
        "hc",
        "ht",
        "hb",
        "hvb",
        "hbl",
        "hbi",
    };
    static const char *const values_table[] = {
        "1", "C3A9", "61", "68656C6C6F", "410000", "410042", "00FF", "FFFFFFFFFFFFFFFF",
        "2", "",     "",   "",           "000000", "",       "",     "FF",
        "3", NULL,   NULL, NULL,         NULL,     NULL,     NULL,   NULL,
    };
    static const char *const columns_limited[] = {"id", "h"};
    static const char *const values_limited[] = {"3", NULL, "2", "FF"};
    static const char *const columns_labels[] = {"HEX(v)", "h"};
    static const char *const values_labels[] = {"C3A9", "FFFFFFFFFFFFFFFF"};
    static const char *const columns_reopen[] = {"hv", "hbi"};
    static const char *const values_reopen[] = {"C3A9", "FFFFFFFFFFFFFFFF"};
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
        "id INT, v VARCHAR(10), c CHAR(5), txt TEXT, b BINARY(3), vb VARBINARY(3), "
        "bl BLOB, bi BIGINT, d DECIMAL(5,2), bitcol BIT(4)"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '\xC3\xA9', 'a  ', 'hello', 'A', X'410042', X'00FF', -1, 12.30, b'1010'), "
        "(2, '', '', '', X'', X'', X'', 255, NULL, NULL), "
        "(3, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, HEX(v) AS hv, HEX(c) AS hc, HEX(txt) AS ht, HEX(b) AS hb, "
                   "HEX(vb) AS hvb, HEX(bl) AS hbl, HEX(bi) AS hbi FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .context = "table hex values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, HEX(bi) AS h FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table hex row envelope",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT HEX(v), HEX(bi) AS h FROM t WHERE id = 1",
            .columns = columns_labels,
            .column_count = sizeof(columns_labels) / sizeof(columns_labels[0]),
            .values = values_labels,
            .row_count = 1U,
            .context = "hex labels",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "hex preamble unchanged"
    );

    mylite_close(database);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen hex file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT HEX(v) AS hv, HEX(bi) AS hbi FROM t WHERE id = 1",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .context = "hex reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_hex_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, v VARCHAR(20), d DECIMAL(5,2), bitcol BIT(4))",
        NULL
    );
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 'abc', 12.30, b'1010')", NULL);
    failures += execute_error(
        database,
        "SELECT HEX()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'HEX'",
        }
    );
    failures += execute_error(
        database,
        "SELECT HEX('a', 'b')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'HEX'",
        }
    );
    failures += execute_error(
        database,
        "SELECT HEX(missing)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT HEX(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT HEX(12.30)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "HEX() supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT HEX(d) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "HEX() supports only integer, nonbinary string, and binary string columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT HEX(bitcol) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "HEX() supports only integer, nonbinary string, and binary string columns",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t SET v = HEX(v)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'HEX'",
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
    int written =
        snprintf(path, path_size, "runtime_hex_function_%s_%d.mylite", name, current_process_id());

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

    if (written >= 0 && (size_t)written < sizeof(related)) {
        remove(related);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_size = 0U;

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek file\n", path);
        fclose(file);
        return 1;
    }
    read_size = fread(buffer, 1U, size, file);
    fclose(file);
    if (read_size != size) {
        fprintf(stderr, "%s: expected to read %zu bytes, got %zu\n", path, size, read_size);
        return 1;
    }
    return 0;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    size_t value_count = expected.row_count * expected.column_count;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures == 0) {
        failures += expect_size(
            mylite_result_column_count(result),
            expected.column_count,
            expected.context
        );
        failures +=
            expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    }
    for (size_t column = 0U; failures == 0 && column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t index = 0U; failures == 0 && index < value_count; ++index) {
        size_t row = index / expected.column_count;
        size_t column = index % expected.column_count;

        failures +=
            expect_result_value(result, row, column, expected.values[index], expected.context);
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

    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s: row %zu column %zu expected %s, got %s\n",
                context,
                row,
                column,
                expected == NULL ? "(null)" : expected,
                actual == NULL ? "(null)" : actual
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: row %zu column %zu expected %s, got %s\n",
            context,
            row,
            column,
            expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
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
        fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }
    return 0;
}
