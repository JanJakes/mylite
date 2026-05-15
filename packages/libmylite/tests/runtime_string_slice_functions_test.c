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

static int test_no_source_dual_and_do_string_slices(void);
static int test_table_backed_string_slices_and_reopen(void);
static int test_string_slice_diagnostics(void);
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

    failures += test_no_source_dual_and_do_string_slices();
    failures += test_table_backed_string_slices_and_reopen();
    failures += test_string_slice_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_string_slices(void) {
    static const char *const columns_no_source[] = {
        "l5",    "r3",    "l0",    "r0",        "ln",    "rn",        "lp", "rp",
        "lbig",  "rbig",  "lnull", "lnull_len", "rnull", "rnull_len", "le", "rface",
        "lboth", "rboth", "li",    "rnint",     "ldb",   "rmode",     "lt", "rf",
    };
    static const char *const values_no_source[] = {
        "fooba",
        "bar",
        "",
        "",
        "",
        "",
        "ab",
        "bc",
        "abc",
        "abc",
        NULL,
        NULL,
        NULL,
        NULL,
        "\xC3\xA9",
        "\xF0\x9F\x99\x82",
        "\xC3\xA9\xF0\x9F\x99\x82",
        "\xC3\xA9\xF0\x9F\x99\x82",
        "12",
        "345",
        "ap",
        "SUBSTITUTION",
        "1",
        "0",
    };
    static const char *const columns_dual[] = {"a", "b"};
    static const char *const values_dual[] = {"a", "c"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += execute_ok(database, "SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LEFT('foobarbar', 5) AS l5, RIGHT('foobarbar', 3) AS r3, "
                   "LEFT('abc', 0) AS l0, RIGHT('abc', 0) AS r0, LEFT('abc', -1) AS ln, "
                   "RIGHT('abc', -1) AS rn, LEFT('abc', +2) AS lp, RIGHT('abc', +2) AS rp, "
                   "LEFT('abc', 9) AS lbig, RIGHT('abc', 9) AS rbig, "
                   "LEFT(NULL, 1) AS lnull, LEFT('abc', NULL) AS lnull_len, "
                   "RIGHT(NULL, 1) AS rnull, RIGHT('abc', NULL) AS rnull_len, "
                   "LEFT('\xC3\xA9\xF0\x9F\x99\x82', 1) AS le, "
                   "RIGHT('\xC3\xA9\xF0\x9F\x99\x82', 1) AS rface, "
                   "LEFT('\xC3\xA9\xF0\x9F\x99\x82', 2) AS lboth, "
                   "RIGHT('\xC3\xA9\xF0\x9F\x99\x82', 2) AS rboth, "
                   "LEFT(12345, 2) AS li, RIGHT(-12345, 3) AS rnint, "
                   "LEFT(DATABASE(), 2) AS ldb, RIGHT(@@sql_mode, 12) AS rmode, "
                   "LEFT(TRUE, 1) AS lt, RIGHT(FALSE, 1) AS rf",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source string slice values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LEFT ('abc', 1) AS a, RIGHT ('abc', 1) AS b FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual string slice whitespace",
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
            .context = "row count after string slice select",
        }
    );

    failures += execute_ok(database, "DO LEFT('abc', 1), RIGHT(NULL, 1), LEFT(TRUE, 1)", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "string slice do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "string slice do rows");
        failures +=
            expect_int64(mylite_result_affected_rows(result), 0, "string slice do affected");
        failures +=
            expect_size(mylite_result_warning_count(result), 0U, "string slice do warnings");
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
            .context = "row count after string slice do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_string_slices_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "lv",
        "rv",
        "lc",
        "rc",
        "lt",
        "rt",
        "li",
        "rd",
        "ly",
        "ldt",
    };
    static const char *const values_table[] = {
        "1",
        "ab",
        "c",
        "a",
        "a",
        "hel",
        "lo",
        "12",
        "30",
        "2024",
        "2024-01-02",
        "2",
        "\xC3\xA9\xF0\x9F\x99\x82",
        "\xF0\x9F\x99\x82",
        "\xC3\xA9",
        "\xC3\xA9",
        "",
        "",
        "-7",
        "50",
        "1970",
        NULL,
        "3",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_limited[] = {"id", "rv"};
    static const char *const values_limited[] = {"3", NULL, "2", "\xF0\x9F\x99\x82"};
    static const char *const columns_branches[] = {"id", "l0", "rn", "lnull_len", "rnull_len"};
    static const char *const values_branches[] = {
        "1",
        "",
        "",
        NULL,
        NULL,
        "3",
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_labels[] = {"LEFT(v, 2)", "r"};
    static const char *const values_labels[] = {"ab", "c"};
    static const char *const columns_reopen[] = {"lv", "rv"};
    static const char *const values_reopen[] = {"ab", "c"};
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
        "id INT, v VARCHAR(20), c CHAR(5), txt TEXT, i INT, d DECIMAL(6,2), "
        "y YEAR, dt DATETIME, b VARBINARY(4), f DOUBLE, hidden INT"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t(id, v, c, txt, i, d, y, dt, b, f, hidden) VALUES "
        "(1, 'abc', 'a  ', 'hello', 12345, 12.30, 2024, '2024-01-02 13:29:17', "
        "X'4142', 1.5, 77), "
        "(2, '\xC3\xA9\xF0\x9F\x99\x82', '\xC3\xA9', '', -7, -4.50, 70, NULL, "
        "X'c389', -2.5, 5), "
        "(3, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)",
        NULL
    );
    failures += execute_ok(database, "ALTER TABLE t ALTER hidden SET INVISIBLE", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LEFT(v, 2) AS lv, RIGHT(v, 1) AS rv, "
                   "LEFT(c, 2) AS lc, RIGHT(c, 1) AS rc, LEFT(txt, 3) AS lt, "
                   "RIGHT(txt, 2) AS rt, LEFT(i, 2) AS li, RIGHT(d, 2) AS rd, "
                   "LEFT(y, 4) AS ly, LEFT(dt, 10) AS ldt FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .context = "table string slice values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, RIGHT(v, 1) AS rv FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table string slice row envelope",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LEFT(v, 0) AS l0, RIGHT(v, -1) AS rn, "
                   "LEFT(v, NULL) AS lnull_len, RIGHT(v, NULL) AS rnull_len "
                   "FROM t WHERE id IN (1, 3) ORDER BY id",
            .columns = columns_branches,
            .column_count = sizeof(columns_branches) / sizeof(columns_branches[0]),
            .values = values_branches,
            .row_count = 2U,
            .context = "table string slice null and nonpositive branches",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LEFT(v, 2), RIGHT(v, 1) AS r FROM t WHERE id = 1",
            .columns = columns_labels,
            .column_count = sizeof(columns_labels) / sizeof(columns_labels[0]),
            .values = values_labels,
            .row_count = 1U,
            .context = "string slice labels",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "string slice preamble unchanged"
    );

    mylite_close(database);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen string slice file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LEFT(v, 2) AS lv, RIGHT(v, 1) AS rv FROM t WHERE id = 1",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .context = "string slice reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_string_slice_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, v VARCHAR(20), b VARBINARY(4), f DOUBLE)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES (1, 'abc', X'6162', 1.5), "
        "(2, '\xC3\xA9', X'c389', -2.5)",
        NULL
    );
    failures += execute_error(
        database,
        "SELECT LEFT()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT RIGHT('a', 1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT LEFT(missing, 1)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT LEFT(missing, 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT LEFT('abc', '2')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "string slice functions support only integer, boolean, and NULL length literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT LEFT(CAST('ABC' AS BINARY), 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string slice functions support only string, integer, boolean, NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT LEFT(b, 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string slice functions do not support binary columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT LEFT(f, 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string slice functions do not support approximate numeric columns",
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
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-string-slice-functions-%s-%d.mylite",
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
    int failures = 0;

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek file\n", path);
        failures = 1;
    } else if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read file\n", path);
        failures = 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "%s: failed to close file\n", path);
        failures = 1;
    }
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
        fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }
    return 0;
}
