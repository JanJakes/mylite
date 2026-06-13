#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CELL_TEXT(value) {(const unsigned char *)(value), sizeof(value) - 1U, false}
#define CELL_EMPTY {NULL, 0U, false}
#define CELL_NULL {NULL, 0U, true}

enum {
    mysql_error_random_bytes_range = 1690,
    mysql_error_native_function_argument_count = 1582,
    mysql_collation_binary_id = 63,
    mysql_compress_abc_size = 15,
    mysql_compress_abc_display_length = 21,
    mysql_uncompress_display_length = 16777216,
    mysql_uncompressed_length_display_length = 10,
    mysql_random_bytes_display_length = 1024,
    mysql_approximate_decimals = 31,
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

static int test_scalar_compression_random_functions(void);
static int test_compression_random_dual_do_and_arity(void);
static int test_table_backed_compression_random_functions(void);
static int test_compression_random_diagnostics(void);
static int test_compression_random_metadata(void);
static int setup_database(mylite_db **out_database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_single_value_size(
    mylite_db *database,
    const char *sql,
    size_t expected_size,
    size_t expected_warning_count,
    const char *context
);
static int expect_single_value_size_and_prefix(
    mylite_db *database,
    const char *sql,
    size_t expected_size,
    const void *expected_prefix,
    size_t expected_prefix_size,
    const char *context
);
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
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_uint32(uint32_t actual, uint32_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_uint16(uint16_t actual, uint16_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes_contains(
    const void *actual,
    size_t actual_size,
    const char *needle,
    const char *context
);
static int expect_bytes(const void *actual, const void *expected, size_t size, const char *context);

int main(void) {
    int failures = 0;

    failures += test_scalar_compression_random_functions();
    failures += test_compression_random_dual_do_and_arity();
    failures += test_table_backed_compression_random_functions();
    failures += test_compression_random_diagnostics();
    failures += test_compression_random_metadata();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_compression_random_functions(void) {
    static const unsigned char compressed_abc_prefix[] = {0x03, 0x00, 0x00, 0x00};
    static const struct expected_cell values[] = {
        CELL_TEXT("abc"),
        CELL_TEXT("3"),
        CELL_EMPTY,
        CELL_EMPTY,
        CELL_TEXT("0"),
        CELL_TEXT("1"),
        CELL_TEXT("1"),
        CELL_TEXT("1"),
        CELL_TEXT("1"),
    };
    mylite_db *database = NULL;
    int failures = setup_database(&database);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT UNCOMPRESS(COMPRESS('abc')), "
                   "UNCOMPRESSED_LENGTH(COMPRESS('abc')), COMPRESS(''), "
                   "UNCOMPRESS(COMPRESS('')), UNCOMPRESSED_LENGTH(COMPRESS('')), "
                   "COMPRESS(NULL) IS NULL, UNCOMPRESS(NULL) IS NULL, "
                   "UNCOMPRESSED_LENGTH(NULL) IS NULL, RANDOM_BYTES(NULL) IS NULL",
            .column_count = sizeof(values) / sizeof(values[0]),
            .values = values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "scalar compression and random bytes",
        }
    );
    failures += expect_single_value_size_and_prefix(
        database,
        "SELECT COMPRESS('abc')",
        mysql_compress_abc_size,
        compressed_abc_prefix,
        sizeof(compressed_abc_prefix),
        "scalar compress result shape"
    );
    failures += expect_single_value_size(
        database,
        "SELECT RANDOM_BYTES(4)",
        4U,
        0U,
        "scalar random bytes length"
    );

    mylite_close(database);
    return failures;
}

static int test_compression_random_dual_do_and_arity(void) {
    static const struct expected_cell values[] = {
        CELL_TEXT("abc"),
        CELL_TEXT("3"),
    };
    mylite_db *database = NULL;
    int failures = setup_database(&database);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT UNCOMPRESS(COMPRESS('abc')), "
                   "UNCOMPRESSED_LENGTH(COMPRESS('abc')) FROM DUAL",
            .column_count = 2U,
            .values = values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "dual compression functions",
        }
    );
    failures += execute_ok(
        database,
        "DO COMPRESS('abc'), UNCOMPRESS(COMPRESS('abc')), "
        "UNCOMPRESSED_LENGTH(COMPRESS('abc')), RANDOM_BYTES(1)",
        NULL
    );
    failures += execute_error(
        database,
        "SELECT COMPRESS()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'COMPRESS'",
        }
    );
    failures += execute_error(
        database,
        "SELECT RANDOM_BYTES(1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'RANDOM_BYTES'",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_compression_random_functions(void) {
    static const struct expected_cell values[] = {
        CELL_TEXT("1"),
        CELL_TEXT("abc"),
        CELL_TEXT("3"),
        CELL_TEXT("2"),
        CELL_EMPTY,
        CELL_TEXT("0"),
        CELL_TEXT("3"),
        CELL_NULL,
        CELL_NULL,
    };
    mylite_db *database = NULL;
    int failures = setup_database(&database);

    failures += execute_ok(
        database,
        "CREATE TABLE t (id INT PRIMARY KEY, v VARCHAR(20), vb VARBINARY(20))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES (1,'abc',X'610062'),(2,'',X''),(3,NULL,NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, UNCOMPRESS(COMPRESS(v)), UNCOMPRESSED_LENGTH(COMPRESS(vb)) "
                   "FROM t ORDER BY id",
            .column_count = 3U,
            .values = values,
            .row_count = 3U,
            .warning_count = 0U,
            .context = "table-backed compression and random bytes",
        }
    );
    failures += expect_single_value_size(
        database,
        "SELECT RANDOM_BYTES(id) FROM t WHERE id = 3",
        3U,
        0U,
        "table-backed random bytes length"
    );

    mylite_close(database);
    return failures;
}

static int test_compression_random_diagnostics(void) {
    static const struct expected_cell invalid_compressed_values[] = {
        CELL_NULL,
    };
    mylite_db *database = NULL;
    mylite_result *warnings = NULL;
    int failures = setup_database(&database);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT UNCOMPRESS('abc')",
            .column_count = 1U,
            .values = invalid_compressed_values,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "invalid compressed payload warnings",
        }
    );
    failures += execute_ok(database, "SHOW WARNINGS", &warnings);
    if (failures == 0) {
        failures += expect_size(mylite_result_row_count(warnings), 1U, "zlib warnings rows");
        failures += expect_warning(
            warnings,
            0U,
            (struct expected_warning){.code = "1259", .message_part = "ZLIB"},
            "first zlib warning"
        );
    }
    mylite_result_free(warnings);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT UNCOMPRESSED_LENGTH('abc')",
            .column_count = 1U,
            .values = (const struct expected_cell[]){CELL_TEXT("0")},
            .row_count = 1U,
            .warning_count = 1U,
            .context = "invalid compressed length warning",
        }
    );

    failures += expect_single_value_size(
        database,
        "SELECT RANDOM_BYTES('4x')",
        4U,
        1U,
        "random bytes truncated string length"
    );
    warnings = NULL;
    failures += execute_ok(database, "SHOW WARNINGS", &warnings);
    if (failures == 0) {
        failures += expect_size(mylite_result_row_count(warnings), 1U, "random warning rows");
        failures += expect_warning(
            warnings,
            0U,
            (struct expected_warning){.code = "1292", .message_part = "Truncated"},
            "random warning"
        );
    }
    mylite_result_free(warnings);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT UNCOMPRESS('abc') IS NULL, UNCOMPRESSED_LENGTH('abc')",
            .column_count = 2U,
            .values =
                (const struct expected_cell[]){
                    CELL_TEXT("1"),
                    CELL_TEXT("0"),
                },
            .row_count = 1U,
            .warning_count = 2U,
            .context = "multiple invalid compressed payload warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT UNCOMPRESS(X'FFFFFFFF00') IS NULL, "
                   "UNCOMPRESSED_LENGTH(X'FFFFFFFF00')",
            .column_count = 2U,
            .values =
                (const struct expected_cell[]){
                    CELL_TEXT("1"),
                    CELL_TEXT("1073741823"),
                },
            .row_count = 1U,
            .warning_count = 1U,
            .context = "oversized invalid compressed payload warnings",
        }
    );
    warnings = NULL;
    failures += execute_ok(database, "SHOW WARNINGS", &warnings);
    if (failures == 0) {
        failures +=
            expect_size(mylite_result_row_count(warnings), 1U, "oversized compressed warning rows");
        failures += expect_warning(
            warnings,
            0U,
            (struct expected_warning){
                .code = "1256",
                .message_part = "Uncompressed data size too large",
            },
            "oversized compressed warning"
        );
    }
    mylite_result_free(warnings);

    failures += execute_error(
        database,
        "SELECT RANDOM_BYTES(0)",
        (struct expected_sql_error){
            .code = mysql_error_random_bytes_range,
            .sqlstate = "22003",
            .message_part = "length value is out of range in 'random_bytes'",
        }
    );
    failures += execute_error(
        database,
        "SELECT RANDOM_BYTES(1025)",
        (struct expected_sql_error){
            .code = mysql_error_random_bytes_range,
            .sqlstate = "22003",
            .message_part = "length value is out of range in 'random_bytes'",
        }
    );
    failures += execute_error(
        database,
        "SELECT RANDOM_BYTES('abc')",
        (struct expected_sql_error){
            .code = mysql_error_random_bytes_range,
            .sqlstate = "22003",
            .message_part = "length value is out of range in 'random_bytes'",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_compression_random_metadata(void) {
    static const struct expected_column_metadata expected[] = {
        {
            .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = mysql_compress_abc_display_length,
            .decimals = mysql_approximate_decimals,
            .nullable = true,
        },
        {
            .type = MYLITE_RESULT_COLUMN_TYPE_BLOB,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = mysql_uncompress_display_length,
            .decimals = mysql_approximate_decimals,
            .nullable = true,
        },
        {
            .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = mysql_uncompressed_length_display_length,
            .decimals = 0U,
            .nullable = true,
        },
        {
            .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = mysql_random_bytes_display_length,
            .decimals = mysql_approximate_decimals,
            .nullable = true,
        },
    };
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = setup_database(&database);

    failures += execute_ok(
        database,
        "SELECT COMPRESS('abc') AS c, UNCOMPRESS(COMPRESS('abc')) AS u, "
        "UNCOMPRESSED_LENGTH(COMPRESS('abc')) AS l, RANDOM_BYTES(4) AS r",
        &result
    );
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 4U, "metadata columns");
        for (size_t column = 0U; column < 4U; ++column) {
            failures += expect_column_metadata(result, column, expected[column], "metadata");
        }
    }
    mylite_result_free(result);

    mylite_close(database);
    return failures;
}

