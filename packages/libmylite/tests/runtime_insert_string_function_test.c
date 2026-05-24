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

static int test_no_source_dual_and_do_insert_function(void);
static int test_table_backed_insert_function_and_reopen(void);
static int test_insert_function_diagnostics(void);
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

    failures += test_no_source_dual_and_do_insert_function();
    failures += test_table_backed_insert_function_and_reopen();
    failures += test_insert_function_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_insert_function(void) {
    static const char *const columns_no_source[] = {
        "basic",
        "pos_zero",
        "pos_negative",
        "pos_high",
        "zero_length",
        "negative_length",
        "long_length",
        "empty_source",
        "null_source",
        "null_position",
        "null_length",
        "null_replacement",
        "multibyte",
        "numeric_text",
        "boolean_text",
        "session_text",
        "system_text",
    };
    static const char *const values_no_source[] = {
        "QuWhattic",
        "Quadratic",
        "Quadratic",
        "Quadratic",
        "QuWhatadratic",
        "QuWhat",
        "QuWhat",
        "",
        NULL,
        NULL,
        NULL,
        NULL,
        "éXb",
        "1945",
        "0",
        "db:app",
        "x",
    };
    static const char *const columns_dual[] = {"INSERT('abc',2,1,'X')", "inserted"};
    static const char *const values_dual[] = {"aXc", "aYc"};
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
            .sql = "SELECT INSERT('Quadratic', 3, 4, 'What') AS basic, "
                   "INSERT('Quadratic', 0, 4, 'What') AS pos_zero, "
                   "INSERT('Quadratic', -1, 4, 'What') AS pos_negative, "
                   "INSERT('Quadratic', 99, 4, 'What') AS pos_high, "
                   "INSERT('Quadratic', 3, 0, 'What') AS zero_length, "
                   "INSERT('Quadratic', 3, -1, 'What') AS negative_length, "
                   "INSERT('Quadratic', 3, 99, 'What') AS long_length, "
                   "INSERT('', 1, 1, 'What') AS empty_source, "
                   "INSERT(NULL, 1, 1, 'What') AS null_source, "
                   "INSERT('abc', NULL, 1, 'X') AS null_position, "
                   "INSERT('abc', 1, NULL, 'X') AS null_length, "
                   "INSERT('abc', 1, 1, NULL) AS null_replacement, "
                   "INSERT('é🙂b', 2, 1, 'X') AS multibyte, "
                   "INSERT(12345, 2, 2, 9) AS numeric_text, "
                   "INSERT(TRUE, 1, 1, FALSE) AS boolean_text, "
                   "INSERT(DATABASE(), 1, 0, 'db:') AS session_text, "
                   "INSERT(@@warning_count, 1, 1, 'x') AS system_text",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source INSERT() values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT INSERT('abc',2,1,'X'), INSERT ('abc', 2, 1, 'Y') AS inserted "
                   "FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual INSERT() labels",
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
            .context = "row count after INSERT() select",
        }
    );

    failures +=
        execute_ok(database, "DO INSERT('abc', 2, 1, 'X'), INSERT(NULL, 1, 1, 'X')", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "INSERT() DO columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "INSERT() DO rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "INSERT() DO affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "INSERT() DO warnings");
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
            .context = "row count after INSERT() DO",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_insert_function_and_reopen(void) {
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
        "1",
        "aXef",
        "aXef",
        "heLLo",
        "1x45",
        "1x30",
        "20x4",
        "2024/01-02",
        "01.02:03",
        "2024/01-02 13:29:17",
        "2024/01-02 13:29:17",
        "2",
        "AX",
        "BX",
        "x",
        NULL,
        "-x50",
        "19x0",
        NULL,
        NULL,
        NULL,
        NULL,
        "3",
        NULL,
        NULL,
        NULL,
        "-x",
        NULL,
        NULL,
        "2024/12-31",
        "00.00:00",
        "2024/12-31 23:59:58",
        "2024/12-31 23:59:58",
    };
    static const char *const columns_limited[] = {"id", "sv", "session_arg", "system_arg"};
    static const char *const values_limited[] = {
        "3",
        NULL,
        NULL,
        NULL,
        "2",
        "AX",
        "AappaAa",
        "A0Aa",
    };
    static const char *const columns_reopen[] = {"id", "sv"};
    static const char *const values_reopen[] = {"1", "aXef", "2", "AX"};
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
        "id INT, v VARCHAR(20), c CHAR(8), txt TEXT, i INT, d DECIMAL(6,2), y YEAR, "
        "dt DATE, tm TIME, dttm DATETIME, ts TIMESTAMP NULL, b VARBINARY(8), f DOUBLE"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 'abcdef', 'abcdef', 'hello', 12345, 12.30, 2024, '2024-01-02', '01:02:03', "
        "'2024-01-02 13:29:17', '2024-01-02 13:29:17', X'616263', 1.25), "
        "(2, 'AaAa', 'BbBb', 'x', NULL, -4.50, 1970, NULL, NULL, NULL, NULL, X'00', -2.5), "
        "(3, NULL, NULL, NULL, -77, NULL, NULL, '2024-12-31', '00:00:00', "
        "'2024-12-31 23:59:58', '2024-12-31 23:59:58', NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, INSERT(v, 2, 3, 'X') AS sv, INSERT(c, 2, 3, 'X') AS sc, "
                   "INSERT(txt, 3, 2, 'LL') AS stxt, INSERT(i, 2, 2, 'x') AS si, "
                   "INSERT(d, 2, 2, 'x') AS sd, INSERT(y, 3, 1, 'x') AS sy, "
                   "INSERT(dt, 5, 1, '/') AS sdt, INSERT(tm, 3, 1, '.') AS stm, "
                   "INSERT(dttm, 5, 1, '/') AS sdttm, INSERT(ts, 5, 1, '/') AS sts "
                   "FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .context = "table INSERT() values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, INSERT(v, 2, 3, 'X') AS sv, "
                   "INSERT(v, 2, 0, DATABASE()) AS session_arg, "
                   "INSERT(v, 2, 1, @@warning_count) AS system_arg FROM t WHERE id >= 1 "
                   "ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table INSERT() envelope",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "INSERT() preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen INSERT()");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, INSERT(v, 2, 3, 'X') AS sv FROM t WHERE id <= 2 ORDER BY id",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 2U,
            .context = "reopen INSERT() values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_insert_function_diagnostics(void) {
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
        "SELECT INSERT()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT INSERT('a', 1, 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT INSERT('a', 1, 1, 'b', 'c')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT INSERT(v, 2, 1, missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT INSERT('abc', '2', 1, 'X')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INSERT() position supports only integer, boolean, and NULL literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT INSERT('abc', 2, 1.5, 'X')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INSERT() length supports only integer, boolean, and NULL literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT INSERT(CONCAT('a', 'b'), 2, 1, 'X')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INSERT() supports only string, integer, boolean, NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT INSERT(CONCAT(v, id), 2, 1, 'X') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INSERT() supports only string, integer, boolean, NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT INSERT(b, 2, 1, 'X') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "row-scalar SELECT INSERT() does not support binary string or BIT columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT INSERT(f, 2, 1, 'X') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "row-scalar SELECT INSERT() does not support approximate numeric columns",
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
        snprintf(path, path_size, "/tmp/mylite-insert-fn-%s-%d.mylite", name, current_process_id());

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

    return expect_text(actual, expected, context);
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
    if (actual == NULL || expected == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
