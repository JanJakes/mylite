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
    path_suffix_capacity = 16,
    core_column_count = 12,
    null_column_count = 8,
    precedence_column_count = 10,
    numeric_string_column_count = 6,
    like_column_count = 7,
    boundary_column_count = 4,
    warning_column_count = 6,
    comparison_warning_count = 2,
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

static int test_scalar_comparison_values_and_file_safety(void);
static int test_scalar_comparison_warnings_and_diagnostics(void);
static int test_scalar_comparison_overflow_and_unsupported_forms(void);
static int test_scalar_comparison_independent_handles(void);
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

    failures += test_scalar_comparison_values_and_file_safety();
    failures += test_scalar_comparison_warnings_and_diagnostics();
    failures += test_scalar_comparison_overflow_and_unsupported_forms();
    failures += test_scalar_comparison_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_comparison_values_and_file_safety(void) {
    static const char *const core_columns[] = {
        "1=1",
        "1=2",
        "1<>2",
        "1!=1",
        "1<2",
        "2<=2",
        "3>2",
        "3>=4",
        "TRUE=1",
        "FALSE=0",
        "-1<0",
        "0>=-1",
    };
    static const char *const core_values[] = {
        "1",
        "0",
        "1",
        "0",
        "1",
        "1",
        "1",
        "0",
        "1",
        "1",
        "1",
        "1",
    };
    static const char *const null_columns[] = {
        "1=NULL",
        "NULL=NULL",
        "1<>NULL",
        "NULL<NULL",
        "1<=>NULL",
        "NULL<=>NULL",
        "1<=>1",
        "1<=>2",
    };
    static const char *const null_values[] = {NULL, NULL, NULL, NULL, "0", "1", "1", "0"};
    static const char *const precedence_columns[] = {
        "1+2=3",
        "1+2*3=7",
        "1<2=1",
        "1=2<3",
        "2<1=0",
        "1<2<3",
        "3>2>1",
        "NULL=NULL<=>NULL",
        "NULL<=>NULL=1",
        "(NULL<=>NULL)=1",
    };
    static const char *const precedence_values[] = {
        "1",
        "1",
        "1",
        "1",
        "1",
        "1",
        "0",
        "1",
        "1",
        "1",
    };
    static const char *const function_columns[] = {"a", "b", "c", "d"};
    static const char *const function_values[] = {"1", "1", "1", "1"};
    static const char *const numeric_string_columns[] = {
        "'00.42'=0.4200",
        "0+'00.42'=0.4200",
        "0+'1234abcd'=1234",
        "'3.5'>3",
        "0+'-2.5'<-2",
        "1.0<=>1",
    };
    static const char *const numeric_string_values[] = {"1", "1", "1", "1", "1", "1"};
    static const char *const like_columns[] = {
        "prefix_match",
        "prefix_miss",
        "case_fold",
        "escaped_match",
        "escaped_miss",
        "null_value",
        "null_pattern",
    };
    static const char *const like_values[] = {"1", "0", "1", "1", "0", NULL, NULL};
    static const char *const boundary_columns[] = {
        "9223372036854775807=9223372036854775807",
        "-9223372036854775807<0",
        "(-9223372036854775807-1)<-9223372036854775807",
        "(-9223372036854775807-1)<=> (-9223372036854775807-1)",
    };
    static const char *const boundary_values[] = {"1", "1", "1", "1"};
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
            .sql = "SELECT 1=1, 1=2, 1<>2, 1!=1, 1<2, 2<=2, 3>2, 3>=4, "
                   "TRUE=1, FALSE=0, -1<0, 0>=-1",
            .columns = core_columns,
            .column_count = core_column_count,
            .values = core_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "core comparison values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1=NULL, NULL=NULL, 1<>NULL, NULL<NULL, 1<=>NULL, "
                   "NULL<=>NULL, 1<=>1, 1<=>2",
            .columns = null_columns,
            .column_count = null_column_count,
            .values = null_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "comparison null values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1+2=3, 1+2*3=7, 1<2=1, 1=2<3, 2<1=0, 1<2<3, "
                   "3>2>1, NULL=NULL<=>NULL, NULL<=>NULL=1, (NULL<=>NULL)=1",
            .columns = precedence_columns,
            .column_count = precedence_column_count,
            .values = precedence_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "comparison precedence",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT IFNULL(NULL,5)=5 AS a, NULLIF(5,5)<=>NULL b, "
                   "ISNULL(NULL)=TRUE c, COALESCE(NULL,2)>=2 d FROM DUAL",
            .columns = function_columns,
            .column_count = 4U,
            .values = function_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "comparison scalar function operands",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT '00.42'=0.4200, 0+'00.42'=0.4200, "
                   "0+'1234abcd'=1234, '3.5'>3, 0+'-2.5'<-2, 1.0<=>1",
            .columns = numeric_string_columns,
            .column_count = numeric_string_column_count,
            .values = numeric_string_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "comparison numeric string coercion",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 'alpha' LIKE 'a%' AS prefix_match, "
                   "'beta' LIKE 'a%' AS prefix_miss, "
                   "'Alpha' LIKE 'a%' AS case_fold, "
                   "'a_%' LIKE 'a\\_\\%' AS escaped_match, "
                   "'aX%' LIKE 'a\\_\\%' AS escaped_miss, "
                   "NULL LIKE 'a%' AS null_value, "
                   "'alpha' LIKE NULL AS null_pattern",
            .columns = like_columns,
            .column_count = like_column_count,
            .values = like_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "scalar LIKE comparison values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 9223372036854775807=9223372036854775807, "
                   "-9223372036854775807<0, "
                   "(-9223372036854775807-1)<-9223372036854775807, "
                   "(-9223372036854775807-1)<=> (-9223372036854775807-1)",
            .columns = boundary_columns,
            .column_count = boundary_column_count,
            .values = boundary_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "comparison signed boundaries",
        }
    );

    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "scalar comparison leaves catalog generation unchanged"
    );
    failures += mylite_test_expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "scalar comparison leaves sqlite schema generation unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures += mylite_test_expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read comparison preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "scalar comparison leaves preamble unchanged"
    );

    remove_related_files(path);
    return failures;
}