static int setup_database(mylite_db **out_database) {
    int failures =
        expect_int(mylite_test_open_temporary(out_database), MYLITE_OK, "open temporary");

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

static int expect_single_value_size(
    mylite_db *database,
    const char *sql,
    size_t expected_size,
    size_t expected_warning_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 1U, context);
        failures += expect_size(mylite_result_row_count(result), 1U, context);
        failures +=
            expect_size(mylite_result_warning_count(result), expected_warning_count, context);
    }
    if (failures == 0) {
        const void *actual = mylite_result_value_bytes(result, 0U, 0U);

        if (actual == NULL) {
            fprintf(stderr, "%s: expected non-NULL value\n", context);
            failures += 1;
        } else {
            failures +=
                expect_size(mylite_result_value_size(result, 0U, 0U), expected_size, context);
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_single_value_size_and_prefix(
    mylite_db *database,
    const char *sql,
    size_t expected_size,
    const void *expected_prefix,
    size_t expected_prefix_size,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 1U, context);
        failures += expect_size(mylite_result_row_count(result), 1U, context);
        failures += expect_size(mylite_result_warning_count(result), 0U, context);
    }
    if (failures == 0) {
        const void *actual = mylite_result_value_bytes(result, 0U, 0U);
        size_t actual_size = mylite_result_value_size(result, 0U, 0U);

        if (actual == NULL) {
            fprintf(stderr, "%s: expected non-NULL value\n", context);
            failures += 1;
        } else {
            failures += expect_size(actual_size, expected_size, context);
            if (failures == 0) {
                failures += expect_bytes(actual, expected_prefix, expected_prefix_size, context);
            }
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
    failures += expect_size(actual_size, expected.size, context);
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

    failures +=
        expect_int((int)mylite_result_column_type(result, column), (int)expected.type, context);
    failures += expect_uint32(mylite_result_column_flags(result, column), expected.flags, context);
    failures += expect_uint32(
        mylite_result_column_charset_id(result, column),
        expected.charset_id,
        context
    );
    failures += expect_uint32(
        mylite_result_column_collation_id(result, column),
        expected.collation_id,
        context
    );
    failures += expect_uint64(
        mylite_result_column_display_length(result, column),
        expected.display_length,
        context
    );
    failures +=
        expect_uint16(mylite_result_column_decimals(result, column), expected.decimals, context);
    failures +=
        expect_int(mylite_result_column_nullable(result, column), expected.nullable, context);
    return failures;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected size %zu, got %zu\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_uint32(uint32_t actual, uint32_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %u, got %u\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %llu, got %llu\n",
            context,
            (unsigned long long)expected,
            (unsigned long long)actual
        );
        return 1;
    }
    return 0;
}

static int expect_uint16(uint16_t actual, uint16_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %u, got %u\n", context, (unsigned)expected, (unsigned)actual);
        return 1;
    }
    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "NULL" : actual,
            needle
        );
        return 1;
    }
    return 0;
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
