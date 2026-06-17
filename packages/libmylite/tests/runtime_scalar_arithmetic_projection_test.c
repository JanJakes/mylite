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
    path_suffix_capacity = 16,
    core_column_count = 22,
    alias_column_count = 4,
    parenthesized_column_count = 4,
    boundary_column_count = 4,
    mysql_error_parse = 1064,
    mysql_error_incorrect_parameter_count = 1582,
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
    const char *context;
};

static int test_scalar_arithmetic_values_and_file_safety(void);
static int test_scalar_arithmetic_overflow_and_unsupported_forms(void);
static int test_scalar_arithmetic_independent_handles(void);
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
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
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

    failures += test_scalar_arithmetic_values_and_file_safety();
    failures += test_scalar_arithmetic_overflow_and_unsupported_forms();
    failures += test_scalar_arithmetic_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_arithmetic_values_and_file_safety(void) {
    static const char *const core_columns[] = {
        "1+2*3",
        "(1+2)*3",
        "7-10",
        "3*-2",
        "TRUE+2",
        "FALSE*9",
        "NULL+1",
        "IF(1,2,3)+4",
        "IFNULL(NULL,5)*2",
        "COALESCE(NULL,7)-2",
        "NULLIF(8,8)+1",
        "ISNULL(NULL)+9",
        "-(1+2)",
        "+(1+2)",
        "- -1",
        "+ -1",
        "- +1",
        "1",
        "-NULL",
        "NULL",
        "-TRUE",
        "+FALSE",
    };
    static const char *const core_values[] = {
        "7",  "9",  "-3", "-6", "3",  "0",  NULL, "6",  "10", "5",  NULL,
        "10", "-3", "3",  "1",  "-1", "-1", "1",  NULL, NULL, "-1", "0",
    };
    static const char *const alias_columns[] = {"sum", "diff", "product", "nullable"};
    static const char *const alias_values[] = {"3", "-2", "12", NULL};
    static const char *const dual_unary_columns[] = {"neg_if", "pos_ifnull", "-TRUE"};
    static const char *const dual_unary_values[] = {"-2", "5", "-1"};
    static const char *const parenthesized_columns[] = {
        "(1+2)",
        "(1+2)*3",
        "(+2)+(-3)",
        "((1+2)*3)",
    };
    static const char *const parenthesized_values[] = {"3", "9", "-1", "9"};
    static const char *const boundary_columns[] = {
        "-9223372036854775807 - 1",
        "+(-9223372036854775807 - 1)",
        "3037000499*3037000499",
        "-3037000499*3037000499",
    };
    static const char *const boundary_values[] = {
        "-9223372036854775808",
        "-9223372036854775808",
        "9223372030926249001",
        "-9223372030926249001",
    };
    static const char *const row_count_columns[] = {"1+2", "@@warning_count", "ROW_COUNT()"};
    static const char *const row_count_values[] = {"3", "0", "0"};
    static const char *const following_count_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const following_count_values[] = {"0", "-1"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE rc_seed(id INT)", NULL);
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1+2*3, (1+2)*3, 7-10, 3*-2, TRUE+2, FALSE*9, "
                   "NULL+1, IF(1,2,3)+4, IFNULL(NULL,5)*2, "
                   "COALESCE(NULL,7)-2, NULLIF(8,8)+1, ISNULL(NULL)+9, "
                   "-(1+2), +(1+2), - -1, + -1, - +1, + +1, -NULL, +NULL, "
                   "-TRUE, +FALSE",
            .columns = core_columns,
            .column_count = core_column_count,
            .values = core_values,
            .row_count = 1U,
            .context = "core arithmetic values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ALL 1+2 AS sum, 5-7 diff, 3*4 product, NULL*9 AS "
                   "nullable FROM DUAL",
            .columns = alias_columns,
            .column_count = alias_column_count,
            .values = alias_values,
            .row_count = 1U,
            .context = "dual aliases and all",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT -(IF(1,2,3)) AS neg_if, +(IFNULL(NULL,5)) AS pos_ifnull, "
                   "-TRUE FROM DUAL",
            .columns = dual_unary_columns,
            .column_count = 3U,
            .values = dual_unary_values,
            .row_count = 1U,
            .context = "dual unary scalar function operands",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT (1+2), (1+2)*3, (+2)+(-3), ((1+2)*3)",
            .columns = parenthesized_columns,
            .column_count = parenthesized_column_count,
            .values = parenthesized_values,
            .row_count = 1U,
            .context = "parenthesized arithmetic labels",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT -9223372036854775807 - 1, "
                   "+(-9223372036854775807 - 1), 3037000499*3037000499, "
                   "-3037000499*3037000499",
            .columns = boundary_columns,
            .column_count = boundary_column_count,
            .values = boundary_values,
            .row_count = 1U,
            .context = "signed arithmetic boundaries",
        }
    );
    failures += execute_ok(database, "UPDATE rc_seed SET id = 1 WHERE id = 2", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1+2, @@warning_count, ROW_COUNT()",
            .columns = row_count_columns,
            .column_count = 3U,
            .values = row_count_values,
            .row_count = 1U,
            .context = "row count and warnings inside arithmetic select",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .columns = following_count_columns,
            .column_count = 2U,
            .values = following_count_values,
            .row_count = 1U,
            .context = "row count after arithmetic select",
        }
    );

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "scalar arithmetic leaves catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "scalar arithmetic leaves sqlite schema generation unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read arithmetic preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "scalar arithmetic leaves preamble unchanged"
    );

    remove_related_files(path);
    return failures;
}

