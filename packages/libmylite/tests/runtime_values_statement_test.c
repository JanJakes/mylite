#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
    mysql_error_column_count_mismatch = 1136,
    mysql_error_data_out_of_range = 1264,
    mysql_error_empty_values_row = 3942,
    mysql_error_values_default = 3943,
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

static int test_values_statement_success_and_session_state(void);
static int test_values_statement_limits_and_order_validation(void);
static int test_values_statement_diagnostics(void);
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
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_values_statement_success_and_session_state();
    failures += test_values_statement_limits_and_order_validation();
    failures += test_values_statement_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_values_statement_success_and_session_state(void) {
    static const char *const columns[] = {
        "column_0",
        "column_1",
        "column_2",
        "column_3",
        "column_4",
        "column_5",
    };
    static const char *const values[] = {"1", "-2", NULL, "a", "1", "0"};
    static const char *const row_count_columns[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const row_count_values[] = {"-1", "0"};
    const size_t column_count = sizeof(columns) / sizeof(columns[0]);
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open memory database");
    failures += execute_ok(database, "VALUES ROW(1, -2, NULL, 'a', TRUE, FALSE)", &result);
    failures +=
        expect_size(mylite_result_column_count(result), column_count, "values column count");
    failures += expect_size(mylite_result_row_count(result), 1U, "values row count");
    failures += expect_int64(mylite_result_affected_rows(result), 0, "values affected rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "values warnings");
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        failures += expect_text(
            mylite_result_column_name(result, column_index),
            columns[column_index],
            "values column label"
        );
        failures += expect_result_value(result, 0U, column_index, values[column_index], "value");
    }
    mylite_result_free(result);
    result = NULL;

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = row_count_columns,
            .column_count = 2U,
            .values = row_count_values,
            .row_count = 1U,
            .context = "values row_count and warning count",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_values_statement_limits_and_order_validation(void) {
    static const char *const one_column[] = {"column_0"};
    static const char *const two_columns[] = {"column_0", "column_1"};
    static const char *const limit_zero_values[] = {NULL};
    static const char *const limit_two_values[] = {"1", "2"};
    static const char *const comma_offset_values[] = {"2", "3"};
    static const char *const keyword_offset_values[] = {"2", "3"};
    static const char *const order_name_values[] = {"1", "9", "2", "8"};
    static const char *const order_ordinal_values[] = {"1", "9"};
    static const char *const order_constant_values[] = {"1", "2"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open limit database");

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "VALUES ROW(1), ROW(2), ROW(3) LIMIT 0",
            .columns = one_column,
            .column_count = 1U,
            .values = limit_zero_values,
            .row_count = 0U,
            .context = "values limit zero",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "VALUES ROW(1), ROW(2), ROW(3) LIMIT 2",
            .columns = one_column,
            .column_count = 1U,
            .values = limit_two_values,
            .row_count = 2U,
            .context = "values limit row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "VALUES ROW(1), ROW(2), ROW(3) LIMIT 1, 2",
            .columns = one_column,
            .column_count = 1U,
            .values = comma_offset_values,
            .row_count = 2U,
            .context = "values comma offset limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "VALUES ROW(1), ROW(2), ROW(3) LIMIT 2 OFFSET 1",
            .columns = one_column,
            .column_count = 1U,
            .values = keyword_offset_values,
            .row_count = 2U,
            .context = "values keyword offset limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "VALUES ROW(1, 9), ROW(2, 8), ROW(3, 7) ORDER BY column_1 DESC LIMIT 2",
            .columns = two_columns,
            .column_count = 2U,
            .values = order_name_values,
            .row_count = 2U,
            .context = "values order validates without sorting",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "VALUES ROW(1, 9), ROW(2, 8) ORDER BY `column_1`, 1 DESC LIMIT 1",
            .columns = two_columns,
            .column_count = 2U,
            .values = order_ordinal_values,
            .row_count = 1U,
            .context = "values quoted name and ordinal order keys",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "VALUES ROW(1), ROW(2) ORDER BY NULL DESC",
            .columns = one_column,
            .column_count = 1U,
            .values = order_constant_values,
            .row_count = 2U,
            .context = "values NULL order key preserves constructor order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "VALUES ROW(1), ROW(2) ORDER BY '1' DESC",
            .columns = one_column,
            .column_count = 1U,
            .values = order_constant_values,
            .row_count = 2U,
            .context = "values string order key preserves constructor order",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_values_statement_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open diagnostic database");
    failures += execute_error(
        database,
        "VALUES ROW()",
        (struct expected_sql_error){
            .code = mysql_error_empty_values_row,
            .sqlstate = "HY000",
            .message_part = "Each row of a VALUES clause must have at least one column",
        }
    );
    failures += execute_error(
        database,
        "VALUES ROW(1), ROW(2, 3)",
        (struct expected_sql_error){
            .code = mysql_error_column_count_mismatch,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count at row 2",
        }
    );
    failures += execute_error(
        database,
        "VALUES ROW(DEFAULT)",
        (struct expected_sql_error){
            .code = mysql_error_values_default,
            .sqlstate = "HY000",
            .message_part = "A VALUES clause cannot use DEFAULT values",
        }
    );
    failures += execute_error(
        database,
        "VALUES ROW(1) ORDER BY column_1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'column_1' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "VALUES ROW(1) ORDER BY 0",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column '0' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "VALUES ROW(1) ORDER BY t.column_0",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 't.column_0' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "VALUES ROW(9223372036854775808)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "outside the supported signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "VALUES ROW(1) WHERE TRUE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "VALUES ROW(1) LIMIT +1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "VALUES ROW(1) LIMIT 9223372036854775808",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "LIMIT literal is outside the supported range",
        }
    );

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s failed with %d %s %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
    }
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "sqlstate");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    failures += expect_size(mylite_result_column_count(result), 0U, "failed result column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "failed result row count");
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, "query affected rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "query warnings");
    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text(
            mylite_result_column_name(result, column_index),
            expected.columns[column_index],
            expected.context
        );
    }
    for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
            size_t value_index = (row_index * expected.column_count) + column_index;

            failures += expect_result_value(
                result,
                row_index,
                column_index,
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

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s: expected NULL at %zu,%zu but got %s\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }
    return expect_text(actual, expected, context);
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d but got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %lld but got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu but got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected text %s but got %s\n",
        context,
        expected == NULL ? "(null)" : expected,
        actual == NULL ? "(null)" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %s to contain %s\n",
        context,
        actual == NULL ? "(null)" : actual,
        needle == NULL ? "(null)" : needle
    );
    return 1;
}
