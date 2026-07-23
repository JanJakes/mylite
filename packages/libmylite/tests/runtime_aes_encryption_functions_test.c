#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CELL_TEXT(value) {(const unsigned char *)(value), sizeof(value) - 1U, false}
#define CELL_NULL {NULL, 0U, true}

enum {
    mysql_error_native_function_arity = 1582,
    mysql_collation_binary_id = 63,
    mysql_approximate_decimals = 31,
};

static const uint64_t mysql_json_document_display_length = 4294967292ULL;

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
    size_t column_count;
    const struct expected_cell *values;
    size_t row_count;
    size_t warning_count;
    const char *context;
};

struct expected_warning {
    const char *code;
    const char *message_part;
};

struct expected_column_metadata {
    enum mylite_result_column_type type;
    uint32_t flags;
    uint32_t charset_id;
    uint32_t collation_id;
    uint64_t display_length;
    uint16_t decimals;
    int nullable;
};

static int test_scalar_aes_functions(void);
static int test_aes_dual_do_and_arity(void);
static int test_table_backed_aes_functions(void);
static int test_aes_metadata(void);
static int setup_database(mylite_db **out_database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_cell(
    const mylite_result *result,
    size_t row,
    size_t column,
    struct expected_cell expected,
    const char *context
);
static int expect_warning(
    const mylite_result *result,
    size_t row,
    struct expected_warning expected,
    const char *context
);
static int expect_column_metadata(
    const mylite_result *result,
    size_t column,
    struct expected_column_metadata expected,
    const char *context
);
static int expect_bytes_contains(
    const void *actual,
    size_t actual_size,
    const char *needle,
    const char *context
);
static int expect_bytes(const void *actual, const void *expected, size_t size, const char *context);

int main(void) {
    int failures = 0;

    failures += test_scalar_aes_functions();
    failures += test_aes_dual_do_and_arity();
    failures += test_table_backed_aes_functions();
    failures += test_aes_metadata();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_aes_functions(void) {
    static const struct expected_cell values[] = {
        CELL_TEXT("15E36637363712FC2E699B9C95B75393"),
        CELL_TEXT("C717530F41F320757B4AA1BFAF11C42E"),
        CELL_TEXT("72ED6F8AD19A085C32094E16EFC34A08C717530F41F320757B4AA1BFAF11C42E"),
        CELL_TEXT("text"),
        CELL_TEXT("000102"),
        CELL_NULL,
        CELL_NULL,
        CELL_NULL,
        CELL_NULL,
        CELL_NULL,
        CELL_NULL,
    };
    mylite_db *database = NULL;
    mylite_result *warnings = NULL;
    int failures = setup_database(&database);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT HEX(AES_ENCRYPT('text','key')), HEX(AES_ENCRYPT('','key')), "
                   "HEX(AES_ENCRYPT('1234567890123456','key')), "
                   "AES_DECRYPT(AES_ENCRYPT('text','key'),'key'), "
                   "HEX(AES_DECRYPT(AES_ENCRYPT(X'000102','key'),'key')), "
                   "AES_ENCRYPT(NULL,'key'), AES_ENCRYPT('text',NULL), "
                   "AES_DECRYPT(NULL,'key'), AES_DECRYPT(AES_ENCRYPT('text','key'),NULL), "
                   "AES_DECRYPT(AES_ENCRYPT('text','key'),'wrong'), AES_DECRYPT(X'00','key')",
            .column_count = sizeof(values) / sizeof(values[0]),
            .values = values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "scalar AES encryption and decryption",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT HEX(AES_ENCRYPT('text','12345678901234567890'))",
            .column_count = 1U,
            .values = (const struct expected_cell[]){CELL_TEXT("0A51A05D4FA1AC9807BF1783EF411CA6")},
            .row_count = 1U,
            .warning_count = 1U,
            .context = "scalar AES long-key warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT AES_ENCRYPT(NULL,'12345678901234567890')",
            .column_count = 1U,
            .values = (const struct expected_cell[]){CELL_NULL},
            .row_count = 1U,
            .warning_count = 1U,
            .context = "scalar AES null-input long-key warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT AES_DECRYPT(NULL,'12345678901234567890')",
            .column_count = 1U,
            .values = (const struct expected_cell[]){CELL_NULL},
            .row_count = 1U,
            .warning_count = 1U,
            .context = "scalar AES null decrypt long-key warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT AES_DECRYPT(X'00','12345678901234567890')",
            .column_count = 1U,
            .values = (const struct expected_cell[]){CELL_NULL},
            .row_count = 1U,
            .warning_count = 1U,
            .context = "scalar AES invalid decrypt long-key warning",
        }
    );
    failures +=
        execute_ok(database, "SELECT HEX(AES_ENCRYPT('text','12345678901234567890'))", NULL);
    failures += execute_ok(database, "SHOW WARNINGS", &warnings);
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_row_count(warnings),
            1U,
            "AES explicit warnings rows"
        );
        failures += expect_warning(
            warnings,
            0U,
            (struct expected_warning){.code = "3237", .message_part = "AES key size"},
            "AES long-key warning"
        );
    }
    mylite_result_free(warnings);

    mylite_close(database);
    return failures;
}

