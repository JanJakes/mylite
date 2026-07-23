#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
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
    core_column_count = 15,
    precedence_column_count = 12,
    boundary_column_count = 11,
    mysql_error_parse = 1064,
    mysql_error_bigint_out_of_range = 1690,
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
    size_t warning_count;
    int64_t affected_rows;
    const char *context;
};

static int test_scalar_bitwise_values_and_file_safety(void);
static int test_scalar_bitwise_warnings_and_do(void);
static int test_scalar_bitwise_overflow_and_unsupported_forms(void);
static int test_scalar_bitwise_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_scalar_bitwise_values_and_file_safety();
    failures += test_scalar_bitwise_warnings_and_do();
    failures += test_scalar_bitwise_overflow_and_unsupported_forms();
    failures += test_scalar_bitwise_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_bitwise_values_and_file_safety(void) {
    static const char *const core_columns[] = {
        "5&3",
        "5|2",
        "5^3",
        "~0",
        "~1",
        "~(-1)",
        "1<<3",
        "8>>1",
        "TRUE&2",
        "FALSE|2",
        "NULL&1",
        "1|NULL",
        "~NULL",
        "NULL<<1",
        "1<<NULL",
    };
    static const char *const core_values[] = {
        "1",
        "7",
        "6",
        "18446744073709551615",
        "18446744073709551614",
        "0",
        "8",
        "4",
        "0",
        "2",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const precedence_columns[] = {
        "1+2&3",
        "1|2+4",
        "1<<2+1",
        "8>>1+1",
        "~-1",
        "~(1+2)",
        "1&2|4",
        "1|2&3",
        "1^3&2",
        "1&3^2",
        "1|2^3",
        "1^2|4",
    };
    static const char *const precedence_values[] = {
        "3",
        "7",
        "8",
        "2",
        "0",
        "18446744073709551612",
        "4",
        "3",
        "2",
        "1",
        "1",
        "7",
    };
    static const char *const boundary_columns[] = {
        "9223372036854775807&-1",
        "9223372036854775807|0",
        "-9223372036854775807&7",
        "(-9223372036854775807-1)&7",
        "(-9223372036854775807-1)|0",
        "~(-9223372036854775807-1)",
        "1<<63",
        "1<<64",
        "1<<-1",
        "-1>>1",
        "-1<<1",
    };
    static const char *const boundary_values[] = {
        "9223372036854775807",
        "9223372036854775807",
        "1",
        "0",
        "9223372036854775808",
        "9223372036854775807",
        "9223372036854775808",
        "0",
        "0",
        "9223372036854775807",
        "18446744073709551614",
    };
    static const char *const function_columns[] = {"a", "b", "c", "d"};
    static const char *const function_values[] = {"1", "7", NULL, "7"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE catalog_guard(id INT)", NULL);
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 5&3,5|2,5^3,~0,~1,~(-1),1<<3,8>>1,TRUE&2,FALSE|2,"
                   "NULL&1,1|NULL,~NULL,NULL<<1,1<<NULL",
            .columns = core_columns,
            .column_count = core_column_count,
            .values = core_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "core bitwise values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1+2&3,1|2+4,1<<2+1,8>>1+1,~-1,~(1+2),1&2|4,"
                   "1|2&3,1^3&2,1&3^2,1|2^3,1^2|4",
            .columns = precedence_columns,
            .column_count = precedence_column_count,
            .values = precedence_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "bitwise precedence",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 9223372036854775807&-1,9223372036854775807|0,"
                   "-9223372036854775807&7,(-9223372036854775807-1)&7,"
                   "(-9223372036854775807-1)|0,~(-9223372036854775807-1),"
                   "1<<63,1<<64,1<<-1,-1>>1,-1<<1",
            .columns = boundary_columns,
            .column_count = boundary_column_count,
            .values = boundary_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "bitwise signed boundaries",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT IFNULL(NULL,5)&3 AS a,5|IF(1,2,3) b,"
                   "~(NULLIF(5,5)) c,COALESCE(NULL,-1)&7 d FROM DUAL",
            .columns = function_columns,
            .column_count = 4U,
            .values = function_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "bitwise scalar function operands",
        }
    );

    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "bitwise leaves catalog generation"
    );
    failures += mylite_test_expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "bitwise leaves sqlite schema generation"
    );
    mylite_close(database);

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after bitwise reads"
    );
    remove_related_files(path);
    return failures;
}

