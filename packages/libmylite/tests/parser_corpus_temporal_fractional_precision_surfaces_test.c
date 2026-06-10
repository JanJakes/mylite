#include "parser_test_support.h"

#include <stdio.h>
#include <string.h>

static int test_temporal_column_precision_surfaces(void);
static int test_temporal_function_precision_surfaces(void);
static int test_temporal_cast_precision_surfaces(void);
static int test_date_only_function_argument_placeholders(void);
static int parse_ok(const char *sql);
static int expect_temporal_precision(
    const struct mylite_sql_ast_node *node,
    const char *expected,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_temporal_column_precision_surfaces();
    failures += test_temporal_function_precision_surfaces();
    failures += test_temporal_cast_precision_surfaces();
    failures += test_date_only_function_argument_placeholders();

    return failures == 0 ? 0 : 1;
}

static int test_temporal_column_precision_surfaces(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *type = NULL;
    int failures = parser_test_parse_sql(
        "CREATE TABLE temporal_fsp (tm TIME(6), dt DATETIME(3), "
        "ts TIMESTAMP(1) NULL DEFAULT NULL)",
        MYLITE_SQL_PARSE_OK,
        &result
    );

    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(columns, 0U);
    type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_node(type, MYLITE_SQL_AST_TIME_TYPE, "TIME(fsp) type");
    failures += parser_test_expect_span_text(type, "TIME(6)", "TIME(fsp) span");
    failures += expect_temporal_precision(type, "6", "TIME(fsp) precision");

    column = parser_test_child_at(columns, 1U);
    type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_node(type, MYLITE_SQL_AST_DATETIME_TYPE, "DATETIME(fsp) type");
    failures += parser_test_expect_span_text(type, "DATETIME(3)", "DATETIME(fsp) span");
    failures += expect_temporal_precision(type, "3", "DATETIME(fsp) precision");

    column = parser_test_child_at(columns, 2U);
    type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_node(type, MYLITE_SQL_AST_TIMESTAMP_TYPE, "TIMESTAMP(fsp) type");
    failures += parser_test_expect_span_text(type, "TIMESTAMP(1)", "TIMESTAMP(fsp) span");
    failures += expect_temporal_precision(type, "1", "TIMESTAMP(fsp) precision");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_ok("CREATE TABLE zeroes (tm TIME(0), dt DATETIME(0), ts TIMESTAMP(0))");
    failures += parse_ok("ALTER TABLE temporal_fsp ADD COLUMN added TIME(0)");
    failures += parse_ok("ALTER TABLE temporal_fsp MODIFY dt DATETIME(6)");
    failures += parse_ok("ALTER TABLE temporal_fsp CHANGE ts changed TIMESTAMP(6) NULL");

    return failures;
}

static int test_temporal_function_precision_surfaces(void) {
    enum {
        curtime_item_index = 0U,
        current_time_item_index = 1U,
        utc_time_item_index = 2U,
        utc_timestamp_item_index = 3U,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *function = NULL;
    int failures = parser_test_parse_sql(
        "SELECT CURTIME(6), CURRENT_TIME(0), UTC_TIME(4), UTC_TIMESTAMP(2)",
        MYLITE_SQL_PARSE_OK,
        &result
    );

    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    function = parser_test_child_at(parser_test_child_at(select_list, curtime_item_index), 0U);
    failures +=
        parser_test_expect_node(function, MYLITE_SQL_AST_CURRENT_TIME_VALUE, "CURTIME(fsp)");
    failures += expect_temporal_precision(function, "6", "CURTIME(fsp) precision");

    function = parser_test_child_at(parser_test_child_at(select_list, current_time_item_index), 0U);
    failures +=
        parser_test_expect_node(function, MYLITE_SQL_AST_CURRENT_TIME_VALUE, "CURRENT_TIME(fsp)");
    failures += expect_temporal_precision(function, "0", "CURRENT_TIME(fsp) precision");

    function = parser_test_child_at(parser_test_child_at(select_list, utc_time_item_index), 0U);
    failures += parser_test_expect_node(function, MYLITE_SQL_AST_UTC_TIME_VALUE, "UTC_TIME(fsp)");
    failures += expect_temporal_precision(function, "4", "UTC_TIME(fsp) precision");

    function =
        parser_test_child_at(parser_test_child_at(select_list, utc_timestamp_item_index), 0U);
    failures +=
        parser_test_expect_node(function, MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE, "UTC_TIMESTAMP(fsp)");
    failures += expect_temporal_precision(function, "2", "UTC_TIMESTAMP(fsp) precision");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_ok("DO CURTIME(0), CURRENT_TIME(0), UTC_TIME(0), UTC_TIMESTAMP(0)");
    failures += parser_test_parse_sql("SELECT CURTIME (6)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql_with_ignore_space("SELECT CURTIME (6)", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_temporal_cast_precision_surfaces(void) {
    int failures = 0;

    failures += parse_ok("SELECT CAST('2024-01-02 03:04:05.123456' AS DATETIME(6))");
    failures += parse_ok("SELECT CAST('01:02:03.123456' AS TIME(3))");
    failures += parse_ok("SELECT CONVERT('2024-01-02 03:04:05.123456', TIMESTAMP(0))");

    return failures;
}

static int test_date_only_function_argument_placeholders(void) {
    struct mylite_sql_parse_result result;
    int failures = 0;

    failures += parser_test_parse_sql("SELECT CURDATE(1)", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT CURRENT_DATE(1)", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT UTC_DATE(1)", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int parse_ok(const char *sql) {
    struct mylite_sql_parse_result result;
    int failures = parser_test_parse_sql(sql, MYLITE_SQL_PARSE_OK, &result);

    if (failures != 0) {
        (void)parser_test_expect_true(0, sql);
    }
    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int expect_temporal_precision(
    const struct mylite_sql_ast_node *node,
    const char *expected,
    const char *context
) {
    struct mylite_sql_source_span span =
        mylite_sql_ast_node_temporal_fractional_precision_span(node);
    size_t expected_length = strlen(expected);
    int failures = 0;

    failures += parser_test_expect_true(
        mylite_sql_ast_node_has_temporal_fractional_precision(node),
        context
    );
    if (span.length != expected_length ||
        (expected_length > 0U && memcmp(span.text, expected, expected_length) != 0)) {
        fprintf(
            stderr,
            "%s: expected precision span '%s', got '%.*s'\n",
            context,
            expected,
            (int)span.length,
            span.text == NULL ? "" : span.text
        );
        failures += 1;
    }

    return failures;
}
