#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_no_database = 1046,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_expression_residual_runtime(void);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_expression_residual_runtime() == 0 ? 0 : 1;
}

static int test_expression_residual_runtime(void) {
    mylite_db *database = NULL;
    struct expected_sql_error unsupported = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "utility statement is not supported",
    };
    struct expected_sql_error unsupported_do = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "DO supports only",
    };
    struct expected_sql_error no_database = {
        .code = mysql_error_no_database,
        .sqlstate = "3D000",
        .message_part = "No database selected",
    };
    int failures = 0;

    failures += expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open database");
    if (failures != 0) {
        return failures;
    }

    failures += execute_error(database, "SELECT * FROM t0 WHERE 0.9 > t0.c0", unsupported);
    failures += execute_error(database, "SELECT a, b, c FROM t1 WHERE (a > b) <> c", unsupported);
    failures += execute_error(
        database,
        "DO COUNT(DISTINCT ROUND(CAST(SLEEP(0) AS DECIMAL), NULL))",
        unsupported
    );
    failures += execute_error(database, "DO (!(SECOND(0xb16beeb7)))", unsupported_do);
    failures += execute_error(
        database,
        "SELECT \"1900-01-01 00:00:00\" + INTERVAL 1<<20 HOUR",
        unsupported
    );
    failures += execute_error(database, "SELECT * FROM t WHERE a = CURRENT_TIME", unsupported);
    failures += execute_error(database, "SELECT * FROM t WHERE a = CURRENT_DATE", unsupported);
    failures +=
        execute_error(database, "SELECT _latin1'B' BETWEEN _latin1'a' AND _latin1'c'", unsupported);
    failures += execute_error(
        database,
        "SELECT * FROM t1 ORDER BY ADDTIME(a, '00:00:00') DESC",
        unsupported
    );
    failures +=
        execute_error(database, "SELECT 1 FROM t1 GROUP BY INSERT(a,'1','11','1')", unsupported);
    failures +=
        execute_error(database, "SELECT 1 FROM r GROUP BY MAKE_SET(1,c) WITH ROLLUP", unsupported);
    failures += execute_error(database, "SELECT * FROM t1 WHERE NOT(NOT(a))", unsupported);
    failures += execute_error(
        database,
        "SELECT f1, f2, f3 FROM t1 WHERE f1 BETWEEN f2 AND f3",
        no_database
    );
    failures +=
        execute_error(database, "SELECT * FROM t2,t3 WHERE f2 IN (f3,'2003-04-05')", no_database);
    failures += execute_error(
        database,
        "SELECT argument FROM log_rows WHERE argument LIKE ('SET%')",
        unsupported
    );
    failures += execute_error(database, "SELECT a - INTERVAL(b) MICROSECOND FROM t", unsupported);

    mylite_close(database);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, "failed result columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "failed result rows");
    }
    mylite_result_free(result);
    return failures;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "NULL" : actual,
        needle == NULL ? "NULL" : needle
    );
    return 1;
}
