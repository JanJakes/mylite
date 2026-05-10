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
    truth_column_count = 6,
    operand_column_count = 8,
    nested_column_count = 10,
    precedence_column_count = 6,
    boundary_column_count = 6,
    warning_column_count = 6,
    short_circuit_column_count = 5,
    child_warning_count = 4,
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

static int test_scalar_is_values_and_file_safety(void);
static int test_scalar_is_warnings_and_diagnostics(void);
static int test_scalar_is_overflow_and_unsupported_forms(void);
static int test_scalar_is_independent_handles(void);
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

    failures += test_scalar_is_values_and_file_safety();
    failures += test_scalar_is_warnings_and_diagnostics();
    failures += test_scalar_is_overflow_and_unsupported_forms();
    failures += test_scalar_is_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_is_values_and_file_safety(void) {
    static const char *const true_columns[] = {
        "one_true",
        "zero_true",
        "neg_true",
        "null_true",
        "true_true",
        "false_true",
    };
    static const char *const true_values[] = {"1", "0", "1", "0", "1", "0"};
    static const char *const false_columns[] = {
        "one_false",
        "zero_false",
        "neg_false",
        "null_false",
        "true_false",
        "false_false",
    };
    static const char *const false_values[] = {"0", "1", "0", "0", "0", "1"};
    static const char *const unknown_columns[] = {
        "one_unknown",
        "zero_unknown",
        "neg_unknown",
        "null_unknown",
        "true_unknown",
        "false_unknown",
    };
    static const char *const unknown_values[] = {"0", "0", "0", "1", "0", "0"};
    static const char *const not_true_columns[] = {
        "one_not_true",
        "zero_not_true",
        "neg_not_true",
        "null_not_true",
        "true_not_true",
        "false_not_true",
    };
    static const char *const not_true_values[] = {"0", "1", "0", "1", "0", "1"};
    static const char *const not_false_columns[] = {
        "one_not_false",
        "zero_not_false",
        "neg_not_false",
        "null_not_false",
        "true_not_false",
        "false_not_false",
    };
    static const char *const not_false_values[] = {"1", "0", "1", "1", "1", "0"};
    static const char *const not_unknown_columns[] = {
        "one_not_unknown",
        "zero_not_unknown",
        "neg_not_unknown",
        "null_not_unknown",
        "true_not_unknown",
        "false_not_unknown",
    };
    static const char *const not_unknown_values[] = {"1", "1", "1", "0", "1", "1"};
    static const char *const null_columns[] = {
        "one_null",
        "zero_null",
        "neg_null",
        "null_null",
        "true_null",
        "false_null",
    };
    static const char *const null_values[] = {"0", "0", "0", "1", "0", "0"};
    static const char *const not_null_columns[] = {
        "one_not_null",
        "zero_not_null",
        "neg_not_null",
        "null_not_null",
        "true_not_null",
        "false_not_null",
    };
    static const char *const not_null_values[] = {"1", "1", "1", "0", "1", "1"};
    static const char *const operand_columns[] = {
        "1+2 IS TRUE",
        "0*3 IS FALSE",
        "5 DIV 2 IS TRUE",
        "5 % 2 IS TRUE",
        "a",
        "b",
        "c",
        "d",
    };
    static const char *const operand_values[] = {"1", "1", "1", "1", "1", "1", "1", "1"};
    static const char *const nested_columns[] = {
        "(1=1) IS TRUE",
        "(1=NULL) IS UNKNOWN",
        "(NULL<=>NULL) IS TRUE",
        "(1 AND 0) IS FALSE",
        "(1 OR NULL) IS TRUE",
        "(NULL XOR 1) IS UNKNOWN",
        "(1 IS TRUE) IS TRUE",
        "(1 IS NULL) IS FALSE",
        "(1 IS TRUE)=1",
        "1=(1 IS TRUE)",
    };
    static const char *const nested_values[] = {
        "1",
        "1",
        "1",
        "1",
        "1",
        "1",
        "1",
        "1",
        "1",
        "1",
    };
    static const char *const precedence_columns[] = {
        "NOT 1 IS TRUE",
        "NOT (1 IS TRUE)",
        "1 IS TRUE AND 0",
        "1 AND 0 IS FALSE",
        "1 IS TRUE XOR 0",
        "0 OR NULL IS UNKNOWN",
    };
    static const char *const precedence_values[] = {"0", "0", "0", "1", "1", "1"};
    static const char *const boundary_columns[] = {
        "9223372036854775807 IS TRUE",
        "-9223372036854775807 IS TRUE",
        "(-9223372036854775807-1) IS TRUE",
        "0 IS TRUE",
        "0 IS FALSE",
        "NULL IS NOT TRUE",
    };
    static const char *const boundary_values[] = {"1", "1", "1", "0", "1", "1"};
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
    failures += execute_ok(database, "CREATE TABLE catalog_guard(id INT)", NULL);
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 IS TRUE AS one_true, 0 IS TRUE AS zero_true, "
                   "-1 IS TRUE AS neg_true, NULL IS TRUE AS null_true, "
                   "TRUE IS TRUE AS true_true, FALSE IS TRUE AS false_true",
            .columns = true_columns,
            .column_count = truth_column_count,
            .values = true_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is true truth table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 IS FALSE AS one_false, 0 IS FALSE AS zero_false, "
                   "-1 IS FALSE AS neg_false, NULL IS FALSE AS null_false, "
                   "TRUE IS FALSE AS true_false, FALSE IS FALSE AS false_false",
            .columns = false_columns,
            .column_count = truth_column_count,
            .values = false_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is false truth table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 IS UNKNOWN AS one_unknown, 0 IS UNKNOWN AS zero_unknown, "
                   "-1 IS UNKNOWN AS neg_unknown, NULL IS UNKNOWN AS null_unknown, "
                   "TRUE IS UNKNOWN AS true_unknown, FALSE IS UNKNOWN AS false_unknown",
            .columns = unknown_columns,
            .column_count = truth_column_count,
            .values = unknown_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is unknown truth table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 IS NOT TRUE AS one_not_true, 0 IS NOT TRUE AS zero_not_true, "
                   "-1 IS NOT TRUE AS neg_not_true, NULL IS NOT TRUE AS null_not_true, "
                   "TRUE IS NOT TRUE AS true_not_true, FALSE IS NOT TRUE AS false_not_true",
            .columns = not_true_columns,
            .column_count = truth_column_count,
            .values = not_true_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is not true truth table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 IS NOT FALSE AS one_not_false, 0 IS NOT FALSE AS zero_not_false, "
                   "-1 IS NOT FALSE AS neg_not_false, NULL IS NOT FALSE AS null_not_false, "
                   "TRUE IS NOT FALSE AS true_not_false, FALSE IS NOT FALSE AS false_not_false",
            .columns = not_false_columns,
            .column_count = truth_column_count,
            .values = not_false_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is not false truth table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 IS NOT UNKNOWN AS one_not_unknown, "
                   "0 IS NOT UNKNOWN AS zero_not_unknown, "
                   "-1 IS NOT UNKNOWN AS neg_not_unknown, "
                   "NULL IS NOT UNKNOWN AS null_not_unknown, "
                   "TRUE IS NOT UNKNOWN AS true_not_unknown, "
                   "FALSE IS NOT UNKNOWN AS false_not_unknown",
            .columns = not_unknown_columns,
            .column_count = truth_column_count,
            .values = not_unknown_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is not unknown truth table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 IS NULL AS one_null, 0 IS NULL AS zero_null, "
                   "-1 IS NULL AS neg_null, NULL IS NULL AS null_null, "
                   "TRUE IS NULL AS true_null, FALSE IS NULL AS false_null",
            .columns = null_columns,
            .column_count = truth_column_count,
            .values = null_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is null truth table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 IS NOT NULL AS one_not_null, 0 IS NOT NULL AS zero_not_null, "
                   "-1 IS NOT NULL AS neg_not_null, NULL IS NOT NULL AS null_not_null, "
                   "TRUE IS NOT NULL AS true_not_null, FALSE IS NOT NULL AS false_not_null",
            .columns = not_null_columns,
            .column_count = truth_column_count,
            .values = not_null_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is not null truth table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1+2 IS TRUE, 0*3 IS FALSE, 5 DIV 2 IS TRUE, "
                   "5 % 2 IS TRUE, IFNULL(NULL,1) IS TRUE AS a, "
                   "NULLIF(1,1) IS UNKNOWN AS b, ISNULL(NULL) IS TRUE AS c, "
                   "COALESCE(NULL,0) IS FALSE AS d FROM DUAL",
            .columns = operand_columns,
            .column_count = operand_column_count,
            .values = operand_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is arithmetic and function operands",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT (1=1) IS TRUE, (1=NULL) IS UNKNOWN, "
                   "(NULL<=>NULL) IS TRUE, (1 AND 0) IS FALSE, "
                   "(1 OR NULL) IS TRUE, (NULL XOR 1) IS UNKNOWN, "
                   "(1 IS TRUE) IS TRUE, (1 IS NULL) IS FALSE, "
                   "(1 IS TRUE)=1, 1=(1 IS TRUE)",
            .columns = nested_columns,
            .column_count = nested_column_count,
            .values = nested_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is nested operands",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NOT 1 IS TRUE, NOT (1 IS TRUE), 1 IS TRUE AND 0, "
                   "1 AND 0 IS FALSE, 1 IS TRUE XOR 0, 0 OR NULL IS UNKNOWN",
            .columns = precedence_columns,
            .column_count = precedence_column_count,
            .values = precedence_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is precedence",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 9223372036854775807 IS TRUE, "
                   "-9223372036854775807 IS TRUE, "
                   "(-9223372036854775807-1) IS TRUE, 0 IS TRUE, "
                   "0 IS FALSE, NULL IS NOT TRUE",
            .columns = boundary_columns,
            .column_count = boundary_column_count,
            .values = boundary_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is signed boundaries",
        }
    );

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "scalar is leaves catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "scalar is leaves sqlite schema generation unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures +=
        expect_int(read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)), 0, path);
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "scalar is leaves preamble unchanged"
    );

    remove_related_files(path);
    return failures;
}