static int test_aes_dual_do_and_arity(void) {
    mylite_db *database = NULL;
    int failures = setup_database(&database);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT HEX(AES_ENCRYPT('text','key')) FROM DUAL",
            .column_count = 1U,
            .values = (const struct expected_cell[]){CELL_TEXT("15E36637363712FC2E699B9C95B75393")},
            .row_count = 1U,
            .warning_count = 0U,
            .context = "AES from DUAL",
        }
    );
    failures += execute_ok(
        database,
        "DO AES_ENCRYPT('text','key'), AES_DECRYPT(AES_ENCRYPT('text','key'),'key')",
        NULL
    );
    failures += execute_error(
        database,
        "SELECT AES_ENCRYPT('text')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'AES_ENCRYPT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT AES_ENCRYPT(NULL, AES_ENCRYPT('x'))",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'AES_ENCRYPT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT AES_DECRYPT(AES_ENCRYPT('text','key'),'key','extra')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'AES_DECRYPT'",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_aes_functions(void) {
    static const struct expected_cell projection_values[] = {
        CELL_TEXT("1"),
        CELL_TEXT("15E36637363712FC2E699B9C95B75393"),
        CELL_TEXT("74657874"),
        CELL_TEXT("2"),
        CELL_TEXT("C717530F41F320757B4AA1BFAF11C42E"),
        CELL_TEXT(""),
        CELL_TEXT("3"),
        CELL_NULL,
        CELL_NULL,
    };
    mylite_db *database = NULL;
    int failures = setup_database(&database);

    failures += execute_ok(
        database,
        "CREATE TABLE t (id INT PRIMARY KEY, v VARBINARY(32), k VARCHAR(32))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES (1, 'text', 'key'), (2, '', 'key'), (3, NULL, 'key')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, HEX(AES_ENCRYPT(v,k)), HEX(AES_DECRYPT(AES_ENCRYPT(v,k),k)) "
                   "FROM t ORDER BY id",
            .column_count = 3U,
            .values = projection_values,
            .row_count = 3U,
            .warning_count = 0U,
            .context = "table-backed AES projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t "
                   "WHERE HEX(AES_DECRYPT(AES_ENCRYPT(v,k),k)) = '74657874'",
            .column_count = 1U,
            .values = (const struct expected_cell[]){CELL_TEXT("1")},
            .row_count = 1U,
            .warning_count = 0U,
            .context = "table-backed AES decrypted predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t "
                   "WHERE HEX(AES_ENCRYPT(v,k)) = '15E36637363712FC2E699B9C95B75393'",
            .column_count = 1U,
            .values = (const struct expected_cell[]){CELL_TEXT("1")},
            .row_count = 1U,
            .warning_count = 0U,
            .context = "table-backed AES encrypted predicate",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_aes_metadata(void) {
    static const struct expected_column_metadata expected = {
        .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
        .charset_id = mysql_collation_binary_id,
        .collation_id = mysql_collation_binary_id,
        .display_length = mysql_json_document_display_length,
        .decimals = mysql_approximate_decimals,
        .nullable = true,
    };
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = setup_database(&database);

    failures += execute_ok(
        database,
        "SELECT AES_ENCRYPT('text','key') AS enc, "
        "AES_DECRYPT(AES_ENCRYPT('text','key'),'key') AS decv",
        &result
    );
    if (failures == 0) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 2U, "AES metadata columns");
        failures += expect_column_metadata(result, 0U, expected, "AES_ENCRYPT metadata");
        failures += expect_column_metadata(result, 1U, expected, "AES_DECRYPT metadata");
    }
    mylite_result_free(result);

    mylite_close(database);
    return failures;
}

