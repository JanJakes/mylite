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
    mysql_error_parse = 1064,
    mysql_error_incorrect_string_value = 1411,
    mysql_error_native_function_count = 1582,
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

static int test_no_source_dual_and_do_uuid(void);
static int test_table_backed_uuid_and_reopen(void);
static int test_uuid_diagnostics(void);
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
static int expect_result_cell(
    const mylite_result *result,
    size_t row,
    size_t column,
    struct expected_cell expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(const void *actual, const void *expected, size_t size, const char *context);

int main(void) {
    int failures = 0;

    failures += test_no_source_dual_and_do_uuid();
    failures += test_table_backed_uuid_and_reopen();
    failures += test_uuid_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_uuid(void) {
    static const unsigned char uuid_bytes[] = {
        0x6c,
        0xcd,
        0x78,
        0x0c,
        0xba,
        0xba,
        0x10,
        0x26,
        0x95,
        0x64,
        0x5b,
        0x8c,
        0x65,
        0x60,
        0x24,
        0xdb,
    };
    static const unsigned char swapped_bytes[] = {
        0x10,
        0x26,
        0xba,
        0xba,
        0x6c,
        0xcd,
        0x78,
        0x0c,
        0x95,
        0x64,
        0x5b,
        0x8c,
        0x65,
        0x60,
        0x24,
        0xdb,
    };
    static const char *const columns_scalar[] = {
        "canonical",
        "upper_value",
        "compact",
        "braced",
        "bad",
        "num",
        "nil",
        "plain",
        "swapped",
        "negative_swap",
        "null_swap",
        "text16",
        "roundtrip",
        "mismatch",
        "@@warning_count",
    };
    static const struct expected_cell values_scalar[] = {
        {(const unsigned char *)"1", 1U, false},
        {(const unsigned char *)"1", 1U, false},
        {(const unsigned char *)"1", 1U, false},
        {(const unsigned char *)"1", 1U, false},
        {(const unsigned char *)"0", 1U, false},
        {(const unsigned char *)"0", 1U, false},
        {NULL, 0U, true},
        {uuid_bytes, sizeof(uuid_bytes), false},
        {swapped_bytes, sizeof(swapped_bytes), false},
        {swapped_bytes, sizeof(swapped_bytes), false},
        {uuid_bytes, sizeof(uuid_bytes), false},
        {(const unsigned char *)"61626364-6566-6768-696a-6b6c6d6e6f70", 36U, false},
        {(const unsigned char *)"6ccd780c-baba-1026-9564-5b8c656024db", 36U, false},
        {(const unsigned char *)"baba1026-780c-6ccd-9564-5b8c656024db", 36U, false},
        {(const unsigned char *)"0", 1U, false},
    };
    static const char *const columns_dual[] = {"uuid_value"};
    static const struct expected_cell values_dual[] = {
        {(const unsigned char *)"6ccd780c-baba-1026-9564-5b8c656024db", 36U, false},
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += execute_ok(database, "SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT IS_UUID('6ccd780c-baba-1026-9564-5b8c656024db') AS canonical, "
                   "IS_UUID('6CCD780C-BABA-1026-9564-5B8C656024DB') AS upper_value, "
                   "IS_UUID('6ccd780cbaba102695645b8c656024db') AS compact, "
                   "IS_UUID('{6ccd780c-baba-1026-9564-5b8c656024db}') AS braced, "
                   "IS_UUID('bad') AS bad, IS_UUID(123) AS num, IS_UUID(NULL) AS nil, "
                   "UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db') AS plain, "
                   "UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db', 1) AS swapped, "
                   "UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db', -1) AS negative_swap, "
                   "UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db', NULL) AS null_swap, "
                   "BIN_TO_UUID('abcdefghijklmnop') AS text16, "
                   "BIN_TO_UUID(UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db', 1), 1) "
                   "AS roundtrip, "
                   "BIN_TO_UUID(UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db'), 1) "
                   "AS mismatch, @@warning_count",
            .columns = columns_scalar,
            .column_count = sizeof(columns_scalar) / sizeof(columns_scalar[0]),
            .values = values_scalar,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "no-source uuid conversion values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT BIN_TO_UUID(UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db')) "
                   "AS uuid_value FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "dual uuid conversion",
        }
    );
    failures += execute_ok(
        database,
        "DO IS_UUID('6ccd780c-baba-1026-9564-5b8c656024db'), "
        "UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db'), "
        "BIN_TO_UUID(UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db'))",
        &result
    );
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "uuid do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "uuid do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "uuid do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "uuid do warnings");
    }
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_uuid_and_reopen(void) {
    static const unsigned char uuid_bytes[] = {
        0x6c,
        0xcd,
        0x78,
        0x0c,
        0xba,
        0xba,
        0x10,
        0x26,
        0x95,
        0x64,
        0x5b,
        0x8c,
        0x65,
        0x60,
        0x24,
        0xdb,
    };
    static const unsigned char swapped_bytes[] = {
        0x10,
        0x26,
        0xba,
        0xba,
        0x6c,
        0xcd,
        0x78,
        0x0c,
        0x95,
        0x64,
        0x5b,
        0x8c,
        0x65,
        0x60,
        0x24,
        0xdb,
    };
    static const char *const columns_table[] = {"id", "ok", "ub", "ubs", "bt", "bts"};
    static const struct expected_cell values_table[] = {
        {(const unsigned char *)"1", 1U, false},
        {(const unsigned char *)"1", 1U, false},
        {uuid_bytes, sizeof(uuid_bytes), false},
        {swapped_bytes, sizeof(swapped_bytes), false},
        {(const unsigned char *)"6ccd780c-baba-1026-9564-5b8c656024db", 36U, false},
        {(const unsigned char *)"baba1026-780c-6ccd-9564-5b8c656024db", 36U, false},
        {(const unsigned char *)"2", 1U, false},
        {(const unsigned char *)"1", 1U, false},
        {uuid_bytes, sizeof(uuid_bytes), false},
        {swapped_bytes, sizeof(swapped_bytes), false},
        {(const unsigned char *)"1026baba-6ccd-780c-9564-5b8c656024db", 36U, false},
        {(const unsigned char *)"6ccd780c-baba-1026-9564-5b8c656024db", 36U, false},
        {(const unsigned char *)"3", 1U, false},
        {(const unsigned char *)"1", 1U, false},
        {uuid_bytes, sizeof(uuid_bytes), false},
        {swapped_bytes, sizeof(swapped_bytes), false},
        {(const unsigned char *)"61626364-6566-6768-696a-6b6c6d6e6f70", 36U, false},
        {(const unsigned char *)"65666768-6364-6162-696a-6b6c6d6e6f70", 36U, false},
    };
    static const char *const columns_limited[] = {"id", "ok"};
    static const struct expected_cell values_limited[] = {
        {(const unsigned char *)"3", 1U, false},
        {(const unsigned char *)"0", 1U, false},
        {(const unsigned char *)"2", 1U, false},
        {NULL, 0U, true},
    };
    static const char *const columns_families[] = {
        "txt_ok",
        "fixed_bin_uuid",
        "blob_uuid",
        "nested_plain",
        "nested_swap",
    };
    static const struct expected_cell values_families[] = {
        {(const unsigned char *)"1", 1U, false},
        {(const unsigned char *)"6ccd780c-baba-1026-9564-5b8c656024db", 36U, false},
        {(const unsigned char *)"6ccd780c-baba-1026-9564-5b8c656024db", 36U, false},
        {(const unsigned char *)"6ccd780c-baba-1026-9564-5b8c656024db", 36U, false},
        {(const unsigned char *)"6ccd780c-baba-1026-9564-5b8c656024db", 36U, false},
    };
    static const char *const columns_reopen[] = {"uuid_value"};
    static const struct expected_cell values_reopen[] = {
        {(const unsigned char *)"6ccd780c-baba-1026-9564-5b8c656024db", 36U, false},
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
        "CREATE TABLE t(id INT, s VARCHAR(64), b VARBINARY(16), fixed_bin BINARY(16), "
        "txt TEXT, blob_value BLOB, invalid VARCHAR(64), i INT)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '6ccd780c-baba-1026-9564-5b8c656024db', "
        "X'6CCD780CBABA102695645B8C656024DB', X'6CCD780CBABA102695645B8C656024DB', "
        "'6ccd780c-baba-1026-9564-5b8c656024db', X'6CCD780CBABA102695645B8C656024DB', "
        "'bad', 42), "
        "(2, '6CCD780CBABA102695645B8C656024DB', "
        "X'1026BABA6CCD780C95645B8C656024DB', X'6CCD780CBABA102695645B8C656024DB', "
        "'6ccd780c-baba-1026-9564-5b8c656024db', X'6CCD780CBABA102695645B8C656024DB', "
        "NULL, 7), "
        "(3, '{6ccd780c-baba-1026-9564-5b8c656024db}', "
        "X'6162636465666768696A6B6C6D6E6F70', X'6CCD780CBABA102695645B8C656024DB', "
        "'6ccd780c-baba-1026-9564-5b8c656024db', X'6CCD780CBABA102695645B8C656024DB', "
        "'bad', NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, IS_UUID(s) AS ok, UUID_TO_BIN(s) AS ub, "
                   "UUID_TO_BIN(s, 1) AS ubs, BIN_TO_UUID(b) AS bt, "
                   "BIN_TO_UUID(b, 1) AS bts FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .warning_count = 0U,
            .context = "table uuid conversion values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, IS_UUID(invalid) AS ok FROM t WHERE id >= 2 ORDER BY id DESC "
                   "LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "table uuid row envelope",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT IS_UUID(txt) AS txt_ok, BIN_TO_UUID(fixed_bin) AS fixed_bin_uuid, "
                   "BIN_TO_UUID(blob_value) AS blob_uuid, BIN_TO_UUID(UUID_TO_BIN(s)) "
                   "AS nested_plain, BIN_TO_UUID(UUID_TO_BIN(s, 1), 1) AS nested_swap "
                   "FROM t WHERE id = 1",
            .columns = columns_families,
            .column_count = sizeof(columns_families) / sizeof(columns_families[0]),
            .values = values_families,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "table uuid descriptor families and nesting",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "uuid preamble unchanged"
    );

    mylite_close(database);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen uuid file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT BIN_TO_UUID(b, 1) AS uuid_value FROM t WHERE id = 2",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "uuid reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_uuid_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, s VARCHAR(64), bad VARCHAR(64), b VARBINARY(1), i INT)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES (1, '6ccd780c-baba-1026-9564-5b8c656024db', 'bad', X'00', 42)",
        NULL
    );
    failures += execute_error(
        database,
        "SELECT IS_UUID()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'IS_UUID'",
        }
    );
    failures += execute_error(
        database,
        "SELECT IS_UUID('a', 'b')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'IS_UUID'",
        }
    );
    failures += execute_error(
        database,
        "SELECT UUID_TO_BIN()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'UUID_TO_BIN'",
        }
    );
    failures += execute_error(
        database,
        "SELECT UUID_TO_BIN('bad')",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_string_value,
            .sqlstate = "HY000",
            .message_part = "Incorrect string value: 'bad' for function uuid_to_bin",
        }
    );
    failures += execute_error(
        database,
        "SELECT UUID_TO_BIN(bad) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_string_value,
            .sqlstate = "HY000",
            .message_part = "Incorrect string value: 'bad' for function uuid_to_bin",
        }
    );
    failures += execute_error(
        database,
        "SELECT UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db', '1')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UUID conversion swap flag supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db', 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'UUID_TO_BIN'",
        }
    );
    failures += execute_error(
        database,
        "SELECT BIN_TO_UUID()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'BIN_TO_UUID'",
        }
    );
    failures += execute_error(
        database,
        "SELECT BIN_TO_UUID(X'00')",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_string_value,
            .sqlstate = "HY000",
            .message_part = "for function bin_to_uuid",
        }
    );
    failures += execute_error(
        database,
        "SELECT BIN_TO_UUID(b) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_string_value,
            .sqlstate = "HY000",
            .message_part = "for function bin_to_uuid",
        }
    );
    failures += execute_error(
        database,
        "SELECT BIN_TO_UUID(X'6CCD780CBABA102695645B8C656024DB', 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'BIN_TO_UUID'",
        }
    );
    failures += execute_error(
        database,
        "SELECT IS_UUID(missing)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT IS_UUID(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT BIN_TO_UUID(i) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "BIN_TO_UUID() supports only nonbinary string and binary string columns",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t SET s = UUID_TO_BIN(s)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
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

    if (failures == 0) {
        failures += expect_size(
            mylite_result_column_count(result),
            expected.column_count,
            expected.context
        );
        failures +=
            expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
        failures += expect_size(
            mylite_result_warning_count(result),
            expected.warning_count,
            expected.context
        );
    }
    for (size_t column_index = 0U; failures == 0 && column_index < expected.column_count;
         ++column_index) {
        failures += expect_text(
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
        "runtime_uuid_conversion_functions_%s_%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "%s: failed to build path\n", name);
        return 1;
    }
    return 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
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
    failures += expect_size(actual_size, expected.size, context);
    if (failures == 0 && expected.size != 0U) {
        failures += expect_bytes(actual, expected.bytes, expected.size, context);
    }
    return failures;
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
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected text [%s], got [%s]\n",
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
