#include "mylite_test_support.h"

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
    mysql_error_native_function_parameter_count = 1582,
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

static int test_no_source_dual_and_do_soundex(void);
static int test_table_backed_soundex_and_reopen(void);
static int test_soundex_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
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
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_no_source_dual_and_do_soundex();
    failures += test_table_backed_soundex_and_reopen();
    failures += test_soundex_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_soundex(void) {
    static const char *const columns_no_source[] = {
        "hello",
        "quadratic",
        "empty_value",
        "digits",
        "null_value",
        "int_value",
        "true_value",
        "punctuated",
        "leading_value",
        "duplicate_value",
        "alphabet",
        "utf8_value",
        "emoji_value",
        "session_value",
        "system_value",
    };
    static const char *const values_no_source[] = {
        "H400",
        "Q36324",
        "",
        "",
        NULL,
        "",
        "",
        "O165",
        "A120",
        "B000",
        "A12312451262312",
        "\303\251246",
        "\360\237\231\202123",
        "A100",
        "",
    };
    static const char *const columns_dual[] = {"a", "b"};
    static const char *const values_dual[] = {"A120", "A120"};
    static const char *const columns_sounds_like[] = {
        "match_value",
        "miss_value",
        "null_left",
        "null_right",
        "true_value",
    };
    static const char *const values_sounds_like[] = {"1", "0", NULL, NULL, "1"};
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
            .sql = "SELECT SOUNDEX('Hello') AS hello, "
                   "SOUNDEX('Quadratically') AS quadratic, "
                   "SOUNDEX('') AS empty_value, SOUNDEX('123') AS digits, "
                   "SOUNDEX(NULL) AS null_value, SOUNDEX(12345) AS int_value, "
                   "SOUNDEX(TRUE) AS true_value, SOUNDEX('O''Brien') AS punctuated, "
                   "SOUNDEX('  abc') AS leading_value, SOUNDEX('BHB') AS duplicate_value, "
                   "SOUNDEX('abcdefghijklmnopqrstuvwxyz') AS alphabet, "
                   "SOUNDEX('\303\251clair') AS utf8_value, "
                   "SOUNDEX('\360\237\231\202bcd') AS emoji_value, "
                   "SOUNDEX(DATABASE()) AS session_value, "
                   "SOUNDEX(@@warning_count) AS system_value",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source soundex values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SOUNDEX ('abc') AS a, SOUNDEX(('abc')) AS b FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual soundex whitespace",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 'mood' SOUNDS LIKE 'mud' AS match_value, "
                   "'mood' SOUNDS LIKE 'xyz' AS miss_value, "
                   "NULL SOUNDS LIKE 'abc' AS null_left, "
                   "'abc' SOUNDS LIKE NULL AS null_right, "
                   "TRUE SOUNDS LIKE '1' AS true_value",
            .columns = columns_sounds_like,
            .column_count = sizeof(columns_sounds_like) / sizeof(columns_sounds_like[0]),
            .values = values_sounds_like,
            .row_count = 1U,
            .context = "no-source sounds like values",
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
            .context = "row count after soundex select",
        }
    );

    failures +=
        execute_ok(database, "DO SOUNDEX('abc'), SOUNDEX(NULL), 'mood' SOUNDS LIKE 'mud'", &result);
    if (failures == 0) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 0U, "soundex do columns");
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "soundex do rows");
        failures +=
            mylite_test_expect_int64(mylite_result_affected_rows(result), 0, "soundex do affected");
        failures +=
            mylite_test_expect_size(mylite_result_warning_count(result), 0U, "soundex do warnings");
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
            .context = "row count after soundex do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_soundex_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "sv",
        "sc",
        "stxt",
        "si",
        "sd",
        "sy",
        "sdt",
        "stm",
        "sdttm",
        "sts",
    };
    static const char *const values_table[] = {
        "1", "R163", "A2613", "Q36324", "",   "",   "",   "",   "",   "",   "",
        "2", NULL,   NULL,    NULL,     NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    };
    static const char *const columns_limited[] = {"id", "sv"};
    static const char *const values_limited[] = {"1", "R163"};
    static const char *const columns_order[] = {"id", "sv"};
    static const char *const values_order[] = {"2", NULL, "1", "R163"};
    static const char *const columns_update[] = {"id", "outv"};
    static const char *const values_update[] = {"1", "R163"};
    static const char *const columns_sounds_like[] = {
        "id",
        "sv_match",
        "txt_match",
        "i_match",
        "d_match",
    };
    static const char *const values_sounds_like[] = {
        "1",
        "1",
        "1",
        "1",
        "1",
        "2",
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_predicate[] = {"id"};
    static const char *const values_predicate[] = {"1"};
    static const char *const columns_labels[] = {"SOUNDEX(v)", "code"};
    static const char *const values_labels[] = {"R163", "R163"};
    static const char *const columns_reopen[] = {"id", "sv"};
    static const char *const values_reopen[] = {"1", "R163"};
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
        "id INT, v VARCHAR(32), c CHAR(16), txt TEXT, i INT, d DECIMAL(6,2), y YEAR, "
        "dt DATE, tm TIME, dttm DATETIME, ts TIMESTAMP NULL, b VARBINARY(8), f DOUBLE"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 'Robert', 'Ashcraft', 'Quadratically', 123, -12.30, 2024, "
        "'2024-01-02', '01:02:03', '2024-01-02 03:04:05', "
        "'2024-01-02 03:04:05', X'616263', 1.25), "
        "(2, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, X'00', NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, SOUNDEX(v) AS sv, SOUNDEX(c) AS sc, "
                   "SOUNDEX(txt) AS stxt, SOUNDEX(i) AS si, SOUNDEX(d) AS sd, "
                   "SOUNDEX(y) AS sy, SOUNDEX(dt) AS sdt, SOUNDEX(tm) AS stm, "
                   "SOUNDEX(dttm) AS sdttm, SOUNDEX(ts) AS sts FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 2U,
            .context = "table soundex values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, SOUNDEX(v) AS sv FROM t WHERE id >= 1 ORDER BY id ASC LIMIT 1",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 1U,
            .context = "table soundex envelope",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, SOUNDEX(v) AS sv FROM t ORDER BY SOUNDEX(v), id",
            .columns = columns_order,
            .column_count = sizeof(columns_order) / sizeof(columns_order[0]),
            .values = values_order,
            .row_count = 2U,
            .context = "soundex order expression",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE soundex_update(id INT, src VARCHAR(20), outv VARCHAR(20))",
        NULL
    );
    failures += execute_ok(database, "INSERT INTO soundex_update VALUES (1, 'Robert', '')", NULL);
    failures += execute_ok(database, "UPDATE soundex_update SET outv = SOUNDEX(src)", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, outv FROM soundex_update ORDER BY id",
            .columns = columns_update,
            .column_count = sizeof(columns_update) / sizeof(columns_update[0]),
            .values = values_update,
            .row_count = 1U,
            .context = "soundex update assignment",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, v SOUNDS LIKE 'Rupert' AS sv_match, "
                   "txt SOUNDS LIKE 'Quadratically' AS txt_match, "
                   "i SOUNDS LIKE '123' AS i_match, "
                   "d SOUNDS LIKE '-12.30' AS d_match FROM t ORDER BY id",
            .columns = columns_sounds_like,
            .column_count = sizeof(columns_sounds_like) / sizeof(columns_sounds_like[0]),
            .values = values_sounds_like,
            .row_count = 2U,
            .context = "table sounds like values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE v SOUNDS LIKE 'Rupert' ORDER BY id",
            .columns = columns_predicate,
            .column_count = sizeof(columns_predicate) / sizeof(columns_predicate[0]),
            .values = values_predicate,
            .row_count = 1U,
            .context = "table sounds like predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SOUNDEX(v), SOUNDEX(v) AS code FROM t WHERE id = 1",
            .columns = columns_labels,
            .column_count = sizeof(columns_labels) / sizeof(columns_labels[0]),
            .values = values_labels,
            .row_count = 1U,
            .context = "soundex labels",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "soundex preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen soundex");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, SOUNDEX(v) AS sv FROM t WHERE id = 1",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .context = "reopen soundex values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_soundex_diagnostics(void) {
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
        "SELECT SOUNDEX()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'SOUNDEX'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SOUNDEX('a', 'b')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'SOUNDEX'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SOUNDEX",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor-backed table reads",
        }
    );
    failures += execute_error(
        database,
        "SELECT SOUNDEX(missing)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT missing SOUNDS LIKE 'abc'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SOUNDEX(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SOUNDEX(CONCAT(v, id)) FROM t",
            .columns = (const char *const[]){"SOUNDEX(CONCAT(v, id))"},
            .column_count = 1U,
            .values = (const char *const[]){"A120"},
            .row_count = 1U,
            .context = "nested concat soundex argument",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SOUNDEX((SELECT 'abc')) FROM t",
            .columns = (const char *const[]){"SOUNDEX((SELECT 'abc'))"},
            .column_count = 1U,
            .values = (const char *const[]){"A120"},
            .row_count = 1U,
            .context = "scalar-subquery soundex argument",
        }
    );
    failures += execute_error(
        database,
        "SELECT SOUNDEX(b) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "row-scalar SELECT SOUNDEX() does not support binary string or BIT columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT b SOUNDS LIKE 'abc' FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "row-scalar SELECT SOUNDEX() does not support binary string or BIT columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT SOUNDEX(f) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "row-scalar SELECT SOUNDEX() does not support approximate numeric columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT f SOUNDS LIKE 'abc' FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "row-scalar SELECT SOUNDEX() does not support approximate numeric columns",
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
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += mylite_test_expect_text(
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

    if (mylite_test_make_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += mylite_test_expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
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

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (actual == NULL || expected == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }
    return 0;
}