static int test_scalar_is_warnings_and_diagnostics(void) {
    static const char *const warning_columns[] = {
        "5 DIV 0 IS NULL",
        "5 DIV 0 IS UNKNOWN",
        "5 DIV 0 IS TRUE",
        "5 DIV 0 IS FALSE",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {"1", "1", "0", "0", "0", "0"};
    static const char *const short_circuit_columns[] = {
        "(0 AND 5 DIV 0) IS FALSE",
        "(1 OR 5 DIV 0) IS TRUE",
        "(NULL XOR 5 DIV 0) IS UNKNOWN",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const short_circuit_values[] = {"1", "1", "1", "0", "0"};
    static const char *const show_warning_columns[] = {"Level", "Code", "Message"};
    static const char *const show_warning_values[] = {
        "Warning",
        "1365",
        "Division by 0",
        "Warning",
        "1365",
        "Division by 0",
        "Warning",
        "1365",
        "Division by 0",
        "Warning",
        "1365",
        "Division by 0",
    };
    static const char *const following_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const warning_following_values[] = {"4", "-1"};
    static const char *const short_circuit_following_values[] = {"0", "-1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "warnings") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open warnings file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE rc_seed(id INT)", NULL);
    failures += execute_ok(database, "UPDATE rc_seed SET id = 1 WHERE id = 2", NULL);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 5 DIV 0 IS NULL, 5 DIV 0 IS UNKNOWN, "
                   "5 DIV 0 IS TRUE, 5 DIV 0 IS FALSE, @@warning_count, ROW_COUNT()",
            .columns = warning_columns,
            .column_count = warning_column_count,
            .values = warning_values,
            .row_count = 1U,
            .warning_count = child_warning_count,
            .affected_rows = 0,
            .context = "is child warning values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = show_warning_columns,
            .column_count = 3U,
            .values = show_warning_values,
            .row_count = child_warning_count,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is show warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .columns = following_columns,
            .column_count = 2U,
            .values = warning_following_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is following diagnostics",
        }
    );
    failures += execute_ok(database, "UPDATE rc_seed SET id = 1 WHERE id = 2", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT (0 AND 5 DIV 0) IS FALSE, (1 OR 5 DIV 0) IS TRUE, "
                   "(NULL XOR 5 DIV 0) IS UNKNOWN, @@warning_count, ROW_COUNT()",
            .columns = short_circuit_columns,
            .column_count = short_circuit_column_count,
            .values = short_circuit_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is logical short circuit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = show_warning_columns,
            .column_count = 3U,
            .values = show_warning_values,
            .row_count = 0U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is short circuit warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .columns = following_columns,
            .column_count = 2U,
            .values = short_circuit_following_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "is short circuit following diagnostics",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_scalar_is_overflow_and_unsupported_forms(void) {
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
    failures += execute_ok(database, "INSERT INTO t VALUES (1), (0), (NULL)", NULL);

    failures += execute_error(
        database,
        "SELECT 3037000500*3037000500 IS TRUE",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT 'a' IS TRUE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1.5 IS TRUE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT 0x31 IS TRUE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT b'1' IS TRUE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 IS TRUE IS TRUE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 IS TRUE = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 IS NULL = 0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 + (0 IS FALSE)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT id IS TRUE FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 IS",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 IS 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT IS TRUE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 IS (TRUE)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_scalar_is_independent_handles(void) {
    static const char *const first_columns[] = {"first_result", "NULL IS NOT TRUE"};
    static const char *const first_values[] = {"1", "1"};
    static const char *const second_columns[] = {"second_result", "1 IS FALSE"};
    static const char *const second_values[] = {"1", "0"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first memory handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second memory handle");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT IFNULL(NULL,1) IS TRUE AS first_result, NULL IS NOT TRUE",
            .columns = first_columns,
            .column_count = 2U,
            .values = first_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first handle is",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT NULLIF(1,1) IS UNKNOWN AS second_result, 1 IS FALSE",
            .columns = second_columns,
            .column_count = 2U,
            .values = second_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second handle is",
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
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);

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
        "/tmp/mylite-scalar-is-projection-%s-%d.mylite",
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
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
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
