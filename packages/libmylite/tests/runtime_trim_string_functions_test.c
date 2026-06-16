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

static int test_no_source_dual_and_do_trim_functions(void);
static int test_table_backed_trim_functions_and_reopen(void);
static int test_trim_diagnostics(void);
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

    failures += test_no_source_dual_and_do_trim_functions();
    failures += test_table_backed_trim_functions_and_reopen();
    failures += test_trim_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_trim_functions(void) {
    static const char *const columns_no_source[] = {
        "lt",        "rt",         "bt",           "tnull",          "lnull",        "rnull",
        "int_value", "neg_value",  "true_value",   "false_value",    "lead_x",       "both_x",
        "trail_xyz", "implicit_x", "lead_default", "trail_default",  "both_default", "multi",
        "partial",   "overlap",    "empty_remove", "numeric_remove", "utf8_remove",  "mode_trim",
    };
    static const char *const values_no_source[] = {
        "barbar", "barbar", "bar",    NULL,  NULL,   NULL,  "123",   "-7",
        "1",      "0",      "barxxx", "bar", "barx", "bar", "bar  ", "  bar",
        "bar",    "abc",    "xabc",   "a",   "abc",  "2",   "abc",   "NO_ENGINE_SUBSTITUTION",
    };
    static const char *const columns_dual[] = {"a", "b", "c"};
    static const char *const values_dual[] = {"a", "b", "c"};
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
            .sql = "SELECT LTRIM('  barbar') AS lt, RTRIM('barbar   ') AS rt, "
                   "TRIM('  bar   ') AS bt, TRIM(NULL) AS tnull, LTRIM(NULL) AS lnull, "
                   "RTRIM(NULL) AS rnull, LTRIM(123) AS int_value, "
                   "RTRIM(-7) AS neg_value, TRIM(TRUE) AS true_value, "
                   "TRIM(FALSE) AS false_value, TRIM(LEADING 'x' FROM 'xxxbarxxx') AS lead_x, "
                   "TRIM(BOTH 'x' FROM 'xxxbarxxx') AS both_x, "
                   "TRIM(TRAILING 'xyz' FROM 'barxxyz') AS trail_xyz, "
                   "TRIM('x' FROM 'xxxbarxxx') AS implicit_x, "
                   "TRIM(LEADING FROM '  bar  ') AS lead_default, "
                   "TRIM(TRAILING FROM '  bar  ') AS trail_default, "
                   "TRIM(BOTH FROM '  bar  ') AS both_default, "
                   "TRIM('xyz' FROM 'xyzxyzabcxyz') AS multi, "
                   "TRIM('xy' FROM 'xyxabcxy') AS partial, TRIM('aa' FROM 'aaaaa') AS overlap, "
                   "TRIM('' FROM 'abc') AS empty_remove, TRIM(1 FROM 1112111) AS numeric_remove, "
                   "TRIM('\xC3\xA9' FROM '\xC3\xA9\xC3\xA9"
                   "abc"
                   "\xC3\xA9') AS utf8_remove, TRIM(@@sql_mode) AS mode_trim",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source trim values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LTRIM ('  a') AS a, RTRIM ('b  ') AS b, TRIM (' c ') AS c FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual trim whitespace",
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
            .context = "row count after trim select",
        }
    );

    failures += execute_ok(
        database,
        "DO LTRIM('  a'), RTRIM(NULL), TRIM('  a  '), TRIM('x' FROM 'xxaxx')",
        &result
    );
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "trim do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "trim do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "trim do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "trim do warnings");
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
            .context = "row count after trim do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_trim_functions_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "tv",
        "lv",
        "rv",
        "tc",
        "ltxt",
        "rtxt",
        "ti",
        "td",
        "ty",
        "tdt",
    };
    static const char *const values_table[] = {
        "1",
        "AbC",
        "AbC  ",
        "  AbC",
        "A",
        "HeLLo  ",
        "  HeLLo",
        "123",
        "12.30",
        "2024",
        "2024-01-02 13:29:17",
        "2",
        "xYz",
        "xYz",
        "xYz",
        "b",
        "",
        "",
        "-7",
        "-4.50",
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
    static const char *const columns_explicit[] =
        {"both_space", "leading_space", "trailing_space", "literal_trim", "empty_remove"};
    static const char *const values_explicit[] = {"AbC", "AbC  ", "  AbC", "a", "  AbC  "};
    static const char *const columns_limited[] = {"id", "trimmed_v"};
    static const char *const values_limited[] = {"3", NULL, "2", "xYz"};
    static const char *const columns_qualified[] = {"tv_alias", "lv_alias", "rv_alias"};
    static const char *const values_qualified[] = {"AbC", "AbC  ", "  AbC"};
    static const char *const columns_labels[] = {"TRIM(v)", "ltrim_alias", "RTRIM(v)"};
    static const char *const values_labels[] = {"AbC", "AbC  ", "  AbC"};
    static const char *const columns_reopen[] = {"tv", "lv", "rv"};
    static const char *const values_reopen[] = {"AbC", "AbC  ", "  AbC"};
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
        "y YEAR, dt DATETIME, b VARBINARY(4), bitcol BIT(3), f DOUBLE"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '  AbC  ', 'A  ', '  HeLLo  ', 123, 12.30, 2024, "
        "'2024-01-02 13:29:17', X'6162', b'101', 1.5), "
        "(2, 'xYz', 'b   ', '', -7, -4.50, 70, NULL, X'00ff', b'010', -2.5), "
        "(3, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, TRIM(v) AS tv, LTRIM(v) AS lv, RTRIM(v) AS rv, "
                   "TRIM(c) AS tc, LTRIM(txt) AS ltxt, RTRIM(txt) AS rtxt, "
                   "TRIM(i) AS ti, TRIM(d) AS td, TRIM(y) AS ty, TRIM(dt) AS tdt "
                   "FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .context = "table trim values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TRIM(' ' FROM v) AS both_space, "
                   "TRIM(LEADING ' ' FROM v) AS leading_space, "
                   "TRIM(TRAILING ' ' FROM v) AS trailing_space, "
                   "TRIM('x' FROM 'xxaxx') AS literal_trim, "
                   "TRIM('' FROM v) AS empty_remove FROM t WHERE id = 1",
            .columns = columns_explicit,
            .column_count = sizeof(columns_explicit) / sizeof(columns_explicit[0]),
            .values = values_explicit,
            .row_count = 1U,
            .context = "table explicit trim values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, TRIM(v) AS trimmed_v FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table trim row envelope",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TRIM(tt.v) AS tv_alias, LTRIM(tt.v) AS lv_alias, "
                   "RTRIM(tt.v) AS rv_alias FROM t AS tt WHERE tt.id = 1",
            .columns = columns_qualified,
            .column_count = sizeof(columns_qualified) / sizeof(columns_qualified[0]),
            .values = values_qualified,
            .row_count = 1U,
            .context = "alias-qualified trim projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TRIM(v), LTRIM(v) AS ltrim_alias, RTRIM(v) FROM t WHERE id = 1",
            .columns = columns_labels,
            .column_count = sizeof(columns_labels) / sizeof(columns_labels[0]),
            .values = values_labels,
            .row_count = 1U,
            .context = "trim labels",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "trim preamble unchanged"
    );

    mylite_close(database);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen trim file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TRIM(v) AS tv, LTRIM(v) AS lv, RTRIM(v) AS rv FROM t WHERE id = 1",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .context = "trim reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_trim_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, v VARCHAR(20), txt TEXT, b VARBINARY(4), bitcol BIT(3), f DOUBLE)",
        NULL
    );
    failures +=
        execute_ok(database, "INSERT INTO t VALUES (1, ' abc ', 'x', X'6162', b'101', 1.5)", NULL);
    failures += execute_error(
        database,
        "SELECT LTRIM()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'LTRIM'",
        }
    );
    failures += execute_error(
        database,
        "SELECT RTRIM('a', 'b')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'RTRIM'",
        }
    );
    failures += execute_error(
        database,
        "SELECT TRIM()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT TRIM('a', 'b')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT TRIM(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT TRIM(b) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "trim functions do not support binary columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT TRIM(bitcol) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "trim functions do not support binary columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT TRIM(f) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "trim functions do not support approximate numeric columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT TRIM(v FROM txt) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "trim functions support only scalar string, integer, boolean, NULL",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TRIM(CONCAT(v)) FROM t",
            .columns = (const char *const[]){"TRIM(CONCAT(v))"},
            .column_count = 1U,
            .values = (const char *const[]){"abc"},
            .row_count = 1U,
            .context = "nested concat trim argument",
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
        "/tmp/mylite-trim-string-functions-%s-%d.mylite",
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
