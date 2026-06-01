#include <mylite/mylite.h>

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
    mysql_error_native_function_argument_count = 1582,
    mysql_error_incorrect_arguments = 1210,
    mysql_error_parse = 1064,
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
    const char *context;
};

static int test_no_source_dual_and_do_scalar_functions(void);
static int test_table_backed_scalar_functions_and_reopen(void);
static int test_scalar_function_warnings_and_diagnostics(void);
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

    failures += test_no_source_dual_and_do_scalar_functions();
    failures += test_table_backed_scalar_functions_and_reopen();
    failures += test_scalar_function_warnings_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_scalar_functions(void) {
    static const char *const columns_scalar[] = {
        "p1",
        "p2",
        "p3",
        "pd1",
        "pd2",
        "shifted",
        "null_shift",
        "w_null",
        "w_bin",
        "w_pad",
    };
    static const char *const values_scalar[] = {
        "200803",
        "200002",
        "196912",
        "11",
        "-1",
        "2004-01-01 14:30:00",
        NULL,
        NULL,
        "4142",
        "61620000",
    };
    static const char *const columns_dual[] = {"p"};
    static const char *const values_dual[] = {"200804"};
    static const char *const columns_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT PERIOD_ADD(200801,2) AS p1, PERIOD_ADD(9912,2) AS p2, "
                   "PERIOD_ADD(7001,-1) AS p3, PERIOD_DIFF(200802,200703) AS pd1, "
                   "PERIOD_DIFF(9912,0001) AS pd2, "
                   "CONVERT_TZ('2004-01-01 12:00:00','+00:00','+02:30') AS shifted, "
                   "CONVERT_TZ(NULL,'+00:00','+01:00') AS null_shift, "
                   "HEX(WEIGHT_STRING(NULL)) AS w_null, "
                   "HEX(WEIGHT_STRING(CAST('AB' AS BINARY))) AS w_bin, "
                   "HEX(WEIGHT_STRING('ab' AS BINARY(4))) AS w_pad",
            .columns = columns_scalar,
            .column_count = sizeof(columns_scalar) / sizeof(columns_scalar[0]),
            .values = values_scalar,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "scalar period timezone weight values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT PERIOD_ADD (200801, 3) AS p FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "scalar functions dual",
        }
    );
    failures += execute_ok(
        database,
        "DO PERIOD_ADD(200801,2), PERIOD_DIFF(200802,200703), "
        "CONVERT_TZ('2004-01-01 12:00:00','+00:00','+02:30'), "
        "WEIGHT_STRING('ab' AS BINARY(4))",
        &result
    );
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "scalar do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "scalar do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "scalar do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "scalar do warnings");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_status,
            .column_count = sizeof(columns_status) / sizeof(columns_status[0]),
            .values = values_after_do,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "scalar do status",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_scalar_functions_and_reopen(void) {
    static const char *const columns[] = {"id", "p2", "pd", "shifted", "ws", "wb", "wbody"};
    static const char *const values[] = {
        "1",
        "200803",
        "12",
        "2004-01-01 14:30:00",
        "61620000",
        "410042",
        "FF00",
        "2",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const reopen_values[] = {"200803", "61620000"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table-backed", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT, p INT, n INT, dt VARCHAR(19), tz VARCHAR(6), s VARCHAR(20), "
        "b VARBINARY(20), body BLOB)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 200801, 2, '2004-01-01 12:00:00', '+02:30', 'ab', X'410042', X'ff00'), "
        "(2, NULL, 1, NULL, '+01:00', NULL, NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, PERIOD_ADD(p,n) AS p2, PERIOD_DIFF(p,200701) AS pd, "
                   "CONVERT_TZ(dt,'+00:00',tz) AS shifted, "
                   "HEX(WEIGHT_STRING(s AS BINARY(4))) AS ws, "
                   "HEX(WEIGHT_STRING(b)) AS wb, HEX(WEIGHT_STRING(body)) AS wbody "
                   "FROM t ORDER BY id",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "table-backed scalar functions",
        }
    );

    mylite_close(database);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen scalar table");
    if (failures == 0) {
        failures += execute_ok(database, "USE app", NULL);
    }
    if (failures == 0) {
        failures += expect_query(
            database,
            (struct expected_query){
                .sql = "SELECT PERIOD_ADD(p,n), HEX(WEIGHT_STRING(s AS BINARY(4))) "
                       "FROM t WHERE id = 1",
                .columns =
                    (const char *const[]){
                        "PERIOD_ADD(p,n)",
                        "HEX(WEIGHT_STRING(s AS BINARY(4)))",
                    },
                .column_count = 2U,
                .values = reopen_values,
                .row_count = 1U,
                .warning_count = 0U,
                .context = "reopen scalar functions",
            }
        );
    }

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_scalar_function_warnings_and_diagnostics(void) {
    static const char *const one_column[] = {"v"};
    static const char *const null_value[] = {NULL};
    static const char *const weight_value[] = {"616263"};
    static const char *const warning_columns[] = {"Level", "Code", "Message"};
    static const char *const convert_warning[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: 'bad'",
    };
    static const char *const weight_warning[] = {
        "Warning",
        "1292",
        "Truncated incorrect BINARY(3) value: 'abcdef'",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONVERT_TZ('bad','+00:00','+01:00') AS v",
            .columns = one_column,
            .column_count = 1U,
            .values = null_value,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "convert_tz invalid datetime warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = warning_columns,
            .column_count = sizeof(warning_columns) / sizeof(warning_columns[0]),
            .values = convert_warning,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "convert_tz warning detail",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT HEX(WEIGHT_STRING('abcdef' AS BINARY(3))) AS v",
            .columns = one_column,
            .column_count = 1U,
            .values = weight_value,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "weight string truncation warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = warning_columns,
            .column_count = sizeof(warning_columns) / sizeof(warning_columns[0]),
            .values = weight_warning,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "weight string warning detail",
        }
    );
    failures += execute_error(
        database,
        "SELECT PERIOD_ADD(0,1)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_arguments,
            .sqlstate = "HY000",
            .message_part = "Incorrect arguments to period_add",
        }
    );
    failures += execute_error(
        database,
        "SELECT PERIOD_ADD(1)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'PERIOD_ADD'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONVERT_TZ('2004-01-01 12:00:00','+00:00')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'CONVERT_TZ'",
        }
    );
    failures += execute_error(
        database,
        "SELECT WEIGHT_STRING()",
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
        "/tmp/mylite-scalar-period-timezone-weight-functions-%s-%d.mylite",
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
