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

static int test_no_source_dual_and_do_case_functions(void);
static int test_table_backed_case_functions_and_reopen(void);
static int test_string_case_diagnostics(void);
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

    failures += test_no_source_dual_and_do_case_functions();
    failures += test_table_backed_case_functions_and_reopen();
    failures += test_string_case_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_case_functions(void) {
    static const char *const columns_no_source[] = {
        "lower_ascii",
        "lcase_ascii",
        "upper_ascii",
        "null_value",
        "int_value",
        "negative_value",
        "true_value",
        "false_value",
        "db_upper",
        "mode_lower",
        "mode_upper",
        "warnings",
    };
    static const char *const values_no_source[] = {
        "abc",
        "a1-z",
        "ABC",
        NULL,
        "123",
        "-7",
        "1",
        "0",
        "APP",
        "no_engine_substitution",
        "NO_ENGINE_SUBSTITUTION",
        "0",
    };
    static const char *const columns_dual[] = {"a", "b"};
    static const char *const values_dual[] = {"abc", "ABC"};
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
            .sql = "SELECT LOWER('ABC') AS lower_ascii, LCASE('A1-Z') AS lcase_ascii, "
                   "UPPER('abc') AS upper_ascii, UCASE(NULL) AS null_value, "
                   "LOWER(123) AS int_value, UPPER(-7) AS negative_value, "
                   "LOWER(TRUE) AS true_value, UPPER(FALSE) AS false_value, "
                   "UPPER(DATABASE()) AS db_upper, LOWER(@@sql_mode) AS mode_lower, "
                   "UPPER(@@sql_mode) AS mode_upper, @@warning_count AS warnings",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source string case values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LOWER ('ABC') AS a, UCASE ('abc') AS b FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual string case whitespace",
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
            .context = "row count after string case select",
        }
    );

    failures += execute_ok(database, "DO LOWER('ABC'), LCASE(NULL), UPPER(TRUE)", &result);
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            0U,
            "string case do columns"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "string case do rows");
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "string case do affected"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            0U,
            "string case do warnings"
        );
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
            .context = "row count after string case do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_case_functions_and_reopen(void) {
    static const char *const columns_table[] = {"id", "lv", "uc", "ln", "un", "ld", "uy", "ldt"};
    static const char *const values_table[] = {
        "1", "alpha", "AB", "mixed", "-7", "12.30", "2024", "2008-01-02 13:29:17",
        "2", "xyz",   "XY", "",      "0",  "0.42",  "1999", "2024-03-04 05:06:07",
        "3", NULL,    NULL, NULL,    NULL, NULL,    NULL,   NULL,
    };
    static const char *const columns_limited[] = {"id", "uv"};
    static const char *const values_limited[] = {"3", NULL, "2", "XYZ"};
    static const char *const columns_order_lower[] = {"id", "lv"};
    static const char *const values_order_lower[] = {"3", NULL, "1", "alpha", "2", "xyz"};
    static const char *const columns_order_upper[] = {"id", "uv"};
    static const char *const values_order_upper[] = {"3", NULL, "1", "ALPHA", "2", "XYZ"};
    static const char *const columns_update[] = {"id", "outv"};
    static const char *const values_update[] = {"1", "alpha", "2", "XYZ"};
    static const char *const columns_labels[] = {"LOWER(v)", "upper_alias", "UCASE(txt)"};
    static const char *const values_labels[] = {"alpha", "AB", "MIXED"};
    static const char *const columns_reopen[] = {"lv", "uv"};
    static const char *const values_reopen[] = {"alpha", "AB"};
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
        "id INT, v VARCHAR(20), c CHAR(4), txt TEXT, n INT, d DECIMAL(5,2), "
        "y YEAR, dt DATETIME, b VARBINARY(4), bitcol BIT(3), f DOUBLE"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 'Alpha', 'aB', 'MiXeD', -7, 12.30, 2024, '2008-01-02 13:29:17', "
        "X'4142', b'101', 1.5), "
        "(2, 'XYZ', 'xY', '', 0, 0.42, 1999, '2024-03-04 05:06:07', "
        "X'6162', b'010', -2.5), "
        "(3, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LOWER(v) AS lv, UPPER(c) AS uc, LOWER(txt) AS ln, "
                   "UPPER(n) AS un, LOWER(d) AS ld, UCASE(y) AS uy, LOWER(dt) AS ldt "
                   "FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .context = "table string case values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, UPPER(v) AS uv FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table string case row envelope",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LOWER(v) AS lv FROM t ORDER BY LOWER(v), id",
            .columns = columns_order_lower,
            .column_count = sizeof(columns_order_lower) / sizeof(columns_order_lower[0]),
            .values = values_order_lower,
            .row_count = 3U,
            .context = "string case lower order expression",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, UPPER(v) AS uv FROM t ORDER BY UPPER(v), id",
            .columns = columns_order_upper,
            .column_count = sizeof(columns_order_upper) / sizeof(columns_order_upper[0]),
            .values = values_order_upper,
            .row_count = 3U,
            .context = "string case upper order expression",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE case_update(id INT, src VARCHAR(20), outv VARCHAR(20))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO case_update VALUES (1, 'Alpha', ''), (2, 'xYz', '')",
        NULL
    );
    failures += execute_ok(database, "UPDATE case_update SET outv = LOWER(src) WHERE id = 1", NULL);
    failures += execute_ok(database, "UPDATE case_update SET outv = UPPER(src) WHERE id = 2", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, outv FROM case_update ORDER BY id",
            .columns = columns_update,
            .column_count = sizeof(columns_update) / sizeof(columns_update[0]),
            .values = values_update,
            .row_count = 2U,
            .context = "string case update assignment",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LOWER(v), UPPER(c) AS upper_alias, UCASE(txt) FROM t WHERE id = 1",
            .columns = columns_labels,
            .column_count = sizeof(columns_labels) / sizeof(columns_labels[0]),
            .values = values_labels,
            .row_count = 1U,
            .context = "string case labels",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "string case preamble unchanged"
    );

    mylite_close(database);
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen string case file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LOWER(v) AS lv, UPPER(c) AS uv FROM t WHERE id = 1",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .context = "string case reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_string_case_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, v VARCHAR(20), b VARBINARY(4), bitcol BIT(3), f DOUBLE)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES (1, 'abc', X'6162', b'101', 1.5), "
        "(2, '\xC3\x89', X'c389', b'010', -2.5)",
        NULL
    );
    failures += execute_ok(database, "CREATE TABLE joined_source(id INT)", NULL);
    failures += execute_error(
        database,
        "UPDATE t JOIN joined_source ON t.id = joined_source.id SET t.v = LOWER(t.v)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOWER()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'LOWER'",
        }
    );
    failures += execute_error(
        database,
        "SELECT LCASE()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'LCASE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT UPPER('a', 'b')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'UPPER'",
        }
    );
    failures += execute_error(
        database,
        "SELECT UCASE('a', 'b')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'UCASE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOWER(missing)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOWER(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LOWER(CONCAT(v)) FROM t WHERE id = 1",
            .columns = (const char *const[]){"LOWER(CONCAT(v))"},
            .column_count = 1U,
            .values = (const char *const[]){"abc"},
            .row_count = 1U,
            .context = "nested concat string case argument",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOWER(CONCAT(v)) FROM t WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string case functions support only ASCII text values",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOWER(CAST('ABC' AS BINARY))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string case functions do not support binary values",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOWER(CONCAT(CAST('ABC' AS BINARY)))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string case functions do not support binary values",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(*) FROM t WHERE LOWER(v) = 'abc'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string case functions support only ASCII text values",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOWER('\xC3\x89')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string case functions support only ASCII text values",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOWER(v) FROM t WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string case functions support only ASCII text values",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOWER(b) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string case functions do not support binary columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOWER(bitcol) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string case functions do not support binary columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOWER(f) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string case functions do not support approximate numeric columns",
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

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    return mylite_test_expect_text(actual, expected, context);
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
