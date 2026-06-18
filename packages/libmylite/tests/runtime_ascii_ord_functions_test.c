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

static int test_no_source_dual_and_do_codepoints(void);
static int test_embedded_nul_codepoints(void);
static int test_table_backed_codepoints_and_reopen(void);
static int test_ascii_ord_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_sized_ok(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_result **out_result,
    const char *context
);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result(
    mylite_result *result,
    const char *const *columns,
    size_t column_count,
    const char *const *values,
    size_t row_count,
    const char *context
);
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

    failures += test_no_source_dual_and_do_codepoints();
    failures += test_embedded_nul_codepoints();
    failures += test_table_backed_codepoints_and_reopen();
    failures += test_ascii_ord_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_codepoints(void) {
    static const char *const columns_no_source[] = {
        "empty_a",     "empty_o",     "null_a", "null_o", "str_a",    "int_o", "true_a",
        "false_o",     "e_a",         "e_o",    "face_a", "face_o",   "hex_a", "hex_o",
        "empty_hex_a", "empty_hex_o", "db_a",   "db_o",   "warnings",
    };
    static const char *const values_no_source[] = {
        "0",   "0",          NULL,  NULL,  "50", "50", "49", "48", "195", "50089",
        "240", "4036991362", "195", "195", "0",  "0",  "97", "97", "0",
    };
    static const char *const columns_dual[] = {"a", "o"};
    static const char *const values_dual[] = {"97", "97"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
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
            .sql = "SELECT ASCII('') AS empty_a, ORD('') AS empty_o, "
                   "ASCII(NULL) AS null_a, ORD(NULL) AS null_o, ASCII('2') AS str_a, "
                   "ORD(2) AS int_o, ASCII(TRUE) AS true_a, ORD(FALSE) AS false_o, "
                   "ASCII('\xC3\xA9') AS e_a, ORD('\xC3\xA9') AS e_o, "
                   "ASCII('\xF0\x9F\x99\x82') AS face_a, "
                   "ORD('\xF0\x9F\x99\x82') AS face_o, ASCII(X'C3A9') AS hex_a, "
                   "ORD(X'C3A9') AS hex_o, ASCII(X'') AS empty_hex_a, "
                   "ORD(X'') AS empty_hex_o, ASCII(DATABASE()) AS db_a, "
                   "ORD(DATABASE()) AS db_o, @@warning_count AS warnings",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source ascii ord values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ASCII ('a') AS a, ORD ('a') AS o FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual ascii ord whitespace",
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
            .context = "row count after ascii ord select",
        }
    );

    failures += execute_ok(database, "DO ASCII('abc'), ORD(NULL), ASCII(X'C3A9')", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "ascii ord do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "ascii ord do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "ascii ord do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "ascii ord do warnings");
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
            .context = "row count after ascii ord do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_embedded_nul_codepoints(void) {
    static const char sql[] = "SELECT ASCII('"
                              "\0"
                              "ab') AS a, ORD('"
                              "\0"
                              "ab') AS o";
    static const char *const columns[] = {"a", "o"};
    static const char *const values[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "embedded-nul", path, sizeof(path));
    failures +=
        execute_sized_ok(database, sql, sizeof(sql) - 1U, &result, "embedded nul ascii ord");
    if (failures == 0) {
        failures += expect_result(
            result,
            columns,
            sizeof(columns) / sizeof(columns[0]),
            values,
            1U,
            "embedded nul ascii ord"
        );
    }
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_codepoints_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",  "av",  "ov", "atxt", "otxt", "ab", "ob",  "abl", "obl", "ab1", "ob1",
        "ab9", "ob9", "ai", "oi",   "ad",   "od", "adt", "odt", "ats", "ots",
    };
    static const char *const values_table[] = {
        "1",          "65",    "65",         "72",    "72",  "195", "195", "0",  "0",  "1",  "1",
        "1",          "1",     "49",         "49",    "49",  "49",  "50",  "50", "50", "50", "2",
        "195",        "50089", "195",        "50089", "0",   "0",   "0",   "0",  "0",  "0",  "0",
        "0",          "45",    "45",         "45",    "45",  NULL,  NULL,  NULL, NULL, "3",  "240",
        "4036991362", "240",   "4036991362", NULL,    NULL,  NULL,  NULL,  NULL, NULL, NULL, NULL,
        NULL,         NULL,    NULL,         NULL,    "50",  "50",  "50",  "50", "4",  "0",  "0",
        "0",          "0",     "65",         "65",    "195", "195", "1",   "1",  "0",  "0",  "48",
        "48",         "48",    "48",         "50",    "50",  "50",  "50",
    };
    static const char *const columns_limited[] = {"id", "a", "o"};
    static const char *const values_limited[] = {
        "4",
        "0",
        "0",
        "3",
        "240",
        "4036991362",
    };
    static const char *const columns_labels[] = {"ASCII(v)", "a", "ORD(v)", "o"};
    static const char *const values_labels[] = {"65", "65", "65", "65"};
    static const char *const columns_predicate[] = {"id"};
    static const char *const values_ascii_predicate[] = {"1"};
    static const char *const values_ord_order[] = {"4", "1", "2", "3"};
    static const char *const columns_dml[] = {"id", "a", "o"};
    static const char *const values_dml[] = {"1", "97", "97", "2", "0", "0", "3", NULL, NULL};
    static const char *const columns_reopen[] = {"a", "o", "ba", "bo"};
    static const char *const values_reopen[] = {"240", "4036991362", NULL, NULL};
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
        "id INT, v VARCHAR(20), txt TEXT, b VARBINARY(20), bl BLOB, b1 BIT(1), "
        "b9 BIT(9), i INT, d DECIMAL(6,2), dt DATE, ts DATETIME"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 'ABC', 'Hello', X'C3A9', X'00FF', b'1', b'100000001', "
        "123, 12.30, '2024-01-02', '2024-01-02 03:04:05'), "
        "(2, '\xC3\xA9', '\xC3\xA9', X'', X'', b'0', b'000000000', "
        "-7, -4.50, NULL, NULL), "
        "(3, '\xF0\x9F\x99\x82', '\xF0\x9F\x99\x82', NULL, NULL, NULL, NULL, "
        "NULL, NULL, '2000-01-01', '2000-01-01 00:00:00'), "
        "(4, '', '', X'410042', X'C3A9', b'1', b'010000001', "
        "0, 0.00, '2001-02-03', '2001-02-03 04:05:06')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ASCII(v) AS av, ORD(v) AS ov, ASCII(txt) AS atxt, "
                   "ORD(txt) AS otxt, ASCII(b) AS ab, ORD(b) AS ob, ASCII(bl) AS abl, "
                   "ORD(bl) AS obl, ASCII(b1) AS ab1, ORD(b1) AS ob1, "
                   "ASCII(b9) AS ab9, ORD(b9) AS ob9, ASCII(i) AS ai, ORD(i) AS oi, "
                   "ASCII(d) AS ad, ORD(d) AS od, ASCII(dt) AS adt, ORD(dt) AS odt, "
                   "ASCII(ts) AS ats, ORD(ts) AS ots FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 4U,
            .context = "table ascii ord values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ASCII(v) AS a, ORD(v) AS o "
                   "FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table ascii ord row envelope",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ASCII(v), ASCII(v) AS a, ORD(v), ORD(v) AS o "
                   "FROM t WHERE id = 1",
            .columns = columns_labels,
            .column_count = sizeof(columns_labels) / sizeof(columns_labels[0]),
            .values = values_labels,
            .row_count = 1U,
            .context = "ascii ord labels",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE ASCII(v) = 65 ORDER BY id",
            .columns = columns_predicate,
            .column_count = 1U,
            .values = values_ascii_predicate,
            .row_count = 1U,
            .context = "ascii predicate rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t ORDER BY ORD(v)",
            .columns = columns_predicate,
            .column_count = 1U,
            .values = values_ord_order,
            .row_count = 4U,
            .context = "ord order rows",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE dml_codepoints(id INT, v VARCHAR(20), a INT, o VARCHAR(20))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO dml_codepoints VALUES (1, 'a', NULL, NULL), (2, '', NULL, NULL), "
        "(3, NULL, NULL, NULL)",
        NULL
    );
    failures += execute_ok(database, "UPDATE dml_codepoints SET a = ASCII(v), o = ORD(v)", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, o FROM dml_codepoints ORDER BY id",
            .columns = columns_dml,
            .column_count = sizeof(columns_dml) / sizeof(columns_dml[0]),
            .values = values_dml,
            .row_count = 3U,
            .context = "ascii ord update assignment rows",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "ascii ord preamble unchanged"
    );

    mylite_close(database);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen ascii ord file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ASCII(v) AS a, ORD(v) AS o, ASCII(b) AS ba, ORD(b) AS bo "
                   "FROM t WHERE id = 3",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .context = "ascii ord reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_ascii_ord_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, v VARCHAR(20), f FLOAT)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 'a', 1.5)", NULL);
    failures += execute_error(
        database,
        "SELECT ASCII()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT ASCII('a', 'b')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT ORD()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'ORD'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ORD('a', 'b')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'ORD'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ASCII(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ASCII(CONCAT(v, 'x')) FROM t",
            .columns = (const char *const[]){"ASCII(CONCAT(v, 'x'))"},
            .column_count = 1U,
            .values = (const char *const[]){"97"},
            .row_count = 1U,
            .context = "nested concat codepoint argument",
        }
    );
    failures += execute_error(
        database,
        "SELECT ORD(v + 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string codepoint functions support only string, hex, integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT ASCII(f) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string codepoint functions do not support approximate numeric columns",
        }
    );
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    return execute_sized_ok(database, sql, strlen(sql), out_result, sql);
}

static int execute_sized_ok(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_result **out_result,
    const char *context
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, sql_size, &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", context, rc, mylite_errmsg(database));
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
    failures += expect_result(
        result,
        expected.columns,
        expected.column_count,
        expected.values,
        expected.row_count,
        expected.context
    );

    mylite_result_free(result);
    return failures;
}

static int expect_result(
    mylite_result *result,
    const char *const *columns,
    size_t column_count,
    const char *const *values,
    size_t row_count,
    const char *context
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), column_count, context);
    failures += expect_size(mylite_result_row_count(result), row_count, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    for (size_t column = 0U; column < column_count; ++column) {
        failures +=
            expect_text(mylite_result_column_name(result, column), columns[column], context);
    }
    for (size_t row = 0U; row < row_count; ++row) {
        for (size_t column = 0U; column < column_count; ++column) {
            size_t value_index = (row * column_count) + column;

            failures += expect_result_value(result, row, column, values[value_index], context);
        }
    }
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
        "/tmp/mylite-ascii-ord-functions-%s-%d.mylite",
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
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
