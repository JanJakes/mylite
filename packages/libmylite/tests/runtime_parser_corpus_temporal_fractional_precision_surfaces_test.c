#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    show_columns_column_count = 6,
    information_schema_column_count = 4,
    mysql_error_parse = 1064,
    mysql_error_precision_too_big = 1426,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_zero_precision_metadata_scalars_and_dml(void);
static int test_nonzero_precision_diagnostics_and_no_mutation(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_zero_precision_metadata_scalars_and_dml();
    failures += test_nonzero_precision_diagnostics_and_no_mutation();

    return failures == 0 ? 0 : 1;
}

static int test_zero_precision_metadata_scalars_and_dml(void) {
    static const char *const show_columns_rows[] = {
        "tm",
        "time",
        "YES",
        "",
        NULL,
        "",
        "dt",
        "datetime",
        "YES",
        "",
        NULL,
        "",
        "ts",
        "timestamp",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const information_schema_rows[] = {
        "tm",
        "time",
        "time",
        "0",
        "dt",
        "datetime",
        "datetime",
        "0",
        "ts",
        "timestamp",
        "timestamp",
        "0",
    };
    static const char *const scalar_rows[] = {
        "22:13:20",
        "22:13:20",
        "22:13:20",
        "2023-11-14 22:13:20",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open zero database"
    );
    if (failures != 0) {
        return failures;
    }

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "SET timestamp = 1700000000");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE zero_precision ("
        "tm TIME(0), dt DATETIME(0), ts TIMESTAMP(0) NULL DEFAULT NULL)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM zero_precision",
            .values = show_columns_rows,
            .column_count = show_columns_column_count,
            .row_count = 3U,
            .context = "zero precision SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, DATETIME_PRECISION "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'zero_precision' ORDER BY ORDINAL_POSITION",
            .values = information_schema_rows,
            .column_count = information_schema_column_count,
            .row_count = 3U,
            .context = "zero precision INFORMATION_SCHEMA.COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CURTIME(0), CURRENT_TIME(0), UTC_TIME(0), UTC_TIMESTAMP(0)",
            .values = scalar_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "zero precision scalar functions",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE default_zero (id INT, tm TIME DEFAULT (CURRENT_TIME(0)))"
    );
    failures += expect_statement_ok(database, "CREATE TABLE dml_zero (tm TIME)");
    failures += expect_statement_ok(database, "INSERT INTO dml_zero VALUES (CURRENT_TIME(0))");
    failures += expect_statement_ok(database, "UPDATE dml_zero SET tm = UTC_TIME(0)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT tm FROM dml_zero",
            .values = (const char *const[]){"22:13:20"},
            .column_count = 1U,
            .row_count = 1U,
            .context = "zero precision DML values",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_nonzero_precision_diagnostics_and_no_mutation(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open diagnostics db"
    );
    if (failures != 0) {
        return failures;
    }

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE base (id INT)");
    failures += execute_error(
        database,
        "CREATE TABLE fractional_col (tm TIME(6))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "fractional temporal column precision is not supported",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW TABLES LIKE 'fractional_col'",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "failed CREATE TABLE left no table",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE too_big_col (tm TIME(7))",
        (struct expected_sql_error){
            .code = mysql_error_precision_too_big,
            .sqlstate = "42000",
            .message_part = "Too-big precision 7 specified for 'tm'. Maximum is 6.",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE base ADD COLUMN tm TIME(6)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "fractional temporal column precision is not supported",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM base LIKE 'tm'",
            .values = NULL,
            .column_count = show_columns_column_count,
            .row_count = 0U,
            .context = "failed ADD COLUMN left no column",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE base MODIFY id DATETIME(6)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "fractional temporal column precision is not supported",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM base LIKE 'id'",
            .values = (const char *const[]){"id", "int", "YES", "", NULL, ""},
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "failed MODIFY left original column",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE base CHANGE id dt DATETIME(6)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "fractional temporal column precision is not supported",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM base LIKE 'dt'",
            .values = NULL,
            .column_count = show_columns_column_count,
            .row_count = 0U,
            .context = "failed CHANGE left no renamed column",
        }
    );
    failures += execute_error(
        database,
        "SELECT CURTIME(6)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "fractional CURRENT_TIME precision is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT CURRENT_TIME(7)",
        (struct expected_sql_error){
            .code = mysql_error_precision_too_big,
            .sqlstate = "42000",
            .message_part = "Too-big precision 7 specified for 'curtime'. Maximum is 6.",
        }
    );
    failures += execute_error(
        database,
        "SELECT UTC_TIME(6)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "fractional UTC_TIME precision is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT UTC_TIME(7)",
        (struct expected_sql_error){
            .code = mysql_error_precision_too_big,
            .sqlstate = "42000",
            .message_part = "Too-big precision 7 specified for 'utc_time'. Maximum is 6.",
        }
    );
    failures += execute_error(
        database,
        "SELECT UTC_TIMESTAMP(6)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "fractional UTC_TIMESTAMP precision is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT UTC_TIMESTAMP(7)",
        (struct expected_sql_error){
            .code = mysql_error_precision_too_big,
            .sqlstate = "42000",
            .message_part = "Too-big precision 7 specified for 'utc_timestamp'. Maximum is 6.",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE dml_values (tm TIME)");
    failures += expect_statement_ok(database, "INSERT INTO dml_values VALUES ('01:02:03')");
    failures += execute_error(
        database,
        "INSERT INTO dml_values VALUES (CURRENT_TIME(6))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "fractional CURRENT_TIME precision is not supported",
        }
    );
    failures += execute_error(
        database,
        "UPDATE dml_values SET tm = UTC_TIME(6)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "fractional UTC_TIME precision is not supported",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT tm FROM dml_values",
            .values = (const char *const[]){"01:02:03"},
            .column_count = 1U,
            .row_count = 1U,
            .context = "failed DML precision left row unchanged",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE default_bad (tm TIME DEFAULT (CURRENT_TIME(6)))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "fractional CURRENT_TIME precision is not supported",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE default_too_big (tm TIME DEFAULT (CURRENT_TIME(7)))",
        (struct expected_sql_error){
            .code = mysql_error_precision_too_big,
            .sqlstate = "42000",
            .message_part = "Too-big precision 7 specified for 'curtime'. Maximum is 6.",
        }
    );

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
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

    failures += mylite_test_expect_int(rc, MYLITE_ERROR, sql);
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures +=
        mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    if (result != NULL) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            0U,
            "failed result columns"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "failed result rows");
    }
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    return execute_ok(database, sql, NULL);
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    if (failures == 0) {
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                failures += expect_result_value(
                    result,
                    row,
                    column,
                    query.values[(row * query.column_count) + column],
                    query.context
                );
            }
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

    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s row %zu column %zu: expected [%s], got [%s]\n",
            context,
            row,
            column,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
}
