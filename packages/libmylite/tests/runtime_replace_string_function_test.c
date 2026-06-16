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

static int test_no_source_dual_and_do_replace(void);
static int test_table_backed_replace_and_reopen(void);
static int test_replace_diagnostics(void);
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

    failures += test_no_source_dual_and_do_replace();
    failures += test_table_backed_replace_and_reopen();
    failures += test_replace_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_replace(void) {
    static const char *const columns_no_source[] = {
        "site",
        "overlap",
        "case_sensitive",
        "empty_search",
        "empty_value",
        "n1",
        "n2",
        "n3",
        "numeric_value",
        "true_value",
        "false_value",
        "session_value",
        "schema_value",
        "system_value",
    };
    static const char *const values_no_source[] = {
        "WwWwWw.mysql.com",
        "bb",
        "AxAx",
        "abc",
        "",
        NULL,
        NULL,
        NULL,
        "1XX45",
        "9",
        "7",
        "db",
        "schema",
        "zero",
    };
    static const char *const columns_dual[] = {"a", "b"};
    static const char *const values_dual[] = {"Abc", "aBc"};
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
            .sql = "SELECT REPLACE('www.mysql.com', 'w', 'Ww') AS site, "
                   "REPLACE('aaaa', 'aa', 'b') AS overlap, "
                   "REPLACE('AaAa', 'a', 'x') AS case_sensitive, "
                   "REPLACE('abc', '', 'x') AS empty_search, REPLACE('', '', 'x') AS empty_value, "
                   "REPLACE(NULL, 'a', 'b') AS n1, REPLACE('abc', NULL, 'b') AS n2, "
                   "REPLACE('abc', 'a', NULL) AS n3, REPLACE(12345, 23, 'XX') AS numeric_value, "
                   "REPLACE(TRUE, 1, 9) AS true_value, REPLACE(FALSE, 0, 7) AS false_value, "
                   "REPLACE(DATABASE(), DATABASE(), 'db') AS session_value, "
                   "REPLACE(SCHEMA(), SCHEMA(), 'schema') AS schema_value, "
                   "REPLACE(@@warning_count, 0, 'zero') AS system_value",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source replace values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT REPLACE ('abc', 'a', 'A') AS a, "
                   "REPLACE(('abc'), ('b'), ('B')) AS b FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual replace whitespace",
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
            .context = "row count after replace select",
        }
    );

    failures +=
        execute_ok(database, "DO REPLACE('abc', 'b', 'B'), REPLACE(NULL, 'a', 'b')", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "replace do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "replace do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "replace do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "replace do warnings");
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
            .context = "row count after replace do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_replace_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "rv",
        "rc",
        "rtxt",
        "ri",
        "rd",
        "ry",
        "rdt",
        "rtm",
        "rdttm",
        "rts",
    };
    static const char *const values_table[] = {
        "1",
        "XbcXbc",
        "X",
        "heLLo",
        "1x345",
        "1x.30",
        "x0x4",
        "2024/01/02",
        "01.02.03",
        "2024/01/02 13:29:17",
        "2024/01/02 13:29:17",
        "2",
        "AXAX",
        "B",
        "x",
        NULL,
        "-4.50",
        "1970",
        NULL,
        NULL,
        NULL,
        NULL,
        "3",
        NULL,
        NULL,
        NULL,
        "-77",
        NULL,
        NULL,
        "2024/12/31",
        "00.00.00",
        "2024/12/31 23:59:58",
        "2024/12/31 23:59:58",
    };
    static const char *const columns_limited[] = {"id", "rv", "session_arg", "system_arg"};
    static const char *const values_limited[] = {
        "3",
        NULL,
        NULL,
        NULL,
        "2",
        "AxAx",
        "AappAapp",
        "A0A0",
    };
    static const char *const columns_labels[] = {"REPLACE(v, 'a', 'x')", "replaced"};
    static const char *const values_labels[] = {"xbcxbc", "AbcAbc"};
    static const char *const columns_reopen[] = {"id", "rv"};
    static const char *const values_reopen[] = {"1", "xbcxbc", "2", "AxAx"};
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
        "(1, 'abcabc', 'a  ', 'hello', 12345, 12.30, 2024, '2024-01-02', '01:02:03', "
        "'2024-01-02 13:29:17', '2024-01-02 13:29:17', X'616263', 1.25), "
        "(2, 'AaAa', 'B', 'x', NULL, -4.50, 70, NULL, NULL, NULL, NULL, X'00', -2.5), "
        "(3, NULL, NULL, NULL, -77, NULL, NULL, '2024-12-31', '00:00:00', "
        "'2024-12-31 23:59:58', '2024-12-31 23:59:58', NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, REPLACE(v, 'a', 'X') AS rv, REPLACE(c, 'a', 'X') AS rc, "
                   "REPLACE(txt, 'l', 'L') AS rtxt, REPLACE(i, 2, 'x') AS ri, "
                   "REPLACE(d, '2', 'x') AS rd, REPLACE(y, '2', 'x') AS ry, "
                   "REPLACE(dt, '-', '/') AS rdt, REPLACE(tm, ':', '.') AS rtm, "
                   "REPLACE(dttm, '-', '/') AS rdttm, REPLACE(ts, '-', '/') AS rts "
                   "FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .context = "table replace values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, REPLACE(v, 'a', 'x') AS rv, "
                   "REPLACE(v, 'a', DATABASE()) AS session_arg, "
                   "REPLACE(v, 'a', @@warning_count) AS system_arg FROM t WHERE id >= 1 "
                   "ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table replace envelope",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT REPLACE(v, 'a', 'x'), REPLACE(v, 'a', 'A') AS replaced "
                   "FROM t WHERE id = 1",
            .columns = columns_labels,
            .column_count = sizeof(columns_labels) / sizeof(columns_labels[0]),
            .values = values_labels,
            .row_count = 1U,
            .context = "replace labels",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "replace preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen replace");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, REPLACE(v, 'a', 'x') AS rv FROM t WHERE id <= 2 ORDER BY id",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 2U,
            .context = "reopen replace values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_replace_diagnostics(void) {
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
        "SELECT REPLACE()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT REPLACE(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT REPLACE(1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT REPLACE(1, 2, 3, 4)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT REPLACE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT REPLACE(v, 'a', missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT REPLACE(CONCAT(v, id), 'a', 'x') FROM t",
            .columns = (const char *const[]){"REPLACE(CONCAT(v, id), 'a', 'x')"},
            .column_count = 1U,
            .values = (const char *const[]){"xbc1"},
            .row_count = 1U,
            .context = "nested concat replace argument",
        }
    );
    failures += execute_error(
        database,
        "SELECT REPLACE((SELECT 'abc'), 'a', 'x') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "REPLACE() supports only string, integer, boolean, NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT REPLACE(b, 'A', 'x') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "row-scalar SELECT REPLACE() does not support binary string or BIT columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT REPLACE(f, '1', 'x') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "row-scalar SELECT REPLACE() does not support approximate numeric columns",
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
        snprintf(path, path_size, "/tmp/mylite-replace-%s-%d.mylite", name, current_process_id());

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