static int test_scalar_arithmetic_overflow_and_unsupported_forms(void) {
    static const char *const columns_table_arithmetic[] = {"1+id"};
    static const char *const values_table_arithmetic[] = {NULL, "1", "2"};
    static const char *const columns_count_arithmetic[] = {"COUNT(*)+3"};
    static const char *const values_count_arithmetic[] = {"6"};
    static const char *const columns_unsigned_table_arithmetic[] = {"age*3"};
    static const char *const values_unsigned_table_arithmetic[] = {"81"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open unsupported file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE t(id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (0), (1), (NULL)", NULL);
    failures += execute_ok(database, "CREATE TABLE unsigned_t(age INT UNSIGNED)", NULL);
    failures += execute_ok(database, "INSERT INTO unsigned_t VALUES (27)", NULL);

    failures += execute_error(
        database,
        "SELECT 9223372036854775807+1",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT 9223372036854775807 - -1",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT 3037000500*3037000500",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT NULL * (9223372036854775807 + 1)",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT -(9223372036854775807+1)",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT -(-9223372036854775807 - 1)",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT 9223372036854775808 + 0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit integer operands",
        }
    );
    failures += execute_error(
        database,
        "SELECT -9223372036854775808 + 0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit integer operands",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1+1/0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit +, binary -, and * arithmetic",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 + '2'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit +, binary -, and * arithmetic",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1.5 + 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit +, binary -, and * arithmetic",
        }
    );
    failures += execute_error(
        database,
        "SELECT VERSION()+1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit +, binary -, and * arithmetic",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 + @@warning_count",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit +, binary -, and * arithmetic",
        }
    );
    failures += execute_error(
        database,
        "SELECT -'2'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit +, binary -, and * arithmetic",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 + ISNULL()",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'ISNULL'",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(1+0,2,3)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT IF() supports only signed 64-bit integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(-(1+2),2,3)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT IF() supports only signed 64-bit integer",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1+id FROM t ORDER BY id",
            .columns = columns_table_arithmetic,
            .column_count = sizeof(columns_table_arithmetic) / sizeof(columns_table_arithmetic[0]),
            .values = values_table_arithmetic,
            .row_count = 3U,
            .context = "table-backed scalar arithmetic projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*)+3 FROM t",
            .columns = columns_count_arithmetic,
            .column_count = sizeof(columns_count_arithmetic) / sizeof(columns_count_arithmetic[0]),
            .values = values_count_arithmetic,
            .row_count = 1U,
            .context = "table-backed count arithmetic projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT age*3 FROM unsigned_t",
            .columns = columns_unsigned_table_arithmetic,
            .column_count = sizeof(columns_unsigned_table_arithmetic) /
                            sizeof(columns_unsigned_table_arithmetic[0]),
            .values = values_unsigned_table_arithmetic,
            .row_count = 1U,
            .context = "table-backed unsigned scalar arithmetic projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT -id FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "row-scalar SELECT supports only signed integer literals",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1+2 WHERE TRUE",
            .columns = (const char *const[]){"1+2"},
            .column_count = 1U,
            .values = (const char *const[]){"3"},
            .row_count = 1U,
            .context = "arithmetic with where true",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1+2 LIMIT 1",
            .columns = (const char *const[]){"1+2"},
            .column_count = 1U,
            .values = (const char *const[]){"3"},
            .row_count = 1U,
            .context = "arithmetic with limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1+2 ORDER BY 1",
            .columns = (const char *const[]){"1+2"},
            .column_count = 1U,
            .values = (const char *const[]){"3"},
            .row_count = 1U,
            .context = "arithmetic with order by ordinal",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_scalar_arithmetic_independent_handles(void) {
    static const char *const first_columns[] = {"first_result", "-(1+2)"};
    static const char *const first_values[] = {"6", "-3"};
    static const char *const second_columns[] = {"second_result", "+(3*4)"};
    static const char *const second_values[] = {"10", "12"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first memory handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second memory handle");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT IF(1,2,3)+4 AS first_result, -(1+2)",
            .columns = first_columns,
            .column_count = 2U,
            .values = first_values,
            .row_count = 1U,
            .context = "first handle arithmetic",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT IFNULL(NULL,5)*2 AS second_result, +(3*4)",
            .columns = second_columns,
            .column_count = 2U,
            .values = second_values,
            .row_count = 1U,
            .context = "second handle arithmetic",
        }
    );

    mylite_close(second);
    mylite_close(first);
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

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-scalar-arithmetic-projection-%s-%d.mylite",
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
    size_t read_size = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    read_size = fread(buffer, 1U, size, file);
    fclose(file);

    return read_size == size ? 0 : 1;
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
