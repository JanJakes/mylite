#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum nested_runtime_shape {
    nested_runtime_parentheses = 0,
    nested_runtime_if,
};

enum {
    mysql_error_parse = 1064,
};

static int test_deep_direct_execution(void);
static int test_deep_prepare(void);
static int test_public_stack_ceiling(void);
static int expect_execute_value(
    mylite_db *database,
    enum nested_runtime_shape shape,
    size_t depth,
    const char *expected,
    const char *context
);
static int expect_prepare_value(
    mylite_db *database,
    enum nested_runtime_shape shape,
    size_t depth,
    bool buffered,
    const char *expected,
    const char *context
);
static int expect_connection_reuse(mylite_db *database, const char *context);
static char *make_nested_sql(enum nested_runtime_shape shape, size_t depth, size_t *out_length);

int main(void) {
    int failures = 0;

    failures += test_deep_direct_execution();
    failures += test_deep_prepare();
    failures += test_public_stack_ceiling();
    return failures == 0 ? 0 : 1;
}

static int test_deep_direct_execution(void) {
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open direct database");

    if (database == NULL) {
        return failures;
    }
    failures += expect_execute_value(
        database,
        nested_runtime_parentheses,
        16384U,
        "0",
        "direct 16384 parentheses"
    );
    failures +=
        expect_execute_value(database, nested_runtime_if, 1732U, "1", "direct 1732 IF calls");
    failures += expect_connection_reuse(database, "direct nesting reuse");
    mylite_close(database);
    return failures;
}

static int test_deep_prepare(void) {
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open prepare database");

    if (database == NULL) {
        return failures;
    }
    failures += expect_prepare_value(
        database,
        nested_runtime_parentheses,
        16384U,
        false,
        "0",
        "streaming prepare 16384 parentheses"
    );
    failures += expect_prepare_value(
        database,
        nested_runtime_if,
        1024U,
        false,
        "1",
        "streaming prepare 1024 IF calls"
    );
    failures += expect_prepare_value(
        database,
        nested_runtime_parentheses,
        16384U,
        true,
        "0",
        "buffered prepare 16384 parentheses"
    );
    failures += expect_prepare_value(
        database,
        nested_runtime_if,
        1024U,
        true,
        "1",
        "buffered prepare 1024 IF calls"
    );
    failures += expect_connection_reuse(database, "prepare nesting reuse");
    mylite_close(database);
    return failures;
}

static int test_public_stack_ceiling(void) {
    static const char expected_message[] =
        "You have an error in your SQL syntax; check the manual that corresponds to your "
        "MySQL server version for the right syntax to use near '' at line 1";
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    size_t sql_length = 0U;
    char *sql = make_nested_sql(nested_runtime_parentheses, 32768U, &sql_length);
    int failures = mylite_test_expect_true(sql != NULL, "allocate stack ceiling SQL");

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open ceiling database");
    if (database == NULL || sql == NULL) {
        free(sql);
        mylite_close(database);
        return failures;
    }
    failures += mylite_test_expect_int(
        mylite_execute(database, sql, sql_length, &result),
        MYLITE_ERROR,
        "public parser stack ceiling"
    );
    failures += mylite_test_expect_true(result == NULL, "stack ceiling has no result");
    failures +=
        mylite_test_expect_int(mylite_errcode(database), mysql_error_parse, "stack ceiling code");
    failures += mylite_test_expect_text(mylite_sqlstate(database), "42000", "stack ceiling state");
    failures +=
        mylite_test_expect_text(mylite_errmsg(database), expected_message, "stack ceiling message");
    failures += expect_connection_reuse(database, "stack ceiling reuse");
    mylite_result_free(result);
    free(sql);
    mylite_close(database);
    return failures;
}

static int expect_execute_value(
    mylite_db *database,
    enum nested_runtime_shape shape,
    size_t depth,
    const char *expected,
    const char *context
) {
    mylite_result *result = NULL;
    size_t sql_length = 0U;
    char *sql = make_nested_sql(shape, depth, &sql_length);
    int failures = mylite_test_expect_true(sql != NULL, context);

    if (sql == NULL) {
        return failures;
    }
    failures += mylite_test_expect_int(
        mylite_execute(database, sql, sql_length, &result),
        MYLITE_OK,
        context
    );
    failures += mylite_test_expect_true(result != NULL, context);
    if (result != NULL) {
        failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
        failures += mylite_test_expect_size(mylite_result_column_count(result), 1U, context);
        failures +=
            mylite_test_expect_text(mylite_result_value_text(result, 0U, 0U), expected, context);
    }
    mylite_result_free(result);
    free(sql);
    return failures;
}

static int expect_prepare_value(
    mylite_db *database,
    enum nested_runtime_shape shape,
    size_t depth,
    bool buffered,
    const char *expected,
    const char *context
) {
    mylite_stmt *statement = NULL;
    size_t sql_length = 0U;
    char *sql = make_nested_sql(shape, depth, &sql_length);
    int failures = mylite_test_expect_true(sql != NULL, context);
    int status = MYLITE_MISUSE;

    if (sql == NULL) {
        return failures;
    }
    status = buffered ? mylite_prepare_buffered(database, sql, sql_length, &statement)
                      : mylite_prepare(database, sql, sql_length, &statement);
    failures += mylite_test_expect_int(status, MYLITE_OK, context);
    failures += mylite_test_expect_true(statement != NULL, context);
    if (statement != NULL) {
        failures += mylite_test_expect_int(mylite_stmt_step(statement), MYLITE_ROW, context);
        failures +=
            mylite_test_expect_text(mylite_stmt_value_text(statement, 0U), expected, context);
        failures += mylite_test_expect_int(mylite_stmt_step(statement), MYLITE_DONE, context);
        failures += mylite_test_expect_int(mylite_stmt_finalize(statement), MYLITE_OK, context);
    }
    free(sql);
    return failures;
}

static int expect_connection_reuse(mylite_db *database, const char *context) {
    static const char sql[] = "SELECT 9";
    mylite_result *result = NULL;
    int failures = mylite_test_expect_int(
        mylite_execute(database, sql, sizeof(sql) - 1U, &result),
        MYLITE_OK,
        context
    );

    failures += mylite_test_expect_true(result != NULL, context);
    if (result != NULL) {
        failures += mylite_test_expect_text(mylite_result_value_text(result, 0U, 0U), "9", context);
    }
    mylite_result_free(result);
    return failures;
}

static char *make_nested_sql(enum nested_runtime_shape shape, size_t depth, size_t *out_length) {
    static const char prefix[] = "SELECT ";
    const char *open = shape == nested_runtime_parentheses ? "(" : "IF(1,1,";
    const size_t prefix_length = sizeof(prefix) - 1U;
    const size_t open_length = strlen(open);
    size_t length = 0U;
    char *sql = NULL;
    char *cursor = NULL;

    if (out_length == NULL || depth > (SIZE_MAX - prefix_length - 1U) / (open_length + 1U)) {
        return NULL;
    }
    length = prefix_length + depth * open_length + 1U + depth;
    sql = (char *)malloc(length + 1U);
    if (sql == NULL) {
        return NULL;
    }
    cursor = sql;
    memcpy(cursor, prefix, prefix_length);
    cursor += prefix_length;
    for (size_t index = 0U; index < depth; ++index) {
        memcpy(cursor, open, open_length);
        cursor += open_length;
    }
    *cursor++ = '0';
    memset(cursor, ')', depth);
    cursor += depth;
    *cursor = '\0';
    *out_length = length;
    return sql;
}
