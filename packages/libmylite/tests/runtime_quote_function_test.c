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
    mysql_error_native_function_argument_count = 1582,
    mysql_error_unknown_column = 1054,
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

static int test_no_source_dual_and_do_quote(void);
static int test_table_backed_quote_and_reopen(void);
static int test_quote_diagnostics(void);
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

    failures += test_no_source_dual_and_do_quote();
    failures += test_table_backed_quote_and_reopen();
    failures += test_quote_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_quote(void) {
    static const char *const columns_no_source[] = {
        "quoted",
        "null_text",
        "empty_value",
        "ascii_value",
        "int_value",
        "neg_value",
        "true_value",
        "false_value",
        "decimal_value",
        "escape_value",
        "session_value",
        "system_value",
    };
    static const char *const values_no_source[] = {
        "'Don\\'t!'",
        "NULL",
        "''",
        "'abc'",
        "'123'",
        "'-7'",
        "'1'",
        "'0'",
        "'1.50'",
        "'a\\0b\\Z\\\\\\'\"'",
        "'app'",
        "'0'",
    };
    static const char *const columns_dual[] = {"a", "b"};
    static const char *const values_dual[] = {"'abc'", "'123'"};
    static const char *const columns_no_backslash[] = {"q"};
    static const char *const values_no_backslash[] = {"'a\\\\\\\\b'"};
    static const char *const columns_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT QUOTE('Don\\'t!') AS quoted, QUOTE(NULL) AS null_text, "
                   "QUOTE('') AS empty_value, QUOTE('abc') AS ascii_value, "
                   "QUOTE(123) AS int_value, QUOTE(-7) AS neg_value, "
                   "QUOTE(TRUE) AS true_value, QUOTE(FALSE) AS false_value, "
                   "QUOTE(1.50) AS decimal_value, "
                   "QUOTE('a\\0b\\Z\\\\\\'\"') AS escape_value, "
                   "QUOTE(DATABASE()) AS session_value, "
                   "QUOTE(@@warning_count) AS system_value",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source quote values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT QUOTE ('abc') AS a, QUOTE((123)) AS b FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual quote whitespace",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_status,
            .column_count = sizeof(columns_status) / sizeof(columns_status[0]),
            .values = values_after_select,
            .row_count = 1U,
            .context = "row count after quote select",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT QUOTE('a\\\\b') AS q",
            .columns = columns_no_backslash,
            .column_count = sizeof(columns_no_backslash) / sizeof(columns_no_backslash[0]),
            .values = values_no_backslash,
            .row_count = 1U,
            .context = "quote no backslash escapes",
        }
    );

    failures += execute_ok(database, "DO QUOTE('abc'), QUOTE(NULL)", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "quote do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "quote do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "quote do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "quote do warnings");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_status,
            .column_count = sizeof(columns_status) / sizeof(columns_status[0]),
            .values = values_after_do,
            .row_count = 1U,
            .context = "row count after quote do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_quote_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "qv",
        "qc",
        "qtxt",
        "qi",
        "qd",
        "qy",
        "qdt",
        "qtm",
        "qdttm",
        "qts",
    };
    static const char *const values_table[] = {
        "1",
        "'a\\'b'",
        "'xy'",
        "'x\\\\y'",
        "'12'",
        "'-12.30'",
        "'2024'",
        "'2024-01-02'",
        "'03:04:05'",
        "'2024-01-02 03:04:05'",
        "'2024-01-02 03:04:06'",
        "2",
        "NULL",
        "NULL",
        "NULL",
        "NULL",
        "NULL",
        "NULL",
        "NULL",
        "NULL",
        "NULL",
        "NULL",
    };
    static const char *const columns_limited[] = {"id", "qv"};
    static const char *const values_limited[] = {"1", "'a\\'b'"};
    static const char *const columns_order[] = {"id", "qv"};
    static const char *const values_order[] = {"1", "'a\\'b'", "2", "NULL"};
    static const char *const columns_update[] = {"id", "outv"};
    static const char *const values_update[] = {"1", "'a\\'b'"};
    static const char *const columns_literal[] = {"id", "qs", "qd", "qn"};
    static const char *const values_literal[] = {"1", "'row'", "'1.50'", "NULL"};
    static const char *const columns_labels[] = {"QUOTE(v)", "quoted"};
    static const char *const values_labels[] = {"'a\\'b'", "'a\\'b'"};
    static const char *const columns_reopen[] = {"id", "qv"};
    static const char *const values_reopen[] = {"1", "'a\\'b'"};
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
        "id INT, v VARCHAR(20), c CHAR(5), txt TEXT, i INT, d DECIMAL(6,2), y YEAR, "
        "dt DATE, tm TIME, dttm DATETIME, ts TIMESTAMP NULL, b VARBINARY(8), f DOUBLE"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 'a\\'b', 'xy', 'x\\\\y', 12, -12.30, 2024, '2024-01-02', '03:04:05', "
        "'2024-01-02 03:04:05', '2024-01-02 03:04:06', X'616263', 1.25), "
        "(2, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, X'00', NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, QUOTE(v) AS qv, QUOTE(c) AS qc, QUOTE(txt) AS qtxt, "
                   "QUOTE(i) AS qi, QUOTE(d) AS qd, QUOTE(y) AS qy, "
                   "QUOTE(dt) AS qdt, QUOTE(tm) AS qtm, QUOTE(dttm) AS qdttm, "
                   "QUOTE(ts) AS qts FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 2U,
            .context = "table quote values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, QUOTE(v) AS qv FROM t WHERE id >= 1 ORDER BY id ASC LIMIT 1",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 1U,
            .context = "table quote envelope",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, QUOTE(v) AS qv FROM t ORDER BY QUOTE(v), id",
            .columns = columns_order,
            .column_count = sizeof(columns_order) / sizeof(columns_order[0]),
            .values = values_order,
            .row_count = 2U,
            .context = "quote order expression",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE quote_update(id INT, src VARCHAR(20), outv VARCHAR(40))",
        NULL
    );
    failures += execute_ok(database, "INSERT INTO quote_update VALUES (1, 'a\\'b', '')", NULL);
    failures += execute_ok(database, "UPDATE quote_update SET outv = QUOTE(src)", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, outv FROM quote_update ORDER BY id",
            .columns = columns_update,
            .column_count = sizeof(columns_update) / sizeof(columns_update[0]),
            .values = values_update,
            .row_count = 1U,
            .context = "quote update assignment",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, QUOTE('row') AS qs, QUOTE(1.50) AS qd, QUOTE(NULL) AS qn "
                   "FROM t ORDER BY id LIMIT 1",
            .columns = columns_literal,
            .column_count = sizeof(columns_literal) / sizeof(columns_literal[0]),
            .values = values_literal,
            .row_count = 1U,
            .context = "table quote scalar literals",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT QUOTE(v), QUOTE(v) AS quoted FROM t WHERE id = 1",
            .columns = columns_labels,
            .column_count = sizeof(columns_labels) / sizeof(columns_labels[0]),
            .values = values_labels,
            .row_count = 1U,
            .context = "quote labels",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "quote preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen quote");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, QUOTE(v) AS qv FROM t WHERE id = 1",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .context = "reopen quote values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_quote_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, v VARCHAR(20), b VARBINARY(4), f DOUBLE)",
        NULL
    );
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 'abc', X'4142', 1.25)", NULL);
    failures += execute_error(
        database,
        "SELECT QUOTE()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'QUOTE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT QUOTE('a', 'b')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'QUOTE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT QUOTE() FROM t",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'QUOTE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT QUOTE(missing)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT QUOTE(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT QUOTE(CONCAT(v, id)) FROM t",
            .columns = (const char *const[]){"QUOTE(CONCAT(v, id))"},
            .column_count = 1U,
            .values = (const char *const[]){"'abc1'"},
            .row_count = 1U,
            .context = "nested concat quote argument",
        }
    );
    failures += execute_error(
        database,
        "SELECT QUOTE((SELECT 'abc')) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "QUOTE() supports only string, integer, DECIMAL, boolean, NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT QUOTE(b) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "row-scalar SELECT QUOTE() does not support binary string or BIT columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT QUOTE(f) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "row-scalar SELECT QUOTE() does not support approximate numeric columns",
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
        snprintf(path, path_size, "/tmp/mylite-quote-%s-%d.mylite", name, current_process_id());

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
    FILE *file = NULL;
    size_t read_count = 0U;

    file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        fprintf(stderr, "%s: failed to seek file\n", path);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    fclose(file);
    if (read_count != size) {
        fprintf(stderr, "%s: expected %zu bytes, read %zu\n", path, size, read_count);
        return 1;
    }
    return 0;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s: row %zu column %zu expected NULL, got [%s]\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: row %zu column %zu expected [%s], got [%s]\n",
            context,
            row,
            column,
            expected,
            actual == NULL ? "NULL" : actual
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
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
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