static int test_scalar_bitwise_warnings_and_do(void) {
    static const char *const left_null_columns[] = {
        "NULL&5 DIV 0",
        "NULL<<5 DIV 0",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const left_null_values[] = {NULL, NULL, "0", "0"};
    static const char *const right_warning_columns[] = {
        "1<<5 DIV 0",
        "5 DIV 0&NULL",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const right_warning_values[] = {NULL, NULL, "0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "warnings") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open warnings file");
    failures += execute_ok(database, "DO 0", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NULL&5 DIV 0,NULL<<5 DIV 0,@@warning_count,ROW_COUNT()",
            .columns = left_null_columns,
            .column_count = 4U,
            .values = left_null_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "left null short circuit",
        }
    );
    failures += execute_ok(database, "DO 0", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1<<5 DIV 0,5 DIV 0&NULL,@@warning_count,ROW_COUNT()",
            .columns = right_warning_columns,
            .column_count = 4U,
            .values = right_warning_values,
            .row_count = 1U,
            .warning_count = 2U,
            .affected_rows = 0,
            .context = "right operand warnings",
        }
    );
    failures += execute_ok(database, "DO 5&3,~0,NULL&5 DIV 0", &result);
    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), 0U, "DO bitwise column count");
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), 0U, "DO bitwise row count");
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        0U,
        "DO bitwise warning count"
    );
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        0,
        "DO bitwise affected rows"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count,ROW_COUNT()",
            .columns = (const char *const[]){"@@warning_count", "ROW_COUNT()"},
            .column_count = 2U,
            .values = (const char *const[]){"0", "0"},
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "state after DO bitwise",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_scalar_bitwise_overflow_and_unsupported_forms(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open unsupported file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE t(id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1), (2), (NULL)", NULL);

    failures += execute_error(
        database,
        "SELECT 3037000500*3037000500&1",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT ~(3037000500*3037000500)",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1+(2&3)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT (1&2)=0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT CASE WHEN TRUE THEN 1&2 END",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT CASE supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT '5'&3",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "limited numeric bitwise",
        }
    );
    failures += execute_error(
        database,
        "SELECT 5.5&3",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "limited numeric bitwise",
        }
    );
    failures += execute_error(
        database,
        "SELECT X'40'|X'01'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "limited numeric bitwise",
        }
    );
    failures += execute_error(
        database,
        "SELECT 5&1/1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "limited numeric bitwise",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id&1 FROM t ORDER BY id",
            .columns = (const char *const[]){"id&1"},
            .column_count = 1U,
            .values = (const char *const[]){NULL, "1", "0"},
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "row bitwise projection",
        }
    );
    failures += execute_ok(database, "DO @@warning_count&1", NULL);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_scalar_bitwise_independent_handles(void) {
    static const char *const first_columns[] = {"~0"};
    static const char *const first_values[] = {"18446744073709551615"};
    static const char *const second_columns[] = {"1<<63"};
    static const char *const second_values[] = {"9223372036854775808"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&first), MYLITE_OK, "open first memory handle");
    failures +=
        mylite_test_expect_int(mylite_open_memory(&second), MYLITE_OK, "open second memory handle");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT ~0",
            .columns = first_columns,
            .column_count = 1U,
            .values = first_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first handle bitwise",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT 1<<63",
            .columns = second_columns,
            .column_count = 1U,
            .values = second_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second handle bitwise",
        }
    );

    mylite_close(first);
    mylite_close(second);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += mylite_test_expect_int(rc, MYLITE_OK, sql);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", sql, mylite_errmsg(database));
        return failures;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return failures;
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
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        expected.context
    );
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        expected.affected_rows,
        expected.context
    );
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[(row * expected.column_count) + column],
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

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(stderr, "%s: expected NULL at %zu,%zu, got %s\n", context, row, column, actual);
            return 1;
        }
        return 0;
    }
    return mylite_test_expect_text(actual, expected, context);
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(related)) {
        (void)remove(related);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0 || fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read file\n", path);
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }
    return 0;
}
