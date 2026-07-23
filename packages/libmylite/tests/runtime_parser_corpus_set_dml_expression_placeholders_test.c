#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_set_dml_expression_placeholders(void);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);

int main(void) {
    return test_set_dml_expression_placeholders() == 0 ? 0 : 1;
}

static int test_set_dml_expression_placeholders(void) {
    mylite_db *database = NULL;
    struct expected_sql_error unsupported = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "utility statement is not supported",
    };
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open transient database"
    );
    if (failures != 0) {
        return failures;
    }

    failures += execute_error(database, "SET @a = 1 / 0", unsupported);
    failures += execute_error(database, "SET sql_mode = 32 + (65536 * 4)", unsupported);
    failures += execute_error(database, "SET @x = 1 + (SELECT COUNT(*) FROM t)", unsupported);
    failures += execute_error(database, "INSERT INTO t VALUES (1 / 0, 2 * 3)", unsupported);
    failures += execute_error(database, "INSERT IGNORE INTO t VALUES (2 / 0)", unsupported);
    failures += execute_error(database, "UPDATE t SET data = data * 2 WHERE id = 3", unsupported);
    failures += execute_error(
        database,
        "UPDATE mysql.server_cost SET cost_value = 0.5 * cost_value",
        unsupported
    );

    mylite_close(database);
    return failures;
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