static int setup_database(mylite_db **out_database) {
    int failures = mylite_test_expect_int(
        mylite_test_open_temporary(out_database),
        MYLITE_OK,
        "open temporary"
    );

    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
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
    for (size_t row = 0U; failures == 0 && row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t index = (row * expected.column_count) + column;

            failures +=
                expect_result_cell(result, row, column, expected.values[index], expected.context);
        }
    }
    mylite_result_free(result);
    return failures;
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
            fprintf(stderr, "%s row %zu column %zu: expected NULL\n", context, row, column);
            return 1;
        }
        return 0;
    }
    if (actual == NULL) {
        fprintf(stderr, "%s row %zu column %zu: expected non-NULL\n", context, row, column);
        return 1;
    }
    failures += mylite_test_expect_size(actual_size, expected.size, context);
    if (failures == 0 && expected.size != 0U) {
        failures += expect_bytes(actual, expected.bytes, expected.size, context);
    }
    return failures;
}

static int expect_warning(
    const mylite_result *result,
    size_t row,
    struct expected_warning expected,
    const char *context
) {
    const void *actual_message = mylite_result_value_bytes(result, row, 2U);
    size_t actual_message_size = mylite_result_value_size(result, row, 2U);
    int failures = 0;

    failures += expect_result_cell(
        result,
        row,
        1U,
        (struct expected_cell){
            (const unsigned char *)expected.code,
            strlen(expected.code),
            false,
        },
        context
    );
    failures +=
        expect_bytes_contains(actual_message, actual_message_size, expected.message_part, context);
    return failures;
}

static int expect_column_metadata(
    const mylite_result *result,
    size_t column,
    struct expected_column_metadata expected,
    const char *context
) {
    int failures = 0;

    failures += mylite_test_expect_int(
        (int)mylite_result_column_type(result, column),
        (int)expected.type,
        context
    );
    failures += mylite_test_expect_uint32(
        mylite_result_column_flags(result, column),
        expected.flags,
        context
    );
    failures += mylite_test_expect_uint32(
        mylite_result_column_charset_id(result, column),
        expected.charset_id,
        context
    );
    failures += mylite_test_expect_uint32(
        mylite_result_column_collation_id(result, column),
        expected.collation_id,
        context
    );
    failures += mylite_test_expect_uint64(
        mylite_result_column_display_length(result, column),
        expected.display_length,
        context
    );
    failures += mylite_test_expect_uint16(
        mylite_result_column_decimals(result, column),
        expected.decimals,
        context
    );
    failures += mylite_test_expect_int(
        mylite_result_column_nullable(result, column),
        expected.nullable,
        context
    );
    return failures;
}

static int expect_bytes_contains(
    const void *actual,
    size_t actual_size,
    const char *needle,
    const char *context
) {
    const unsigned char *bytes = actual;
    size_t needle_size = needle == NULL ? 0U : strlen(needle);

    if (bytes == NULL || needle == NULL || needle_size == 0U || actual_size < needle_size) {
        fprintf(stderr, "%s: expected warning message to contain [%s]\n", context, needle);
        return 1;
    }
    for (size_t index = 0U; index <= actual_size - needle_size; ++index) {
        if (memcmp(bytes + index, needle, needle_size) == 0) {
            return 0;
        }
    }
    fprintf(stderr, "%s: expected warning message to contain [%s]\n", context, needle);
    return 1;
}

static int expect_bytes(
    const void *actual,
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