static int test_scalar_comparison_warnings_and_diagnostics(void) {
    static const char *const warning_columns[] = {
        "NULL=5 DIV 0",
        "5 DIV 0<=>NULL",
        "(NULL=NULL)=5 DIV 0",
        "1=5 DIV 0",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {NULL, "1", NULL, NULL, "0", "0"};
    static const char *const show_warning_columns[] = {"Level", "Code", "Message"};
    static const char *const show_warning_values[] = {
        "Warning",
        "1365",
        "Division by 0",
        "Warning",
        "1365",
        "Division by 0",
    };
    static const char *const following_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const following_values[] = {"2", "-1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "warnings") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open warnings file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE rc_seed(id INT)", NULL);
    failures += execute_ok(database, "UPDATE rc_seed SET id = 1 WHERE id = 2", NULL);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NULL=5 DIV 0, 5 DIV 0<=>NULL, (NULL=NULL)=5 DIV 0, "
                   "1=5 DIV 0, @@warning_count, ROW_COUNT()",
            .columns = warning_columns,
            .column_count = warning_column_count,
            .values = warning_values,
            .row_count = 1U,
            .warning_count = comparison_warning_count,
            .affected_rows = 0,
            .context = "comparison child warning values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = show_warning_columns,
            .column_count = 3U,
            .values = show_warning_values,
            .row_count = comparison_warning_count,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "comparison show warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .columns = following_columns,
            .column_count = 2U,
            .values = following_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "comparison following diagnostics",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_scalar_comparison_overflow_and_unsupported_forms(void) {
    static const char *const parenthesized_row_columns[] = {"(1,2)=(1,2)"};
    static const char *const parenthesized_row_values[] = {"1"};
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

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT (1,2)=(1,2)",
            .columns = parenthesized_row_columns,
            .column_count = 1U,
            .values = parenthesized_row_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "parenthesized row comparison value",
        }
    );
    failures += execute_error(
        database,
        "SELECT 3037000500*3037000500 = 1",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT 0x31=49",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar comparison",
        }
    );
    failures += execute_error(
        database,
        "SELECT b'1'=1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar comparison",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count=0",
            .columns = (const char *const[]){"@@warning_count=0"},
            .column_count = 1U,
            .values = (const char *const[]){"0"},
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "system variable comparison operand",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1+(2=2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT id=1 FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_scalar_comparison_independent_handles(void) {
    static const char *const first_columns[] = {"first_result", "NULL<=>NULL"};
    static const char *const first_values[] = {"1", "1"};
    static const char *const second_columns[] = {"second_result", "3>4"};
    static const char *const second_values[] = {"0", "0"};
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
            .sql = "SELECT IFNULL(NULL,5)=5 AS first_result, NULL<=>NULL",
            .columns = first_columns,
            .column_count = 2U,
            .values = first_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first handle comparison",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT NULLIF(5,5)<=>1 AS second_result, 3>4",
            .columns = second_columns,
            .column_count = 2U,
            .values = second_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second handle comparison",
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
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        expected.affected_rows,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
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
