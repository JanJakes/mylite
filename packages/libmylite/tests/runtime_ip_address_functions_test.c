#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CELL_TEXT(value) {(const unsigned char *)(value), sizeof(value) - 1U, false}
#define CELL_NULL {NULL, 0U, true}

enum {
    mysql_error_native_function_argument_count = 1582,
    mysql_collation_binary_id = 63,
    mysql_collation_utf8mb4_0900_ai_ci_id = 255,
    mysql_inet_aton_display_length = 21,
    mysql_inet_ntoa_display_length = 31,
    mysql_approximate_decimals = 31,
    ip_address_warning_row_count = 6,
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

struct expected_column_metadata {
    enum mylite_result_column_type type;
    uint32_t flags;
    uint32_t charset_id;
    uint32_t collation_id;
    uint64_t display_length;
    uint16_t decimals;
    int nullable;
};

static int test_scalar_ip_address_functions(void);
static int test_ip_address_assignment_and_dml_contexts(void);
static int test_ip_address_dual_do_and_arity(void);
static int test_table_backed_ip_address_functions(void);
static int test_ip_address_metadata(void);
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
static int expect_column_metadata(
    const mylite_result *result,
    size_t column,
    struct expected_column_metadata expected,
    const char *context
);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_int(int actual, int expected, const char *context);
static int expect_uint32(uint32_t actual, uint32_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_uint16(uint16_t actual, uint16_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(const void *actual, const void *expected, size_t size, const char *context);

int main(void) {
    int failures = 0;

    failures += test_scalar_ip_address_functions();
    failures += test_ip_address_assignment_and_dml_contexts();
    failures += test_ip_address_dual_do_and_arity();
    failures += test_table_backed_ip_address_functions();
    failures += test_ip_address_metadata();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_ip_address_functions(void) {
    static const struct expected_cell scalar_values[] = {
        CELL_TEXT("2130706433"),
        CELL_TEXT("2130706433"),
        CELL_TEXT("167772161"),
        CELL_TEXT("1"),
        CELL_TEXT("0"),
        CELL_TEXT("4294967295"),
        CELL_NULL,
        CELL_TEXT("123"),
        CELL_TEXT("1"),
        CELL_TEXT("0"),
        CELL_TEXT("16777221"),
        CELL_TEXT("2130706433"),
        CELL_TEXT("0"),
    };
    static const struct expected_cell short_values[] = {
        CELL_TEXT("16909060"),
        CELL_TEXT("1"),
        CELL_TEXT("1"),
        CELL_TEXT("16777218"),
        CELL_TEXT("16908292"),
        CELL_TEXT("0"),
        CELL_TEXT("0"),
    };
    static const struct expected_cell warning_values[] = {
        CELL_TEXT("1"),
        CELL_TEXT("0.0.0.123"),
        CELL_TEXT("0.0.0.0"),
        CELL_TEXT("1"),
        CELL_TEXT("1"),
    };
    static const struct expected_cell warning_rows[] = {
        CELL_TEXT("Warning"),
        CELL_TEXT("1411"),
        CELL_TEXT("Incorrect string value: ''bad'' for function inet_aton"),
        CELL_TEXT("Warning"),
        CELL_TEXT("1292"),
        CELL_TEXT("Truncated incorrect INTEGER value: '123abc'"),
        CELL_TEXT("Warning"),
        CELL_TEXT("1292"),
        CELL_TEXT("Truncated incorrect BINARY value: 'x'3231''"),
        CELL_TEXT("Warning"),
        CELL_TEXT("1292"),
        CELL_TEXT("Truncated incorrect INTEGER value: '4294967296abc'"),
        CELL_TEXT("Warning"),
        CELL_TEXT("1411"),
        CELL_TEXT("Incorrect integer value: ''4294967296abc'' for function inet_ntoa"),
        CELL_TEXT("Warning"),
        CELL_TEXT("1411"),
        CELL_TEXT("Incorrect integer value: '-(1)' for function inet_ntoa"),
    };
    mylite_db *database = NULL;
    int failures = setup_database(&database);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT INET_ATON('127.0.0.1'), INET_ATON('127.1'), "
                   "INET_ATON('10.0.1'), INET_ATON('1'), INET_ATON('0.0.0.0'), "
                   "INET_ATON('255.255.255.255'), INET_ATON(NULL), INET_ATON(123), "
                   "INET_ATON(TRUE), INET_ATON(FALSE), INET_ATON(1.5), "
                   "INET_ATON(X'3132372E302E302E31'), @@warning_count",
            .column_count = sizeof(scalar_values) / sizeof(scalar_values[0]),
            .values = scalar_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "scalar INET_ATON values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT INET_ATON('01.002.003.004'), INET_ATON('.1'), "
                   "INET_ATON('..1'), INET_ATON('1..2'), INET_ATON('1.2..4'), "
                   "INET_ATON('0..0'), @@warning_count",
            .column_count = sizeof(short_values) / sizeof(short_values[0]),
            .values = short_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "scalar INET_ATON short forms",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT INET_ATON('bad') IS NULL, INET_NTOA('123abc'), "
                   "INET_NTOA(X'3231'), INET_NTOA('4294967296abc') IS NULL, "
                   "INET_NTOA(-1) IS NULL",
            .column_count = sizeof(warning_values) / sizeof(warning_values[0]),
            .values = warning_values,
            .row_count = 1U,
            .warning_count = ip_address_warning_row_count,
            .context = "ip address warning counts",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .column_count = 3U,
            .values = warning_rows,
            .row_count = ip_address_warning_row_count,
            .warning_count = 0U,
            .context = "ip address warning rows",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_ip_address_assignment_and_dml_contexts(void) {
    static const struct expected_cell assignment_values[] = {
        CELL_TEXT("2130706433"),
        CELL_TEXT("0.0.0.1"),
    };
    static const struct expected_cell inserted_values[] = {
        CELL_TEXT("2130706433"),
        CELL_TEXT("0.0.0.1"),
    };
    mylite_db *database = NULL;
    int failures = setup_database(&database);

    failures += execute_ok(database, "SET @aton = INET_ATON('127.0.0.1')", NULL);
    failures += execute_ok(database, "SET @ntoa = INET_NTOA(1)", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @aton, @ntoa",
            .column_count = 2U,
            .values = assignment_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "ip address user variable assignment",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE values_t(a BIGINT UNSIGNED, n VARCHAR(32))", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO values_t VALUES (INET_ATON('127.0.0.1'), INET_NTOA(1))",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT a, n FROM values_t",
            .column_count = 2U,
            .values = inserted_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "ip address source-free DML values",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_ip_address_dual_do_and_arity(void) {
    static const struct expected_cell values[] = {
        CELL_TEXT("127.0.0.1"),
        CELL_TEXT("0.0.0.1"),
    };
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = setup_database(&database);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT INET_NTOA(2130706433), INET_NTOA(1) FROM DUAL",
            .column_count = 2U,
            .values = values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "dual INET_NTOA values",
        }
    );
    failures += execute_ok(database, "DO INET_ATON('127.0.0.1'), INET_NTOA(1)", &result);
    failures += expect_size(mylite_result_column_count(result), 0U, "ip DO columns");
    failures += expect_size(mylite_result_row_count(result), 0U, "ip DO rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "ip DO warnings");
    mylite_result_free(result);

    failures += execute_error(
        database,
        "SELECT INET_ATON()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'INET_ATON'",
        }
    );
    failures += execute_error(
        database,
        "SELECT INET_NTOA(1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'INET_NTOA'",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_ip_address_functions(void) {
    static const struct expected_cell table_values[] = {
        CELL_TEXT("1"),
        CELL_TEXT("2130706433"),
        CELL_TEXT("127.0.0.1"),
        CELL_TEXT("2"),
        CELL_TEXT("167772161"),
        CELL_TEXT("10.0.0.1"),
        CELL_TEXT("3"),
        CELL_NULL,
        CELL_TEXT("255.255.255.255"),
        CELL_TEXT("4"),
        CELL_NULL,
        CELL_NULL,
    };
    static const struct expected_cell predicate_values[] = {
        CELL_TEXT("3"),
        CELL_TEXT("1"),
    };
    mylite_db *database = NULL;
    int failures = setup_database(&database);

    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT PRIMARY KEY, ip VARCHAR(32), n BIGINT UNSIGNED)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '127.0.0.1', 2130706433), "
        "(2, '10.0.1', 167772161), "
        "(3, 'bad', 4294967295), "
        "(4, NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, INET_ATON(ip), INET_NTOA(n) FROM t ORDER BY id",
            .column_count = 3U,
            .values = table_values,
            .row_count = 4U,
            .warning_count = 1U,
            .context = "table-backed ip functions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE "
                   "INET_ATON(ip) = 2130706433 OR INET_NTOA(n) = '255.255.255.255' "
                   "ORDER BY INET_ATON(ip), id",
            .column_count = 1U,
            .values = predicate_values,
            .row_count = 2U,
            .warning_count = 2U,
            .context = "table-backed ip predicate and ordering",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_ip_address_metadata(void) {
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = setup_database(&database);

    failures +=
        execute_ok(database, "SELECT INET_ATON('127.0.0.1') AS a, INET_NTOA(1) AS n", &result);
    failures += expect_column_metadata(
        result,
        0U,
        (struct expected_column_metadata){
            .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_UNSIGNED |
                     MYLITE_RESULT_COLUMN_FLAG_NUM,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = mysql_inet_aton_display_length,
            .decimals = 0U,
            .nullable = 1,
        },
        "INET_ATON metadata"
    );
    failures += expect_column_metadata(
        result,
        1U,
        (struct expected_column_metadata){
            .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            .flags = 0U,
            .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .display_length = mysql_inet_ntoa_display_length,
            .decimals = mysql_approximate_decimals,
            .nullable = 1,
        },
        "INET_NTOA metadata"
    );

    mylite_result_free(result);
    mylite_close(database);
    return failures;
}

static int setup_database(mylite_db **out_database) {
    int rc = mylite_test_open_temporary(out_database);
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(stderr, "failed to open temporary database: %d\n", rc);
        return 1;
    }
    failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    failures += execute_ok(*out_database, "USE app", NULL);
    failures += execute_ok(*out_database, "SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION'", NULL);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK, got %d %s %s\n",
            sql,
            rc,
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
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
        fprintf(stderr, "%s: expected error, got OK\n", sql);
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
    size_t value_index = 0U;

    if (failures != 0) {
        return failures;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            failures += expect_result_cell(
                result,
                row,
                column,
                expected.values[value_index],
                expected.context
            );
            ++value_index;
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
            fprintf(stderr, "%s: expected NULL at %zu,%zu\n", context, row, column);
            return 1;
        }
        return 0;
    }
    if (actual == NULL) {
        fprintf(stderr, "%s: expected value at %zu,%zu\n", context, row, column);
        return 1;
    }
    failures += expect_size(actual_size, expected.size, context);
    if (failures == 0) {
        failures += expect_bytes(actual, expected.bytes, expected.size, context);
    }
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

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected size %zu, got %zu\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_uint32(uint32_t actual, uint32_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %" PRIu32 ", got %" PRIu32 "\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %" PRIu64 ", got %" PRIu64 "\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_uint16(uint16_t actual, uint16_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %" PRIu16 ", got %" PRIu16 "\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected [%s], got [%s]\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(stderr, "%s: expected [%s] to contain [%s]\n", context, actual, needle);
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
    if (size != 0U && memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
