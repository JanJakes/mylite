#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

#include <stdbool.h>
#include <stdint.h>
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
    mysql_error_unknown_character_set = 1115,
    mysql_error_parse = 1064,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_cell {
    const void *bytes;
    size_t size;
    bool is_null;
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    size_t column_count;
    const struct expected_cell *values;
    size_t row_count;
    size_t warning_count;
    const char *context;
};

static int test_no_source_dual_and_do_char(void);
static int test_table_backed_char_and_reopen(void);
static int test_char_diagnostics(void);
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
static int expect_result_cell(
    const mylite_result *result,
    size_t row,
    size_t column,
    struct expected_cell expected,
    const char *context
);
static int expect_bytes(const void *actual, const void *expected, size_t size, const char *context);

int main(void) {
    int failures = 0;

    failures += test_no_source_dual_and_do_char();
    failures += test_table_backed_char_and_reopen();
    failures += test_char_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_char(void) {
    static const unsigned char a_bytes[] = {0x41};
    static const unsigned char mysql_bytes[] = {0x4d, 0x79, 0x53, 0x51, 0x4c};
    static const unsigned char skip_null_bytes[] = {0x41, 0x42};
    static const unsigned char bool_bytes[] = {0x01, 0x00};
    static const unsigned char zero_bytes[] = {0x00};
    static const unsigned char two_byte_bytes[] = {0x01, 0x00};
    static const unsigned char three_byte_bytes[] = {0x01, 0x00, 0x00};
    static const unsigned char u32_bytes[] = {0xff, 0xff, 0xff, 0xff};
    static const unsigned char neg256_bytes[] = {0xff, 0xff, 0xff, 0x00};
    static const unsigned char invalid_prefix_bytes[] = {0x41};
    static const char *const columns_scalar[] = {
        "c",
        "word",
        "empty_value",
        "skip_null",
        "bools",
        "zero_value",
        "two_byte",
        "three_byte",
        "u32",
        "neg_one",
        "neg256",
        "@@warning_count",
    };
    static const struct expected_cell values_scalar[] = {
        {a_bytes, sizeof(a_bytes), false},
        {mysql_bytes, sizeof(mysql_bytes), false},
        {NULL, 0U, false},
        {skip_null_bytes, sizeof(skip_null_bytes), false},
        {bool_bytes, sizeof(bool_bytes), false},
        {zero_bytes, sizeof(zero_bytes), false},
        {two_byte_bytes, sizeof(two_byte_bytes), false},
        {three_byte_bytes, sizeof(three_byte_bytes), false},
        {u32_bytes, sizeof(u32_bytes), false},
        {u32_bytes, sizeof(u32_bytes), false},
        {neg256_bytes, sizeof(neg256_bytes), false},
        {(const unsigned char *)"0", 1U, false},
    };
    static const unsigned char e_acute_bytes[] = {0xc3, 0xa9};
    static const char *const columns_using[] = {
        "using_binary",
        "using_utf8mb4",
        "using_latin1",
        "using_charset",
        "using_collation",
        "using_coercibility",
    };
    static const struct expected_cell values_using[] = {
        {a_bytes, sizeof(a_bytes), false},
        {e_acute_bytes, sizeof(e_acute_bytes), false},
        {a_bytes, sizeof(a_bytes), false},
        {(const unsigned char *)"utf8mb4", 7U, false},
        {(const unsigned char *)"utf8mb4_0900_ai_ci", 18U, false},
        {(const unsigned char *)"4", 1U, false},
    };
    static const char *const columns_warning[] = {"u", "u3"};
    static const struct expected_cell values_warning[] = {
        {a_bytes, sizeof(a_bytes), false},
        {(const unsigned char *)"B", 1U, false},
    };
    static const char *const columns_invalid[] = {"prefix"};
    static const struct expected_cell values_invalid[] = {
        {invalid_prefix_bytes, sizeof(invalid_prefix_bytes), false},
    };
    static const char *const columns_strict_invalid[] = {"invalid_value"};
    static const struct expected_cell values_strict_invalid[] = {
        {NULL, 0U, true},
    };
    static const char *const columns_dual[] = {"u"};
    static const struct expected_cell values_dual[] = {{a_bytes, sizeof(a_bytes), false}};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += execute_ok(database, "SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CHAR(65) AS c, CHAR(77,121,83,81,76) AS word, "
                   "CHAR(NULL) AS empty_value, CHAR(65,NULL,66) AS skip_null, "
                   "CHAR(TRUE,FALSE) AS bools, CHAR(0) AS zero_value, "
                   "CHAR(256) AS two_byte, CHAR(65536) AS three_byte, "
                   "CHAR(4294967295) AS u32, CHAR(-1) AS neg_one, "
                   "CHAR(-256) AS neg256, @@warning_count",
            .columns = columns_scalar,
            .column_count = sizeof(columns_scalar) / sizeof(columns_scalar[0]),
            .values = values_scalar,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "no-source char values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CHAR(65 USING binary) AS using_binary, "
                   "CHAR(195,169 USING utf8mb4) AS using_utf8mb4, "
                   "CHAR(65 USING latin1) AS using_latin1, "
                   "CHARSET(CHAR(65 USING utf8mb4)) AS using_charset, "
                   "COLLATION(CHAR(65 USING utf8mb4)) AS using_collation, "
                   "COERCIBILITY(CHAR(65 USING utf8mb4)) AS using_coercibility",
            .columns = columns_using,
            .column_count = sizeof(columns_using) / sizeof(columns_using[0]),
            .values = values_using,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "no-source char using charset values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CHAR(65 USING utf8) AS u, CHAR(66 USING utf8mb3) AS u3",
            .columns = columns_warning,
            .column_count = sizeof(columns_warning) / sizeof(columns_warning[0]),
            .values = values_warning,
            .row_count = 1U,
            .warning_count = 2U,
            .context = "char using charset warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CHAR(65,255,66 USING utf8mb4) AS prefix",
            .columns = columns_invalid,
            .column_count = sizeof(columns_invalid) / sizeof(columns_invalid[0]),
            .values = values_invalid,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "char using invalid utf8 prefix",
        }
    );
    failures += execute_ok(
        database,
        "SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CHAR(65,255,66 USING utf8mb4) AS invalid_value",
            .columns = columns_strict_invalid,
            .column_count = sizeof(columns_strict_invalid) / sizeof(columns_strict_invalid[0]),
            .values = values_strict_invalid,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "char using strict invalid utf8",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CHAR (65) AS u FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "dual char whitespace",
        }
    );
    failures += execute_ok(database, "DO CHAR(65), CHAR(NULL)", &result);
    if (failures == 0) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 0U, "char do columns");
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "char do rows");
        failures +=
            mylite_test_expect_int64(mylite_result_affected_rows(result), 0, "char do affected");
        failures +=
            mylite_test_expect_size(mylite_result_warning_count(result), 0U, "char do warnings");
    }
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_char_and_reopen(void) {
    static const char *const columns_table[] = {"id", "cn", "cb", "cu", "combo"};
    static const unsigned char n256_bytes[] = {0x01, 0x00};
    static const unsigned char neg256_bytes[] = {0xff, 0xff, 0xff, 0x00};
    static const unsigned char one_bytes[] = {0x01};
    static const unsigned char combo_3_bytes[] = {0x01, 0x00, 0x01};
    static const unsigned char neg1_bytes[] = {0xff, 0xff, 0xff, 0xff};
    static const unsigned char zero_bytes[] = {0x00};
    static const struct expected_cell values_table[] = {
        {(const unsigned char *)"3", 1U, false},
        {n256_bytes, sizeof(n256_bytes), false},
        {neg256_bytes, sizeof(neg256_bytes), false},
        {one_bytes, sizeof(one_bytes), false},
        {combo_3_bytes, sizeof(combo_3_bytes), false},
        {(const unsigned char *)"2", 1U, false},
        {NULL, 0U, false},
        {neg1_bytes, sizeof(neg1_bytes), false},
        {zero_bytes, sizeof(zero_bytes), false},
        {zero_bytes, sizeof(zero_bytes), false},
    };
    static const char *const columns_reopen[] = {"cn", "cb", "cu"};
    static const unsigned char a_bytes[] = {0x41};
    static const unsigned char three_byte_bytes[] = {0x01, 0x00, 0x00};
    static const unsigned char u32_bytes[] = {0xff, 0xff, 0xff, 0xff};
    static const struct expected_cell values_reopen[] = {
        {a_bytes, sizeof(a_bytes), false},
        {three_byte_bytes, sizeof(three_byte_bytes), false},
        {u32_bytes, sizeof(u32_bytes), false},
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    mylite_file_preamble_init(expected_preamble);
    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, n INT, b BIGINT, u BIGINT UNSIGNED, label VARCHAR(10))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 65, 65536, 4294967295, 'a'), "
        "(2, NULL, -1, 0, 'b'), "
        "(3, 256, -256, 1, 'c')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, CHAR(n) AS cn, CHAR(b) AS cb, CHAR(u) AS cu, "
                   "CHAR(n,u) AS combo FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "table char row envelope",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "char preamble unchanged"
    );

    mylite_close(database);
    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen char file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CHAR(n) AS cn, CHAR(b) AS cb, CHAR(u) AS cu FROM t WHERE id = 1",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "char reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_char_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, n INT, label VARCHAR(20), d DECIMAL(5,2), bitcol BIT(4))",
        NULL
    );
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 65, 'label', 12.30, b'1010')", NULL);
    failures += execute_error(
        database,
        "SELECT CHAR()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT CHAR('77')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CHAR() supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT CHAR(77.3)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CHAR() supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT CHAR(X'41')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CHAR() supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT CHAR(missing)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CHAR(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CHAR(label) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CHAR() supports only integer descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT CHAR(d) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CHAR() supports only integer descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT CHAR(n USING utf8mb4) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CHAR() supports one or more integer, boolean, NULL, and descriptor "
                            "integer column arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT CHAR(65 USING nosuch)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_character_set,
            .sqlstate = "42000",
            .message_part = "Unknown character set: 'nosuch'",
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

    if (failures == 0) {
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
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            expected.warning_count,
            expected.context
        );
    }
    for (size_t column_index = 0U; failures == 0 && column_index < expected.column_count;
         ++column_index) {
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, column_index),
            expected.columns[column_index],
            expected.context
        );
    }
    for (size_t row_index = 0U; failures == 0 && row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
            size_t offset = (row_index * expected.column_count) + column_index;

            failures += expect_result_cell(
                result,
                row_index,
                column_index,
                expected.values[offset],
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
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    remove(related);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek file\n", path);
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    fclose(file);
    if (bytes_read != size) {
        fprintf(stderr, "%s: expected %zu bytes, read %zu\n", path, size, bytes_read);
        return 1;
    }
    return 0;
}

static int expect_result_cell(
    const mylite_result *result,
    size_t row,
    size_t column,
    struct expected_cell expected,
    const char *context
) {
    const void *actual = mylite_result_value_bytes(result, row, column);
    size_t actual_size = mylite_result_value_size(result, row, column);
    int failures = 0;

    if (expected.is_null) {
        if (actual != NULL) {
            fprintf(stderr, "%s: row %zu column %zu expected NULL\n", context, row, column);
            return 1;
        }
        return 0;
    }
    if (actual == NULL) {
        fprintf(stderr, "%s: row %zu column %zu expected non-NULL\n", context, row, column);
        return 1;
    }
    failures += mylite_test_expect_size(actual_size, expected.size, context);
    if (failures == 0 && expected.size != 0U) {
        failures += expect_bytes(actual, expected.bytes, expected.size, context);
    }
    return failures;
}

static int expect_bytes(
    const void *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if ((actual == NULL || expected == NULL || memcmp(actual, expected, size) != 0) && size != 0U) {
        fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }
    return 0;
}
