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
    mysql_error_invalid_default = 1067,
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

static int test_no_source_dual_and_do_lengths(void);
static int test_table_backed_lengths_and_reopen(void);
static int test_string_length_diagnostics(void);
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

    failures += test_no_source_dual_and_do_lengths();
    failures += test_table_backed_lengths_and_reopen();
    failures += test_string_length_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_lengths(void) {
    static const char *const columns_no_source[] = {
        "bytes",
        "octets",
        "chars",
        "characters",
        "bits",
        "e_bytes",
        "e_chars",
        "face_bytes",
        "face_chars",
        "null_len",
        "int_len",
        "neg_len",
        "true_bits",
        "db_len",
        "warnings",
    };
    static const char *const values_no_source[] = {
        "3",
        "3",
        "3",
        "3",
        "24",
        "2",
        "1",
        "4",
        "1",
        NULL,
        "3",
        "2",
        "8",
        "3",
        "0",
    };
    static const char *const columns_dual[] = {"a", "b", "c", "d", "e"};
    static const char *const values_dual[] = {"1", "1", "8", "1", "1"};
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
            .sql = "SELECT LENGTH('abc') AS bytes, OCTET_LENGTH('abc') AS octets, "
                   "CHAR_LENGTH('abc') AS chars, CHARACTER_LENGTH('abc') AS characters, "
                   "BIT_LENGTH('abc') AS bits, LENGTH('\xC3\xA9') AS e_bytes, "
                   "CHAR_LENGTH('\xC3\xA9') AS e_chars, "
                   "LENGTH('\xF0\x9F\x99\x82') AS face_bytes, "
                   "CHAR_LENGTH('\xF0\x9F\x99\x82') AS face_chars, "
                   "LENGTH(NULL) AS null_len, LENGTH(123) AS int_len, "
                   "CHAR_LENGTH(-7) AS neg_len, BIT_LENGTH(TRUE) AS true_bits, "
                   "LENGTH(DATABASE()) AS db_len, @@warning_count AS warnings",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source string length values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LENGTH ('a') AS a, OCTET_LENGTH ('a') AS b, "
                   "BIT_LENGTH ('a') AS c, CHAR_LENGTH ('a') AS d, "
                   "CHARACTER_LENGTH ('a') AS e FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual string length whitespace",
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
            .context = "row count after string length select",
        }
    );

    failures +=
        execute_ok(database, "DO LENGTH('abc'), CHAR_LENGTH(NULL), BIT_LENGTH(TRUE)", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "string length do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "string length do rows");
        failures +=
            expect_int64(mylite_result_affected_rows(result), 0, "string length do affected");
        failures +=
            expect_size(mylite_result_warning_count(result), 0U, "string length do warnings");
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
            .context = "row count after string length do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_lengths_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",   "vb",  "vc",  "vbit", "cb",   "cc", "tb", "tc",  "bb", "bc",
        "bbit", "blb", "blc", "bit1", "bit9", "ib", "db", "dtb", "hb",
    };
    static const char *const values_table[] = {
        "1",  "3",  "3",  "24", "1",  "1",  "5",  "5",  "3",  "3",  "24", "2",  "2",  "1",  "2",
        "3",  "5",  "10", "2",  "2",  "6",  "2",  "48", "2",  "1",  "0",  "0",  "0",  "0",  "0",
        "0",  "0",  "1",  "2",  "2",  "5",  NULL, "1",  "3",  NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    };
    static const char *const columns_limited[] = {"id", "bytes"};
    static const char *const values_limited[] = {"3", NULL, "2", "6"};
    static const char *const columns_labels[] = {
        "LENGTH(v)",
        "b",
        "CHAR_LENGTH(v)",
        "CHARACTER_LENGTH(v)",
        "BIT_LENGTH(v)",
    };
    static const char *const values_labels[] = {"3", "3", "3", "3", "24"};
    static const char *const columns_reopen[] = {"bytes", "chars", "hidden_bytes"};
    static const char *const values_reopen[] = {"6", "2", "1"};
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
        "id INT, v VARCHAR(20), c CHAR(5), txt TEXT, b VARBINARY(20), bl BLOB, "
        "b1 BIT(1), b9 BIT(9), i INT, d DECIMAL(6,2), dt DATE, hidden INT"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t(id, v, c, txt, b, bl, b1, b9, i, d, dt, hidden) VALUES "
        "(1, 'abc', 'a  ', 'hello', X'410042', X'00ff', b'1', b'100000001', "
        "123, 12.30, '2024-01-02', 77), "
        "(2, '\xC3\xA9\xF0\x9F\x99\x82', '\xC3\xA9', '', X'', X'', b'0', "
        "b'100000001', -7, -4.50, NULL, 5), "
        "(3, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)",
        NULL
    );
    failures += execute_ok(database, "ALTER TABLE t ALTER hidden SET INVISIBLE", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LENGTH(v) AS vb, CHAR_LENGTH(v) AS vc, BIT_LENGTH(v) AS vbit, "
                   "LENGTH(c) AS cb, CHAR_LENGTH(c) AS cc, LENGTH(txt) AS tb, "
                   "CHAR_LENGTH(txt) AS tc, LENGTH(b) AS bb, CHAR_LENGTH(b) AS bc, "
                   "BIT_LENGTH(b) AS bbit, LENGTH(bl) AS blb, CHAR_LENGTH(bl) AS blc, "
                   "LENGTH(b1) AS bit1, LENGTH(b9) AS bit9, LENGTH(i) AS ib, "
                   "LENGTH(d) AS db, LENGTH(dt) AS dtb, LENGTH(hidden) AS hb "
                   "FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .context = "table string length values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LENGTH(v) AS bytes FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table string length row envelope",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LENGTH(v), OCTET_LENGTH(v) AS b, CHAR_LENGTH(v), "
                   "CHARACTER_LENGTH(v), BIT_LENGTH(v) FROM t WHERE id = 1",
            .columns = columns_labels,
            .column_count = sizeof(columns_labels) / sizeof(columns_labels[0]),
            .values = values_labels,
            .row_count = 1U,
            .context = "string length labels",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "string length preamble unchanged"
    );

    mylite_close(database);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen string length file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LENGTH(v) AS bytes, CHAR_LENGTH(v) AS chars, "
                   "LENGTH(hidden) AS hidden_bytes FROM t WHERE id = 2",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .context = "string length reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_string_length_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, v VARCHAR(20), f FLOAT)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 'a', 1.5)", NULL);
    failures += execute_error(
        database,
        "SELECT LENGTH()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'LENGTH'",
        }
    );
    failures += execute_error(
        database,
        "SELECT OCTET_LENGTH()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'OCTET_LENGTH'",
        }
    );
    failures += execute_error(
        database,
        "SELECT BIT_LENGTH('a', 'b')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'BIT_LENGTH'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CHAR_LENGTH()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'CHAR_LENGTH'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CHARACTER_LENGTH('a', 'b')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'CHARACTER_LENGTH'",
        }
    );
    failures += execute_error(
        database,
        "SELECT LENGTH(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t WHERE LENGTH(v) = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '('",
        }
    );
    failures += execute_error(
        database,
        "SELECT LENGTH(CONCAT(v, 'x')) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string length functions support only string, integer, boolean, NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT LENGTH(v + 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string length functions support only string, integer, boolean, NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT LENGTH(f) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string length functions do not support approximate numeric columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT LENGTH(CAST('ABC' AS BINARY))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string length functions support only string, integer, boolean, NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT LENGTH(RAND()) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "string length functions do not support RAND() arguments in table-backed SELECT",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t ORDER BY LENGTH(v)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '('",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t SET v = LENGTH(v)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'LENGTH'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE default_reject (v INT DEFAULT (LENGTH('a')))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'v'",
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
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-string-length-functions-%s-%d.mylite",
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
