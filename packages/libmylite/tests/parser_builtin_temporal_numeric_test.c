#include "parser_test_support.h"

enum {
    expanded_cast_target_select_item_count = 11,
    convert_decimal_select_item_index = 10,
};

static int test_cast_binary_expression(void);
static int test_date_add_second_function(void);
static int test_addtime_subtime_functions(void);
static int test_date_format_function(void);
static int test_get_format_function(void);
static int test_time_format_function(void);
static int test_str_to_date_function(void);
static int test_datediff_function(void);
static int test_timediff_function(void);
static int test_timestampadd_second_function(void);
static int test_timestampdiff_function(void);
static int test_timestamp_function(void);
static int test_unix_timestamp_function(void);
static int test_from_unixtime_function(void);
static int test_time_second_conversion_functions(void);
static int test_temporal_constructor_functions(void);
static int test_temporal_extract_functions(void);
static int test_abs_function(void);
static int test_sign_function(void);
static int test_rounding_functions(void);
static int test_base_conversion_functions(void);
static int test_bit_count_function(void);
static int test_numeric_format_truncate_crc32_functions(void);
static int test_hex_function(void);
static int test_base64_functions(void);
static int test_compression_random_functions(void);
static int test_digest_functions(void);
static int test_unhex_function(void);
static int test_pi_function(void);
static int test_rand_function(void);
static int test_any_value_function(void);
static int test_validate_password_strength_function(void);
static int test_sqrt_function(void);
static int test_angle_conversion_functions(void);
static int test_inverse_trig_functions(void);
static int test_direct_trig_functions(void);
static int test_atan_functions(void);
static int test_exp_log_power_functions(void);

int main(void) {
    int failures = 0;

    failures += test_cast_binary_expression();
    failures += test_date_add_second_function();
    failures += test_addtime_subtime_functions();
    failures += test_date_format_function();
    failures += test_get_format_function();
    failures += test_time_format_function();
    failures += test_str_to_date_function();
    failures += test_datediff_function();
    failures += test_timediff_function();
    failures += test_timestampadd_second_function();
    failures += test_timestampdiff_function();
    failures += test_timestamp_function();
    failures += test_unix_timestamp_function();
    failures += test_from_unixtime_function();
    failures += test_time_second_conversion_functions();
    failures += test_temporal_constructor_functions();
    failures += test_temporal_extract_functions();
    failures += test_abs_function();
    failures += test_sign_function();
    failures += test_rounding_functions();
    failures += test_base_conversion_functions();
    failures += test_bit_count_function();
    failures += test_numeric_format_truncate_crc32_functions();
    failures += test_hex_function();
    failures += test_base64_functions();
    failures += test_compression_random_functions();
    failures += test_digest_functions();
    failures += test_unhex_function();
    failures += test_pi_function();
    failures += test_rand_function();
    failures += test_any_value_function();
    failures += test_validate_password_strength_function();
    failures += test_sqrt_function();
    failures += test_angle_conversion_functions();
    failures += test_inverse_trig_functions();
    failures += test_direct_trig_functions();
    failures += test_atan_functions();
    failures += test_exp_log_power_functions();

    return failures == 0 ? 0 : 1;
}

static int test_cast_binary_expression(void) {
    enum {
        cast_basic_char_item = 0,
        cast_basic_signed_item = 1,
        cast_basic_signed_integer_item = 2,
        cast_basic_signed_int_item = 3,
        cast_basic_unsigned_item = 4,
        cast_basic_unsigned_integer_item = 5,
        cast_basic_unsigned_int_item = 6,
        cast_basic_date_item = 7,
        convert_basic_char_item = 0,
        convert_basic_signed_item = 1,
        convert_basic_signed_integer_item = 2,
        convert_basic_signed_int_item = 3,
        convert_basic_unsigned_item = 4,
        convert_basic_unsigned_integer_item = 5,
        convert_basic_unsigned_int_item = 6,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_item = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT CAST('ABC' AS BINARY) AS binary, CAST(+12 AS BINARY) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_item = parser_test_child_at(select_list, 0U);
    first_expression = parser_test_child_at(first_item, 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CAST_BINARY_EXPRESSION,
        "cast binary expression"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "CAST('ABC' AS BINARY)", "cast binary span");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "cast binary string input"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_item, 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "binary alias"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(first_item, 1U),
        "binary",
        "binary alias span"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_CAST_BINARY_EXPRESSION,
        "cast binary signed expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_UNARY_EXPRESSION,
        "cast binary signed input"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "cast from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT CAST('ABC' AS BINARY(5));", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_CAST_BINARY_EXPRESSION,
        "cast binary length compatibility expression"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT CAST('ABC' AS CHAR), CAST('1' AS SIGNED), "
        "CAST('1' AS SIGNED INTEGER), CAST('1' AS SIGNED INT), "
        "CAST('1' AS UNSIGNED), CAST('1' AS UNSIGNED INTEGER), "
        "CAST('1' AS UNSIGNED INT), CAST('2024-01-01' AS DATE);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, cast_basic_char_item), 0U),
        MYLITE_SQL_AST_CAST_CHAR_EXPRESSION,
        "cast char expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, cast_basic_signed_item), 0U),
        MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION,
        "cast signed expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, cast_basic_signed_integer_item), 0U),
        MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION,
        "cast signed integer expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, cast_basic_signed_int_item), 0U),
        MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION,
        "cast signed int expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, cast_basic_unsigned_item), 0U),
        MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION,
        "cast unsigned expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, cast_basic_unsigned_integer_item),
            0U
        ),
        MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION,
        "cast unsigned integer expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, cast_basic_unsigned_int_item), 0U),
        MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION,
        "cast unsigned int expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, cast_basic_date_item), 0U),
        MYLITE_SQL_AST_CAST_CHAR_EXPRESSION,
        "cast date compatibility expression"
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT CAST('ABC' AS CHAR(5));", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_CAST_CHAR_EXPRESSION,
        "cast char length compatibility expression"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT CAST('abc' AS CHAR(2) CHARACTER SET utf8 BINARY), "
        "CAST('abc' AS NCHAR), CAST('abc' AS NATIONAL CHAR (2)), "
        "CAST('2025-10-05 14:05:28' AS TIME), "
        "CAST('2025-10-05 14:05:28' AS DATETIME), "
        "CAST('123.456' AS DECIMAL(10,1)), CAST('123.456' AS FLOAT), "
        "CAST('123.456' AS REAL), CAST('123.456' AS DOUBLE), "
        "CAST('{\"name\":\"value\"}' AS JSON), CONVERT('123.456', DECIMAL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    failures += parser_test_expect_child_count(
        select_list,
        expanded_cast_target_select_item_count,
        "expanded cast target select item count"
    );
    for (size_t item = 0U; item < expanded_cast_target_select_item_count; ++item) {
        failures += parser_test_expect_node(
            parser_test_child_at(parser_test_child_at(select_list, item), 0U),
            item == convert_decimal_select_item_index ? MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION
                                                      : MYLITE_SQL_AST_CAST_CHAR_EXPRESSION,
            "expanded cast target compatibility expression"
        );
    }
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT CAST('1' AS INT);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT CONVERT('ABC' USING BINARY) AS binary, (CONVERT('x' USING BINARY)), "
        "CONVERT(123 USING BINARY) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_item = parser_test_child_at(select_list, 0U);
    first_expression = parser_test_child_at(first_item, 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION,
        "convert using binary expression"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "CONVERT('ABC' USING BINARY)",
        "convert using binary span"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "convert using binary string input"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_item, 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "convert alias"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION,
        "parenthesized convert using binary expression"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION,
        "convert using binary runtime-deferred operand"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "convert from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT CONVERT('ABC', BINARY) AS binary, (CONVERT('x', BINARY)), "
        "CONVERT(+123 USING utf8mb4) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_item = parser_test_child_at(select_list, 0U);
    first_expression = parser_test_child_at(first_item, 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION,
        "convert binary type expression"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "CONVERT('ABC', BINARY)",
        "convert binary type span"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "convert binary type string input"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_item, 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "binary alias"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION,
        "parenthesized convert binary type expression"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION,
        "convert using charset expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_UNARY_EXPRESSION,
        "convert using charset signed input"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(third_expression, 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "convert using charset identifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(third_expression, 1U),
        "utf8mb4",
        "convert using charset identifier span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "convert syntax expansion from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO CONVERT(NULL, BINARY), CONVERT(FALSE USING utf8mb4);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_DO_STATEMENT,
        "convert syntax do statement"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION,
        "convert binary type do expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION,
        "convert using charset do expression"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT CONVERT('ABC', BINARY(5));", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION,
        "convert binary length compatibility expression"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT CONVERT('ABC', CHAR), CONVERT('1', SIGNED), "
        "CONVERT('1', SIGNED INTEGER), CONVERT('1', SIGNED INT), "
        "CONVERT('1', UNSIGNED), CONVERT('1', UNSIGNED INTEGER), "
        "CONVERT('1', UNSIGNED INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, convert_basic_char_item), 0U),
        MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION,
        "convert char type expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, convert_basic_signed_item), 0U),
        MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION,
        "convert signed type expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, convert_basic_signed_integer_item),
            0U
        ),
        MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION,
        "convert signed integer type expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, convert_basic_signed_int_item), 0U),
        MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION,
        "convert signed int type expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, convert_basic_unsigned_item), 0U),
        MYLITE_SQL_AST_CONVERT_UNSIGNED_TYPE_EXPRESSION,
        "convert unsigned type expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, convert_basic_unsigned_integer_item),
            0U
        ),
        MYLITE_SQL_AST_CONVERT_UNSIGNED_TYPE_EXPRESSION,
        "convert unsigned integer type expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, convert_basic_unsigned_int_item),
            0U
        ),
        MYLITE_SQL_AST_CONVERT_UNSIGNED_TYPE_EXPRESSION,
        "convert unsigned int type expression"
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT CONVERT('ABC', CHAR(5));", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION,
        "convert char length compatibility expression"
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT CONVERT('1', INTEGER);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT CONVERT('ABC', BINARY, 1);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE t (cast INT); SELECT cast FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE t (binary INT);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT CONVERT(val, BINARY) AS v1, CONVERT(val USING utf8mb4) AS v2 "
        "FROM t WHERE CONVERT(num, SIGNED) < 0 ORDER BY CONVERT(val USING utf8mb4);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    where_clause = parser_test_first_child_kind(select, MYLITE_SQL_AST_WHERE_CLAUSE);
    order_clause = parser_test_first_child_kind(select, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(where_clause, 0U), 0U),
        MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION,
        "convert predicate left expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(order_clause, 0U),
        MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION,
        "convert order expression"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_date_add_second_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND), "
        "Date_Add('2008-01-02', INTERVAL -1 SECOND) AS shifted FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_DATE_ADD_FUNCTION,
        "date_add function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
        "date_add span"
    );
    failures += parser_test_expect_child_count(first_expression, 3U, "date_add child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "date_add date argument"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "date_add interval argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 2U),
        MYLITE_SQL_AST_IDENTIFIER,
        "date_add interval unit"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_DATE_ADD_FUNCTION,
        "mixed-case date_add function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 1U),
        MYLITE_SQL_AST_UNARY_EXPRESSION,
        "signed date_add interval argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "date_add dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 SECOND), "
        "ADDDATE('2008-01-02', INTERVAL +1 SECOND) AS adddate_alias, "
        "SUBDATE('2008-01-02', INTERVAL -1 SECOND) AS subdate_alias FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_DATE_SUB_FUNCTION,
        "date_sub function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
        "date_sub span"
    );
    failures += parser_test_expect_child_count(first_expression, 3U, "date_sub child count");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_ADDDATE_FUNCTION,
        "adddate function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 1U),
        MYLITE_SQL_AST_UNARY_EXPRESSION,
        "signed adddate interval argument"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_SUBDATE_FUNCTION,
        "subdate function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(third_expression, 1U),
        MYLITE_SQL_AST_UNARY_EXPRESSION,
        "signed subdate interval argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "date_sub aliases dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO DATE_ADD(NULL, INTERVAL +0 SECOND), DATE_SUB(NULL, INTERVAL +0 SECOND), "
        "ADDDATE('2024-02-29', INTERVAL NULL SECOND), "
        "SUBDATE('2024-02-29', INTERVAL NULL SECOND);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "date_add do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_DATE_ADD_FUNCTION,
        "do date_add function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_DATE_SUB_FUNCTION,
        "do date_sub function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 2U),
        MYLITE_SQL_AST_ADDDATE_FUNCTION,
        "do adddate null interval"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 3U),
        MYLITE_SQL_AST_SUBDATE_FUNCTION,
        "do subdate null interval"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE date_add_keywords (date_add INT, second INT); "
        "SELECT date_add, second FROM date_add_keywords;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE date_add(id INT);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("CREATE TABLE date_add (id INT);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE date_sub(id INT);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("CREATE TABLE date_sub (id INT);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("CREATE TABLE adddate(id INT);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("CREATE TABLE subdate(id INT);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DATE_ADD ('2008-01-02', INTERVAL 1 SECOND);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT DATE_SUB ('2008-01-02', INTERVAL 1 SECOND);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT ADDDATE ('2008-01-02', INTERVAL 1 SECOND);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT SUBDATE ('2008-01-02', INTERVAL 1 SECOND);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql_with_ignore_space(
        "SELECT DATE_ADD ('2008-01-02', INTERVAL 1 SECOND);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql_with_ignore_space(
        "SELECT DATE_SUB ('2008-01-02', INTERVAL 1 SECOND), "
        "ADDDATE ('2008-01-02', INTERVAL 1 SECOND), "
        "SUBDATE ('2008-01-02', INTERVAL 1 SECOND);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql_with_ignore_space(
        "CREATE TABLE date_add (id INT);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql_with_ignore_space(
        "CREATE TABLE `date_add` (id INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql_with_ignore_space(
        "CREATE TABLE date_sub (id INT);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql_with_ignore_space(
        "CREATE TABLE `date_sub` (id INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql_with_ignore_space(
        "CREATE TABLE adddate(id INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql_with_ignore_space(
        "CREATE TABLE subdate(id INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql_with_ignore_space(
        "CREATE TABLE t (second INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DATE_ADD('2008-01-02', INTERVAL 1 MINUTE);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE interval (interval INT);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_addtime_subtime_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT ADDTIME('01:02:03', '00:00:01'), "
        "SubTime('2008-01-02 13:29:17', '-00:00:01') AS shifted FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ADDTIME_FUNCTION,
        "addtime function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "ADDTIME('01:02:03', '00:00:01')",
        "addtime span"
    );
    failures += parser_test_expect_child_count(first_expression, 2U, "addtime child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "addtime left"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "addtime right"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_SUBTIME_FUNCTION,
        "mixed-case subtime"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "addtime dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO ADDTIME(NULL, 'bad'), SUBTIME('01:02:03', NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "addtime do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_ADDTIME_FUNCTION,
        "do addtime"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_SUBTIME_FUNCTION,
        "do subtime"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT ADDTIME(), ADDTIME('01:02:03'), ADDTIME('01:02:03', '00:00:01', 'x');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ADDTIME_ARGUMENT_COUNT_ERROR,
        "addtime no arg count"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_ADDTIME_ARGUMENT_COUNT_ERROR,
        "addtime one arg count"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_ADDTIME_ARGUMENT_COUNT_ERROR,
        "addtime three arg count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT SUBTIME(), SUBTIME('01:02:03'), SUBTIME('01:02:03', '00:00:01', 'x');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_SUBTIME_ARGUMENT_COUNT_ERROR,
        "subtime no arg count"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_SUBTIME_ARGUMENT_COUNT_ERROR,
        "subtime one arg count"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_SUBTIME_ARGUMENT_COUNT_ERROR,
        "subtime three arg count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT ADDTIME ('01:02:03', '00:00:01'), SUBTIME ('01:02:03', '00:00:01');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql_with_ignore_space(
        "SELECT ADDTIME ('01:02:03', '00:00:01'), SUBTIME ('01:02:03', '00:00:01');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("CREATE TABLE addtime(id INT);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("CREATE TABLE subtime(id INT);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql_with_ignore_space(
        "CREATE TABLE addtime(id INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql_with_ignore_space(
        "CREATE TABLE subtime(id INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_date_format_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT DATE_FORMAT('2008-01-02 13:29:17', '%Y'), "
        "Date_Format(option_value, '%H.%i') = 0.42 AS matched FROM options;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_DATE_FORMAT_FUNCTION,
        "date_format function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "DATE_FORMAT('2008-01-02 13:29:17', '%Y')",
        "date_format span"
    );
    failures += parser_test_expect_child_count(first_expression, 2U, "date_format child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "date_format value argument"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "date_format format argument"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "date_format numeric comparison"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_DATE_FORMAT_FUNCTION,
        "date_format comparison left"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "date_format alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO DATE_FORMAT(NULL, '%Y'), DATE_FORMAT('2008-01-02', NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "date_format do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_DATE_FORMAT_FUNCTION,
        "do date_format null value"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_DATE_FORMAT_FUNCTION,
        "do date_format null format"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT DATE_FORMAT();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_DATE_FORMAT_ARGUMENT_COUNT_ERROR,
        "date_format zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT DATE_FORMAT('2008-01-02');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_DATE_FORMAT_ARGUMENT_COUNT_ERROR,
        "date_format one argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DATE_FORMAT('2008-01-02', '%Y', 'extra');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_DATE_FORMAT_ARGUMENT_COUNT_ERROR,
        "date_format extra argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DATE_FORMAT ('2008-01-02', '%Y') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM options WHERE DATE_FORMAT(option_value, '%H.%i') = +0.42 "
        "ORDER BY id LIMIT 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(statement, 2U), 0U),
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "date_format where predicate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_DATE_FORMAT_FUNCTION,
        "date_format where predicate lhs"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE date_format (date_format INT); SELECT date_format FROM date_format;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_get_format_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT GET_FORMAT(DATE, 'USA'), get_format(time, 'iso') AS fmt, "
        "GET_FORMAT(DATETIME, NULL), GET_FORMAT(TIMESTAMP, 123) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_GET_FORMAT_FUNCTION,
        "get_format function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "GET_FORMAT(DATE, 'USA')",
        "get_format span"
    );
    failures += parser_test_expect_child_count(first_expression, 2U, "get_format child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(first_expression, 0U),
        "DATE",
        "get_format class"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "get_format format argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_GET_FORMAT_FUNCTION,
        "lower get_format function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "get_format alias"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(third_expression, 0U),
        "DATETIME",
        "get_format datetime"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(third_expression, 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "get_format null argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DO GET_FORMAT(DATE, 'USA');", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "get_format do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_GET_FORMAT_FUNCTION,
        "do get_format"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT GET_FORMAT();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT GET_FORMAT(DATE);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT GET_FORMAT(DATE, 'USA', 'extra');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT GET_FORMAT(YEAR, 'USA');", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT GET_FORMAT('DATE', 'USA');", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE get_format (get_format INT); SELECT get_format FROM get_format;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql_with_ignore_space(
        "CREATE TABLE get_format (get_format INT); SELECT GET_FORMAT (DATE, 'USA');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_time_format_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT TIME_FORMAT('100:00:00', '%H'), "
        "Time_Format(option_value, '%H.%i') AS formatted FROM options;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_TIME_FORMAT_FUNCTION,
        "time_format function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "TIME_FORMAT('100:00:00', '%H')",
        "time_format span"
    );
    failures += parser_test_expect_child_count(first_expression, 2U, "time_format child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "time_format value argument"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "time_format format argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_TIME_FORMAT_FUNCTION,
        "time_format second function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "time_format alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO TIME_FORMAT(NULL, '%H'), TIME_FORMAT('01:02:03', NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "time_format do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_TIME_FORMAT_FUNCTION,
        "do time_format null value"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_TIME_FORMAT_FUNCTION,
        "do time_format null format"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT TIME_FORMAT();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_TIME_FORMAT_ARGUMENT_COUNT_ERROR,
        "time_format zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT TIME_FORMAT('01:02:03');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_TIME_FORMAT_ARGUMENT_COUNT_ERROR,
        "time_format one argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT TIME_FORMAT('01:02:03', '%H', 'extra');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_TIME_FORMAT_ARGUMENT_COUNT_ERROR,
        "time_format extra argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT TIME_FORMAT ('01:02:03', '%H') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE time_format (time_format INT); SELECT time_format FROM time_format;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_str_to_date_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT STR_TO_DATE('2024-01-02', '%Y-%m-%d'), "
        "Str_To_Date(option_value, '%H:%i:%s') AS parsed FROM options;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_STR_TO_DATE_FUNCTION,
        "str_to_date function"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "STR_TO_DATE('2024-01-02', '%Y-%m-%d')",
        "str_to_date span"
    );
    failures += parser_test_expect_child_count(first_expression, 2U, "str_to_date child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "str_to_date value argument"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "str_to_date format argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_STR_TO_DATE_FUNCTION,
        "str_to_date second function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "str_to_date alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO STR_TO_DATE(NULL, '%Y-%m-%d'), STR_TO_DATE('2024-01-02', NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "str_to_date do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_STR_TO_DATE_FUNCTION,
        "do str_to_date null value"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_STR_TO_DATE_FUNCTION,
        "do str_to_date null format"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT STR_TO_DATE();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_STR_TO_DATE_ARGUMENT_COUNT_ERROR,
        "str_to_date zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT STR_TO_DATE('2024-01-02');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_STR_TO_DATE_ARGUMENT_COUNT_ERROR,
        "str_to_date one argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT STR_TO_DATE('2024-01-02', '%Y', 'extra');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_STR_TO_DATE_ARGUMENT_COUNT_ERROR,
        "str_to_date extra argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT STR_TO_DATE ('2024-01-02', '%Y-%m-%d') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE str_to_date (str_to_date INT); SELECT str_to_date FROM str_to_date;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_datediff_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT DATEDIFF('2008-01-02', '2008-01-01'), "
        "DateDiff(option_value, created_at) AS days FROM options;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_DATEDIFF_FUNCTION, "datediff");
    failures += parser_test_expect_span_text(
        first_expression,
        "DATEDIFF('2008-01-02', '2008-01-01')",
        "datediff span"
    );
    failures += parser_test_expect_child_count(first_expression, 2U, "datediff child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "left"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "right"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_DATEDIFF_FUNCTION,
        "datediff column function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "datediff column left"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "datediff column right"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "datediff alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("DO DATEDIFF(NULL, '2008-01-01');", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "datediff do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_DATEDIFF_FUNCTION,
        "do datediff"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT DATEDIFF();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR,
        "datediff zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT DATEDIFF('2008-01-02');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR,
        "datediff one argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DATEDIFF('2008-01-03', '2008-01-02', '2008-01-01');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR,
        "datediff extra argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DATEDIFF ('2008-01-02', '2008-01-01') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE datediff (datediff INT); SELECT datediff FROM datediff;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_timediff_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT TIMEDIFF('01:02:03', '00:00:01'), "
        "TimeDiff(started_at, finished_at) AS elapsed FROM options;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_TIMEDIFF_FUNCTION, "timediff");
    failures += parser_test_expect_span_text(
        first_expression,
        "TIMEDIFF('01:02:03', '00:00:01')",
        "timediff span"
    );
    failures += parser_test_expect_child_count(first_expression, 2U, "timediff child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "left"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "right"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_TIMEDIFF_FUNCTION,
        "timediff column function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "timediff column left"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "timediff column right"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "timediff alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DO TIMEDIFF(NULL, 'bad');", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "timediff do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_TIMEDIFF_FUNCTION,
        "do timediff"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT TIMEDIFF(), TIMEDIFF('01:02:03'), TIMEDIFF('01:02:03','00:00:01','x');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_TIMEDIFF_ARGUMENT_COUNT_ERROR,
        "timediff no arg count"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_TIMEDIFF_ARGUMENT_COUNT_ERROR,
        "timediff one arg count"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_TIMEDIFF_ARGUMENT_COUNT_ERROR,
        "timediff three arg count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT TIMEDIFF ('01:02:03', '00:00:01') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql_with_ignore_space(
        "SELECT TIMEDIFF ('01:02:03', '00:00:01') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE timediff (timediff INT); SELECT timediff FROM timediff;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql_with_ignore_space(
        "CREATE TABLE timediff (timediff INT); SELECT timediff FROM timediff;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_timestampadd_second_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT TIMESTAMPADD(SECOND, 1, '2008-01-02'), "
        "timestampadd(SQL_TSI_SECOND, +1, created_at) AS shifted FROM options;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION,
        "timestampadd"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "TIMESTAMPADD(SECOND, 1, '2008-01-02')",
        "timestampadd span"
    );
    failures += parser_test_expect_child_count(first_expression, 3U, "timestampadd child count");
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "timestampadd unit"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(first_expression, 0U),
        "SECOND",
        "timestampadd unit"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "timestampadd interval"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "timestampadd temporal"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION,
        "timestampadd column function"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(second_expression, 0U),
        "SQL_TSI_SECOND",
        "alias unit span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 1U),
        MYLITE_SQL_AST_UNARY_EXPRESSION,
        "signed timestampadd interval"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 2U),
        MYLITE_SQL_AST_IDENTIFIER,
        "timestampadd column temporal"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "timestampadd alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO TIMESTAMPADD(SECOND, NULL, '2008-01-01');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "timestampadd do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION,
        "do timestampadd"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT TIMESTAMPADD (SECOND, 1, '2008-01-02') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION,
        "timestampadd whitespace"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "timestampadd dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE timestampadd (timestampadd INT); SELECT timestampadd FROM timestampadd;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT TIMESTAMPADD(MINUTE, 1, '2008-01-02');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT TIMESTAMPADD();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT TIMESTAMPADD(SECOND);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT TIMESTAMPADD(SECOND, 1);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT TIMESTAMPADD(SECOND, 1, '2008-01-02', 'extra');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT TIMESTAMPADD('SECOND', 1, '2008-01-02');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT TIMESTAMPADD(SQL_TSI_MICROSECOND, 1, '2008-01-02');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_timestampdiff_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT TIMESTAMPDIFF(DAY, '2008-01-01', '2008-01-02'), "
        "timestampdiff(SQL_TSI_HOUR, created_at, updated_at) AS hours FROM options;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_TIMESTAMPDIFF_FUNCTION,
        "timestampdiff"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "TIMESTAMPDIFF(DAY, '2008-01-01', '2008-01-02')",
        "timestampdiff span"
    );
    failures += parser_test_expect_child_count(first_expression, 3U, "timestampdiff child count");
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "unit"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(first_expression, 0U),
        "DAY",
        "unit span"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "left"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "right"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_TIMESTAMPDIFF_FUNCTION,
        "timestampdiff column function"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(second_expression, 0U),
        "SQL_TSI_HOUR",
        "alias unit span"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "timestampdiff column left"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 2U),
        MYLITE_SQL_AST_IDENTIFIER,
        "timestampdiff column right"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "timestampdiff alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO TIMESTAMPDIFF(MONTH, NULL, '2008-01-01');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "timestampdiff do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_TIMESTAMPDIFF_FUNCTION,
        "do timestampdiff"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT TIMESTAMPDIFF (DAY,'2008-01-01','2008-01-02') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE timestampdiff (timestampdiff INT); SELECT timestampdiff FROM timestampdiff;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT TIMESTAMPDIFF();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT TIMESTAMPDIFF(DAY);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT TIMESTAMPDIFF(DAY, '2008-01-01');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT TIMESTAMPDIFF(DAY, '2008-01-01', '2008-01-02', 'extra');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT TIMESTAMPDIFF('DAY', '2008-01-01', '2008-01-02');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "SELECT TIMESTAMPDIFF(DAY_HOUR, '2008-01-01', '2008-01-02');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_timestamp_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT TIMESTAMP('2003-12-31'), "
        "Timestamp('2003-12-31','12:00:00') AS ts, TIMESTAMP(d, tm) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_TIMESTAMP_FUNCTION,
        "timestamp date"
    );
    failures +=
        parser_test_expect_child_count(first_expression, 1U, "timestamp date argument count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "timestamp date literal"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_TIMESTAMP_FUNCTION,
        "timestamp add time"
    );
    failures +=
        parser_test_expect_child_count(second_expression, 2U, "timestamp two argument count");
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "timestamp left literal"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "timestamp right literal"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "timestamp alias"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_TIMESTAMP_FUNCTION,
        "timestamp columns"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "timestamp date column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(third_expression, 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "timestamp time column"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO TIMESTAMP('2003-12-31'), TIMESTAMP(NULL, NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "timestamp do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_TIMESTAMP_FUNCTION,
        "do timestamp literal"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_TIMESTAMP_FUNCTION,
        "do timestamp nulls"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT TIMESTAMP();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_TIMESTAMP_ARGUMENT_COUNT_ERROR,
        "timestamp empty argument count marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT TIMESTAMP('2003-12-31', '00:00:00', 'extra');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_TIMESTAMP_ARGUMENT_COUNT_ERROR,
        "timestamp extra argument count marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT TIMESTAMP ('2003-12-31') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE timestamp (timestamp INT); SELECT timestamp FROM timestamp;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_unix_timestamp_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT UNIX_TIMESTAMP(), Unix_Timestamp('1970-01-01 00:00:01') AS ts, "
        "UNIX_TIMESTAMP(d) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION,
        "unix_timestamp no-argument function"
    );
    failures +=
        parser_test_expect_child_count(first_expression, 0U, "unix_timestamp no-argument count");
    failures += parser_test_expect_span_text(
        first_expression,
        "UNIX_TIMESTAMP()",
        "unix_timestamp no-argument span"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION,
        "unix_timestamp literal function"
    );
    failures +=
        parser_test_expect_child_count(second_expression, 1U, "unix_timestamp literal count");
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "unix_timestamp literal argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "unix_timestamp alias"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION,
        "unix_timestamp column function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "unix_timestamp column argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO UNIX_TIMESTAMP(), UNIX_TIMESTAMP(NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "unix_timestamp do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION,
        "do unix_timestamp no-argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION,
        "do unix_timestamp null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT UNIX_TIMESTAMP('1970-01-01', 'extra');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_UNIX_TIMESTAMP_ARGUMENT_COUNT_ERROR,
        "unix_timestamp extra argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT UNIX_TIMESTAMP ('1970-01-01 00:00:01') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unix_timestamp (unix_timestamp INT); "
        "SELECT unix_timestamp FROM unix_timestamp;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_from_unixtime_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT FROM_UNIXTIME(0), From_Unixtime(+1) AS dt, FROM_UNIXTIME(seconds) "
        "FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION,
        "from_unixtime integer function"
    );
    failures += parser_test_expect_child_count(first_expression, 1U, "from_unixtime integer count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "from_unixtime integer argument"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION,
        "from_unixtime signed function"
    );
    failures += parser_test_expect_child_count(second_expression, 1U, "from_unixtime signed count");
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_UNARY_EXPRESSION,
        "from_unixtime signed argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "from_unixtime alias"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION,
        "from_unixtime column function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "from_unixtime column argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO FROM_UNIXTIME(1), FROM_UNIXTIME(NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "from_unixtime do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION,
        "do from_unixtime integer"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION,
        "do from_unixtime null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT FROM_UNIXTIME();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_FROM_UNIXTIME_ARGUMENT_COUNT_ERROR,
        "from_unixtime zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT FROM_UNIXTIME(1, '%Y-%m-%d');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION,
        "from_unixtime format marker"
    );
    failures += parser_test_expect_child_count(first_expression, 2U, "from_unixtime format count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT FROM_UNIXTIME(1, 2, 3);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_FROM_UNIXTIME_ARGUMENT_COUNT_ERROR,
        "from_unixtime extra argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT FROM_UNIXTIME (1) AS dt FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE from_unixtime (from_unixtime INT); "
        "SELECT from_unixtime FROM from_unixtime;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_time_second_conversion_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT TIME_TO_SEC('00:39:38'), Sec_To_Time(2378) AS tm, TIME_TO_SEC(v) "
        "FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_TIME_TO_SEC_FUNCTION,
        "time_to_sec literal function"
    );
    failures += parser_test_expect_child_count(first_expression, 1U, "time_to_sec literal count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "time_to_sec literal argument"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION,
        "sec_to_time integer function"
    );
    failures += parser_test_expect_child_count(second_expression, 1U, "sec_to_time integer count");
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "sec_to_time integer argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "sec_to_time alias"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_TIME_TO_SEC_FUNCTION,
        "time_to_sec column function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "time_to_sec column argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT TIME_TO_SEC ('00:39:38') AS secs, SEC_TO_TIME (2378) AS tm FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO TIME_TO_SEC('00:00:01'), SEC_TO_TIME(1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "time second do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_TIME_TO_SEC_FUNCTION,
        "do time_to_sec"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION,
        "do sec_to_time"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT TIME_TO_SEC();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_TIME_TO_SEC_ARGUMENT_COUNT_ERROR,
        "time_to_sec zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT TIME_TO_SEC('00:00:01', '00:00:02');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_TIME_TO_SEC_ARGUMENT_COUNT_ERROR,
        "time_to_sec extra argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT SEC_TO_TIME();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_SEC_TO_TIME_ARGUMENT_COUNT_ERROR,
        "sec_to_time zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT SEC_TO_TIME(1, 2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_SEC_TO_TIME_ARGUMENT_COUNT_ERROR,
        "sec_to_time extra argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE time_second_keywords(time_to_sec INT, sec_to_time INT); "
        "SELECT time_to_sec, sec_to_time FROM time_second_keywords;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_temporal_constructor_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT FROM_DAYS(366), MAKEDATE(2024, 60), MAKETIME(1, 2, 3) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_FROM_DAYS_FUNCTION,
        "from_days function"
    );
    failures += parser_test_expect_child_count(first_expression, 1U, "from_days count");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_MAKEDATE_FUNCTION,
        "makedate function"
    );
    failures += parser_test_expect_child_count(second_expression, 2U, "makedate count");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_MAKETIME_FUNCTION,
        "maketime function"
    );
    failures += parser_test_expect_child_count(third_expression, 3U, "maketime count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT FROM_DAYS (+366) AS d, MAKEDATE (2024, +1) AS md, "
        "MAKETIME (-1, +2, +3) AS mt FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO FROM_DAYS(366), MAKEDATE(2024, 1), MAKETIME(1, 2, 3);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "temporal constructor do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_FROM_DAYS_FUNCTION,
        "do from_days"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_MAKEDATE_FUNCTION,
        "do makedate"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 2U),
        MYLITE_SQL_AST_MAKETIME_FUNCTION,
        "do maketime"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT FROM_DAYS();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_FROM_DAYS_ARGUMENT_COUNT_ERROR,
        "from_days zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT FROM_DAYS(1, 2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_FROM_DAYS_ARGUMENT_COUNT_ERROR,
        "from_days extra argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT MAKEDATE(1);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_MAKEDATE_ARGUMENT_COUNT_ERROR,
        "makedate one argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT MAKETIME(1, 2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_MAKETIME_ARGUMENT_COUNT_ERROR,
        "maketime two argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE temporal_constructor_keywords(from_days INT, makedate INT, maketime INT); "
        "SELECT from_days, makedate, maketime FROM temporal_constructor_keywords;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_temporal_extract_functions(void) {
    enum {
        temporal_extract_quarter_projection_index = 3,
        temporal_extract_day_projection_index = 4,
        temporal_extract_dayofmonth_projection_index = 5,
        temporal_extract_dayofweek_projection_index = 6,
        temporal_extract_dayofyear_projection_index = 7,
        temporal_extract_last_day_projection_index = 8,
        temporal_extract_time_projection_index = 9,
        temporal_extract_week_projection_index = 10,
        temporal_extract_weekday_projection_index = 11,
        temporal_extract_weekofyear_projection_index = 12,
        temporal_extract_yearweek_projection_index = 13,
        temporal_extract_hour_projection_index = 14,
        temporal_extract_minute_projection_index = 15,
        temporal_extract_second_projection_index = 16,
        temporal_extract_microsecond_projection_index = 17,
        temporal_extract_dayname_projection_index = 18,
        temporal_extract_monthname_projection_index = 19,
        temporal_extract_do_week_index = 4,
        temporal_extract_do_weekday_index = 5,
        temporal_extract_do_weekofyear_index = 6,
        temporal_extract_do_yearweek_index = 7,
        temporal_extract_do_quarter_index = 8,
        temporal_extract_do_time_index = 9,
        temporal_extract_do_hour_index = 11,
        temporal_extract_do_microsecond_index = 12,
        temporal_extract_do_dayname_index = 13,
        temporal_extract_do_monthname_index = 14
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT DATE('2008-01-02 13:29:17'), YEAR(d), MONTH(d), QUARTER(d), "
        "DAY(d), DAYOFMONTH(d), DAYOFWEEK(d), DAYOFYEAR(d), LAST_DAY(d), TIME(dt), "
        "WEEK(d), WEEKDAY(d), WEEKOFYEAR(d), YEARWEEK(d, 3), HOUR(tm), MINUTE(tm), "
        "SECOND(tm), MICROSECOND(tm), DAYNAME(d), MONTHNAME(d) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_DATE_FUNCTION,
        "date extract function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_YEAR_FUNCTION,
        "year extract function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 2U), 0U),
        MYLITE_SQL_AST_MONTH_FUNCTION,
        "month extract function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, temporal_extract_quarter_projection_index),
            0U
        ),
        MYLITE_SQL_AST_QUARTER_FUNCTION,
        "quarter extract function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, temporal_extract_day_projection_index),
            0U
        ),
        MYLITE_SQL_AST_DAY_FUNCTION,
        "day extract function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, temporal_extract_dayofmonth_projection_index),
            0U
        ),
        MYLITE_SQL_AST_DAYOFMONTH_FUNCTION,
        "dayofmonth extract function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, temporal_extract_dayofweek_projection_index),
            0U
        ),
        MYLITE_SQL_AST_DAYOFWEEK_FUNCTION,
        "dayofweek extract function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, temporal_extract_dayofyear_projection_index),
            0U
        ),
        MYLITE_SQL_AST_DAYOFYEAR_FUNCTION,
        "dayofyear extract function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, temporal_extract_last_day_projection_index),
            0U
        ),
        MYLITE_SQL_AST_LAST_DAY_FUNCTION,
        "last_day extract function"
    );
    expression = parser_test_child_at(
        parser_test_child_at(select_list, temporal_extract_week_projection_index),
        0U
    );
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_WEEK_FUNCTION, "week extract function");
    failures += parser_test_expect_child_count(expression, 1U, "week extract child count");
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, temporal_extract_weekday_projection_index),
            0U
        ),
        MYLITE_SQL_AST_WEEKDAY_FUNCTION,
        "weekday extract function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, temporal_extract_weekofyear_projection_index),
            0U
        ),
        MYLITE_SQL_AST_WEEKOFYEAR_FUNCTION,
        "weekofyear extract function"
    );
    expression = parser_test_child_at(
        parser_test_child_at(select_list, temporal_extract_yearweek_projection_index),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_YEARWEEK_FUNCTION,
        "yearweek extract function"
    );
    failures += parser_test_expect_child_count(expression, 2U, "yearweek extract child count");
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, temporal_extract_time_projection_index),
            0U
        ),
        MYLITE_SQL_AST_TIME_FUNCTION,
        "time extract function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, temporal_extract_hour_projection_index),
            0U
        ),
        MYLITE_SQL_AST_HOUR_FUNCTION,
        "hour extract function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, temporal_extract_minute_projection_index),
            0U
        ),
        MYLITE_SQL_AST_MINUTE_FUNCTION,
        "minute extract function"
    );
    expression = parser_test_child_at(
        parser_test_child_at(select_list, temporal_extract_second_projection_index),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_SECOND_FUNCTION,
        "second extract function"
    );
    failures += parser_test_expect_child_count(expression, 1U, "second extract child count");
    expression = parser_test_child_at(
        parser_test_child_at(select_list, temporal_extract_microsecond_projection_index),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_MICROSECOND_FUNCTION,
        "microsecond function"
    );
    failures += parser_test_expect_child_count(expression, 1U, "microsecond child count");
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, temporal_extract_dayname_projection_index),
            0U
        ),
        MYLITE_SQL_AST_DAYNAME_FUNCTION,
        "dayname extract function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, temporal_extract_monthname_projection_index),
            0U
        ),
        MYLITE_SQL_AST_MONTHNAME_FUNCTION,
        "monthname extract function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DATE ('2008-01-02 13:29:17'), DAYOFWEEK ('2008-01-02') AS dow, "
        "DAYOFYEAR ('2008-01-02') AS doy, LAST_DAY ('2008-01-02') AS month_end, "
        "DAYNAME ('2008-01-02') AS day_name, MONTHNAME ('2008-01-02') AS month_name, "
        "WEEK ('2008-02-20') AS wk, WEEKDAY ('2008-02-20') AS wd, "
        "WEEKOFYEAR ('2008-02-20') AS woy, YEARWEEK ('2008-02-20', 3) AS yw, "
        "QUARTER ('2008-04-01') AS q, TIME ('13:29:17') AS tm, "
        "HOUR ('13:29:17') AS h, MICROSECOND ('13:29:17.000006') AS us "
        "FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT TO_DAYS(d), TO_SECONDS(dt), TO_DAYS ('2008-01-02') AS day_no, "
        "TO_SECONDS ('2008-01-02 13:29:17') AS seconds_no FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_TO_DAYS_FUNCTION,
        "to_days extract function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_TO_SECONDS_FUNCTION,
        "to_seconds extract function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 2U), 0U),
        MYLITE_SQL_AST_TO_DAYS_FUNCTION,
        "spaced to_days extract function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 3U), 0U),
        MYLITE_SQL_AST_TO_SECONDS_FUNCTION,
        "spaced to_seconds extract function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO DATE(NULL), DAYOFWEEK('2008-01-02'), DAYOFYEAR('2008-01-02'), "
        "LAST_DAY('2008-01-02'), WEEK('2008-02-20'), WEEKDAY('2008-02-20'), "
        "WEEKOFYEAR('2008-02-20'), YEARWEEK('2008-02-20', 3), QUARTER('2008-04-01'), "
        "TIME('13:29:17'), YEAR('2008-01-02'), HOUR('13:29:17'), "
        "MICROSECOND('13:29:17.000006'), DAYNAME('2008-01-02'), "
        "MONTHNAME('2008-01-02');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "temporal extract do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_DATE_FUNCTION,
        "do date extract"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_DAYOFWEEK_FUNCTION,
        "do dayofweek extract"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 2U),
        MYLITE_SQL_AST_DAYOFYEAR_FUNCTION,
        "do dayofyear extract"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 3U),
        MYLITE_SQL_AST_LAST_DAY_FUNCTION,
        "do last_day extract"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, temporal_extract_do_week_index),
        MYLITE_SQL_AST_WEEK_FUNCTION,
        "do week extract"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, temporal_extract_do_weekday_index),
        MYLITE_SQL_AST_WEEKDAY_FUNCTION,
        "do weekday extract"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, temporal_extract_do_weekofyear_index),
        MYLITE_SQL_AST_WEEKOFYEAR_FUNCTION,
        "do weekofyear extract"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, temporal_extract_do_yearweek_index),
        MYLITE_SQL_AST_YEARWEEK_FUNCTION,
        "do yearweek extract"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, temporal_extract_do_quarter_index),
        MYLITE_SQL_AST_QUARTER_FUNCTION,
        "do quarter extract"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, temporal_extract_do_time_index),
        MYLITE_SQL_AST_TIME_FUNCTION,
        "do time extract"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, temporal_extract_do_hour_index),
        MYLITE_SQL_AST_HOUR_FUNCTION,
        "do hour extract"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, temporal_extract_do_microsecond_index),
        MYLITE_SQL_AST_MICROSECOND_FUNCTION,
        "do microsecond function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, temporal_extract_do_dayname_index),
        MYLITE_SQL_AST_DAYNAME_FUNCTION,
        "do dayname extract"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, temporal_extract_do_monthname_index),
        MYLITE_SQL_AST_MONTHNAME_FUNCTION,
        "do monthname extract"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT EXTRACT(YEAR FROM d), EXTRACT(HOUR_SECOND FROM tm), "
        "EXTRACT(MICROSECOND FROM dt) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_EXTRACT_FUNCTION,
        "extract year function"
    );
    failures += parser_test_expect_child_count(expression, 2U, "extract year child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(expression, 0U),
        "YEAR",
        "extract year unit"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(expression, 1U),
        "d",
        "extract year argument"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_EXTRACT_FUNCTION,
        "extract hour_second function"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(expression, 0U),
        "HOUR_SECOND",
        "extract composite unit"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_EXTRACT_FUNCTION,
        "extract microsecond function"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(expression, 0U),
        "MICROSECOND",
        "extract unsupported unit"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t WHERE YEAR(d) = 2008 AND EXTRACT(YEAR FROM d) IS NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t WHERE YEAR(d) BETWEEN 2000 AND 2010 "
        "AND DAYOFMONTH(d) NOT BETWEEN 9 AND 11;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_AND_PREDICATE,
        "temporal extract between and predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT DAYNAME();", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_DAYNAME_ARGUMENT_COUNT_ERROR,
        "dayname zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT MONTHNAME('2008-01-02', 'x');", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_MONTHNAME_ARGUMENT_COUNT_ERROR,
        "monthname extra argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT DAYOFMONTH();", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_DAYOFMONTH_ARGUMENT_COUNT_ERROR,
        "dayofmonth zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT DAYOFWEEK();", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_DAYOFWEEK_ARGUMENT_COUNT_ERROR,
        "dayofweek zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT DAYOFYEAR('2008-01-02', 'x');", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_DAYOFYEAR_ARGUMENT_COUNT_ERROR,
        "dayofyear extra argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT LAST_DAY();", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_LAST_DAY_ARGUMENT_COUNT_ERROR,
        "last_day zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT WEEKDAY();", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_WEEKDAY_ARGUMENT_COUNT_ERROR,
        "weekday zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT WEEKOFYEAR('2008-01-02', 'x');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_WEEKOFYEAR_ARGUMENT_COUNT_ERROR,
        "weekofyear extra argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT YEARWEEK();", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_YEARWEEK_ARGUMENT_COUNT_ERROR,
        "yearweek zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT YEARWEEK('2008-01-02', 3, 4);", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_YEARWEEK_ARGUMENT_COUNT_ERROR,
        "yearweek extra argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT TO_DAYS();", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_TO_DAYS_ARGUMENT_COUNT_ERROR,
        "to_days zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT TO_DAYS('2008-01-02', 'x');", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_TO_DAYS_ARGUMENT_COUNT_ERROR,
        "to_days extra argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT TO_SECONDS();", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_TO_SECONDS_ARGUMENT_COUNT_ERROR,
        "to_seconds zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT TO_SECONDS('2008-01-02', 'x');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_TO_SECONDS_ARGUMENT_COUNT_ERROR,
        "to_seconds extra argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE temporal_extract_keywords(day INT, dayofmonth INT, dayofweek INT, "
        "dayofyear INT, dayname INT, last_day INT, hour INT, minute INT, month INT, "
        "monthname INT, second INT, year INT, date DATE, time TIME, extract INT, "
        "microsecond INT, quarter INT, week INT, "
        "weekday INT, weekofyear INT, yearweek INT, to_days INT, to_seconds INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT DATE();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT TIME();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT HOUR();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT MICROSECOND();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT QUARTER();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT WEEK();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT WEEK('2008-01-02', 3, 4);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT QUARTER('2008-01-02', 'x');", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT EXTRACT();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT EXTRACT(YEAR);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_abs_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT ABS(-64), Abs(NULL), abs(~0) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_ABS_FUNCTION, "abs function");
    failures += parser_test_expect_span_text(first_expression, "ABS(-64)", "abs function span");
    failures += parser_test_expect_child_count(first_expression, 1U, "abs argument count");
    failures += parser_test_expect_operator(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "abs negative integer"
    );
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_ABS_FUNCTION, "mixed abs");
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "abs NULL"
    );
    failures += parser_test_expect_node(third_expression, MYLITE_SQL_AST_ABS_FUNCTION, "lower abs");
    failures += parser_test_expect_operator(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_NOT,
        "abs bitwise argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "abs from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT ABS (1), (ABS(1)), ABS(IFNULL(NULL,-7));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    parenthesized = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_ABS_FUNCTION, "spaced abs");
    failures += parser_test_expect_span_text(first_expression, "ABS (1)", "spaced abs span");
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized abs"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parenthesized, 0U),
        MYLITE_SQL_AST_ABS_FUNCTION,
        "wrapped abs"
    );
    failures += parser_test_expect_span_text(parenthesized, "(ABS(1))", "parenthesized abs span");
    failures +=
        parser_test_expect_node(third_expression, MYLITE_SQL_AST_ABS_FUNCTION, "ifnull abs value");
    failures += parser_test_expect_node(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_IFNULL_FUNCTION,
        "abs ifnull"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ABS();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ABS_ARGUMENT_COUNT_ERROR,
        "empty abs argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT ABS(1,2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ABS_ARGUMENT_COUNT_ERROR,
        "two abs argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("CREATE TABLE abs (abs INT);", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "abs identifier");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_sign_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT SIGN(-64), Sign(NULL), sign(~0) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_SIGN_FUNCTION, "sign function");
    failures += parser_test_expect_span_text(first_expression, "SIGN(-64)", "sign function span");
    failures += parser_test_expect_child_count(first_expression, 1U, "sign argument count");
    failures += parser_test_expect_operator(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "sign negative integer"
    );
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_SIGN_FUNCTION, "mixed sign");
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "sign NULL"
    );
    failures +=
        parser_test_expect_node(third_expression, MYLITE_SQL_AST_SIGN_FUNCTION, "lower sign");
    failures += parser_test_expect_operator(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_NOT,
        "sign bitwise argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "sign from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT SIGN (1), (SIGN(1)), SIGN(IFNULL(NULL,-7));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    parenthesized = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_SIGN_FUNCTION, "spaced sign");
    failures += parser_test_expect_span_text(first_expression, "SIGN (1)", "spaced sign span");
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized sign"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parenthesized, 0U),
        MYLITE_SQL_AST_SIGN_FUNCTION,
        "wrapped sign"
    );
    failures += parser_test_expect_span_text(parenthesized, "(SIGN(1))", "parenthesized sign span");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_SIGN_FUNCTION,
        "ifnull sign value"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_IFNULL_FUNCTION,
        "sign ifnull"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT SIGN();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_SIGN_ARGUMENT_COUNT_ERROR,
        "empty sign argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT SIGN(1,2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_SIGN_ARGUMENT_COUNT_ERROR,
        "two sign argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE sign (sign INT);", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "sign identifier");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_rounding_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *fourth_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT CEIL(-64), Ceiling(NULL), floor(~0), ROUND(1+2) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    fourth_expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_CEIL_FUNCTION, "ceil function");
    failures += parser_test_expect_span_text(first_expression, "CEIL(-64)", "ceil function span");
    failures += parser_test_expect_child_count(first_expression, 1U, "ceil argument count");
    failures += parser_test_expect_operator(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "ceil negative integer"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_CEILING_FUNCTION,
        "mixed ceiling"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "ceiling NULL"
    );
    failures +=
        parser_test_expect_node(third_expression, MYLITE_SQL_AST_FLOOR_FUNCTION, "lower floor");
    failures += parser_test_expect_operator(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_NOT,
        "floor bitwise argument"
    );
    failures +=
        parser_test_expect_node(fourth_expression, MYLITE_SQL_AST_ROUND_FUNCTION, "round function");
    failures += parser_test_expect_operator(
        parser_test_child_at(fourth_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "round arithmetic argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "rounding from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT CEIL (1), (CEILING(1)), FLOOR(IFNULL(NULL,-7));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    parenthesized = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_CEIL_FUNCTION, "spaced ceil");
    failures += parser_test_expect_span_text(first_expression, "CEIL (1)", "spaced ceil span");
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized ceiling"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parenthesized, 0U),
        MYLITE_SQL_AST_CEILING_FUNCTION,
        "wrapped ceiling"
    );
    failures +=
        parser_test_expect_span_text(parenthesized, "(CEILING(1))", "parenthesized ceiling span");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_FLOOR_FUNCTION,
        "floor ifnull value"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_IFNULL_FUNCTION,
        "floor ifnull"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ROUND(123,-1);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ROUND_FUNCTION,
        "two-argument round function"
    );
    failures +=
        parser_test_expect_child_count(first_expression, 2U, "two-argument round child count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "two-argument round value"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(first_expression, 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "two-argument round places"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT CEIL();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CEIL_ARGUMENT_COUNT_ERROR,
        "empty ceil argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT CEILING(1,2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CEILING_ARGUMENT_COUNT_ERROR,
        "two ceiling argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT FLOOR();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_FLOOR_ARGUMENT_COUNT_ERROR,
        "empty floor argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT ROUND();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ROUND_ARGUMENT_COUNT_ERROR,
        "empty round argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT ROUND(1,2,3);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ROUND_ARGUMENT_COUNT_ERROR,
        "extra round argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE ceil (ceiling INT, floor INT, round INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "rounding identifiers"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_base_conversion_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *fourth_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT BIN(64), Oct(NULL), bin(~0), oct(1+2) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    fourth_expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_BIN_FUNCTION, "bin function");
    failures += parser_test_expect_span_text(first_expression, "BIN(64)", "bin function span");
    failures += parser_test_expect_child_count(first_expression, 1U, "bin argument count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "bin integer"
    );
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_OCT_FUNCTION, "mixed oct");
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "oct NULL"
    );
    failures += parser_test_expect_node(third_expression, MYLITE_SQL_AST_BIN_FUNCTION, "lower bin");
    failures += parser_test_expect_operator(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_NOT,
        "bin bitwise argument"
    );
    failures +=
        parser_test_expect_node(fourth_expression, MYLITE_SQL_AST_OCT_FUNCTION, "lower oct");
    failures += parser_test_expect_operator(
        parser_test_child_at(fourth_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "oct arithmetic argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "base conversion from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT BIN (1), (OCT(1)), CONV(1010,2,10), BIN(IFNULL(NULL,7));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    parenthesized = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    fourth_expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_BIN_FUNCTION, "spaced bin");
    failures += parser_test_expect_span_text(first_expression, "BIN (1)", "spaced bin span");
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized oct"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parenthesized, 0U),
        MYLITE_SQL_AST_OCT_FUNCTION,
        "wrapped oct"
    );
    failures += parser_test_expect_span_text(parenthesized, "(OCT(1))", "parenthesized oct span");
    failures +=
        parser_test_expect_node(third_expression, MYLITE_SQL_AST_CONV_FUNCTION, "conv function");
    failures += parser_test_expect_child_count(third_expression, 3U, "conv argument count");
    failures += parser_test_expect_literal(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "conv value"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(third_expression, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "conv from base"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(third_expression, 2U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "conv to base"
    );
    failures +=
        parser_test_expect_node(fourth_expression, MYLITE_SQL_AST_BIN_FUNCTION, "nested bin value");
    failures += parser_test_expect_node(
        parser_test_child_at(fourth_expression, 0U),
        MYLITE_SQL_AST_IFNULL_FUNCTION,
        "ifnull bin value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT BIN();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_BIN_ARGUMENT_COUNT_ERROR,
        "empty bin argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT BIN(1,2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_BIN_ARGUMENT_COUNT_ERROR,
        "two bin argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT OCT();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_OCT_ARGUMENT_COUNT_ERROR,
        "empty oct argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT OCT(1,2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_OCT_ARGUMENT_COUNT_ERROR,
        "two oct argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT CONV();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR,
        "empty conv argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT CONV(1);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR,
        "one conv argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT CONV(1,10);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR,
        "two conv argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT CONV(1,10,2,3);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR,
        "four conv argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE bin (oct INT, bin INT, conv INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "base conversion identifiers"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_bit_count_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT BIT_COUNT(64), Bit_Count(NULL), bit_count(~0) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_BIT_COUNT_FUNCTION,
        "bit_count function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "BIT_COUNT(64)", "bit_count function span");
    failures += parser_test_expect_child_count(first_expression, 1U, "bit_count argument count");
    failures += parser_test_expect_literal(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "bit_count integer"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_BIT_COUNT_FUNCTION,
        "mixed bit_count"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "bit_count NULL"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_BIT_COUNT_FUNCTION,
        "lower bit_count"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_NOT,
        "bit_count bitwise argument"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "bit_count from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT BIT_COUNT (1), (BIT_COUNT(1)), BIT_COUNT(IFNULL(NULL,7));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    parenthesized = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_BIT_COUNT_FUNCTION,
        "spaced bit_count"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "BIT_COUNT (1)", "spaced bit_count span");
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized bit_count"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parenthesized, 0U),
        MYLITE_SQL_AST_BIT_COUNT_FUNCTION,
        "wrapped bit_count"
    );
    failures += parser_test_expect_span_text(
        parenthesized,
        "(BIT_COUNT(1))",
        "parenthesized bit_count span"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_BIT_COUNT_FUNCTION,
        "nested value"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_IFNULL_FUNCTION,
        "ifnull bit_count value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT BIT_COUNT();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_BIT_COUNT_ARGUMENT_COUNT_ERROR,
        "empty bit_count argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT BIT_COUNT(1,2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_BIT_COUNT_ARGUMENT_COUNT_ERROR,
        "two bit_count argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE bit_count (bit_count INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "bit_count identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_numeric_format_truncate_crc32_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT CRC32('MySQL'), FORMAT(12332.123456,4), TRUNCATE(-1.999,1) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_CRC32_FUNCTION, "crc32 function");
    failures += parser_test_expect_span_text(expression, "CRC32('MySQL')", "crc32 span");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "crc32"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_FORMAT_FUNCTION,
        "format function"
    );
    failures += parser_test_expect_child_count(second_expression, 2U, "format argument count");
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_DECIMAL,
        "format value"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(second_expression, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "format places"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_TRUNCATE_FUNCTION,
        "truncate function"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "truncate signed value"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "numeric functions dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT crc32 (X'616263'), format(+1, -1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_CRC32_FUNCTION, "spaced crc32");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_HEX,
        "crc32 hex"
    );
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_FORMAT_FUNCTION, "lower format");
    failures += parser_test_expect_operator(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "format positive value"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(second_expression, 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "format negative places"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT CRC32();", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_CRC32_ARGUMENT_COUNT_ERROR,
        "crc32 empty argument count"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT FORMAT(1);", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_FORMAT_ARGUMENT_COUNT_ERROR,
        "format one argument count"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT FORMAT(1,2,'de_DE');", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_FORMAT_LOCALE_UNSUPPORTED,
        "format locale unsupported"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT TRUNCATE(1);", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_TRUNCATE_ARGUMENT_COUNT_ERROR,
        "truncate one argument count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE numeric_names (crc32 INT, format INT, truncate INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "numeric function identifiers"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_hex_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT HEX('abc'), HEX(X'0061'), HEX(-1), HEX(v) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_HEX_FUNCTION, "hex string function");
    failures += parser_test_expect_span_text(expression, "HEX('abc')", "hex string span");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "hex string"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_HEX_FUNCTION, "hex binary function");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_HEX,
        "hex literal"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_HEX_FUNCTION, "hex signed function");
    failures += parser_test_expect_operator(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "hex negative argument"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_HEX_FUNCTION, "hex column function");
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "hex column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "hex from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT HEX ('a'), (HEX(255)) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_HEX_FUNCTION, "spaced hex");
    failures += parser_test_expect_span_text(expression, "HEX ('a')", "spaced hex span");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized hex"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_HEX_FUNCTION,
        "wrapped hex"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT HEX();", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_HEX_ARGUMENT_COUNT_ERROR,
        "empty hex argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT HEX('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_HEX_ARGUMENT_COUNT_ERROR,
        "two hex argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DO HEX('abc'), HEX(NULL);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "hex do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_HEX_FUNCTION,
        "do hex"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_HEX_FUNCTION,
        "do null hex"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE hex_names (hex INT);", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "hex identifier");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_base64_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT TO_BASE64('abc'), FROM_BASE64(X'59574A6A'), TO_BASE64(-1), "
        "FROM_BASE64(v) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_TO_BASE64_FUNCTION,
        "to_base64 string function"
    );
    failures +=
        parser_test_expect_span_text(expression, "TO_BASE64('abc')", "to_base64 string span");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "to_base64 string"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_FROM_BASE64_FUNCTION,
        "from_base64 binary function"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_HEX,
        "from_base64 hex literal"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_TO_BASE64_FUNCTION,
        "to_base64 signed function"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "to_base64 negative argument"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_FROM_BASE64_FUNCTION,
        "from_base64 column function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "from_base64 column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "base64 from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT TO_BASE64 ('a'), (FROM_BASE64(NULL)) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_TO_BASE64_FUNCTION, "spaced to_base64");
    failures +=
        parser_test_expect_span_text(expression, "TO_BASE64 ('a')", "spaced to_base64 span");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized base64"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_FROM_BASE64_FUNCTION,
        "wrapped from_base64"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT TO_BASE64();", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_TO_BASE64_ARGUMENT_COUNT_ERROR,
        "empty to_base64 argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT FROM_BASE64('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_FROM_BASE64_ARGUMENT_COUNT_ERROR,
        "two from_base64 argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO TO_BASE64('abc'), FROM_BASE64(NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "base64 do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_TO_BASE64_FUNCTION,
        "do to_base64"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_FROM_BASE64_FUNCTION,
        "do null from_base64"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE base64_names (to_base64 INT, from_base64 INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "base64 identifiers"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_compression_random_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT COMPRESS('abc'), UNCOMPRESS(c), UNCOMPRESSED_LENGTH(c), RANDOM_BYTES(4) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_COMPRESS_FUNCTION, "compress function");
    failures += parser_test_expect_span_text(expression, "COMPRESS('abc')", "compress span");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "compress string"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_UNCOMPRESS_FUNCTION,
        "uncompress function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "uncompress column"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_FUNCTION,
        "uncompressed length function"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_RANDOM_BYTES_FUNCTION,
        "random bytes function"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "random bytes length"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "compression random from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT COMPRESS (), UNCOMPRESS('a','b'), UNCOMPRESSED_LENGTH(), RANDOM_BYTES();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_COMPRESS_ARGUMENT_COUNT_ERROR,
        "compress empty argument count error"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_UNCOMPRESS_ARGUMENT_COUNT_ERROR,
        "uncompress extra argument count error"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_ARGUMENT_COUNT_ERROR,
        "uncompressed length empty argument count error"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_RANDOM_BYTES_ARGUMENT_COUNT_ERROR,
        "random bytes empty argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO COMPRESS('abc'), UNCOMPRESS(NULL), UNCOMPRESSED_LENGTH(NULL), RANDOM_BYTES(1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "compression do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_COMPRESS_FUNCTION,
        "do compress"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_UNCOMPRESS_FUNCTION,
        "do uncompress"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 2U),
        MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_FUNCTION,
        "do uncompressed length"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 3U),
        MYLITE_SQL_AST_RANDOM_BYTES_FUNCTION,
        "do random bytes"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE compression_names (compress INT, uncompress INT, "
        "uncompressed_length INT, random_bytes INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "compression random identifiers"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_digest_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT MD5('abc'), SHA(col), SHA1(X'616263'), SHA2(v,256) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_MD5_FUNCTION, "md5 function");
    failures += parser_test_expect_span_text(expression, "MD5('abc')", "md5 span");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "md5 string"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_SHA_FUNCTION, "sha function");
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "sha column"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_SHA1_FUNCTION, "sha1 function");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_HEX,
        "sha1 hex literal"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_SHA2_FUNCTION, "sha2 function");
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "sha2 column"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "sha2 length"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "digest from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT MD5 ('a'), (SHA2('a',0)) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_MD5_FUNCTION, "spaced md5");
    failures += parser_test_expect_span_text(expression, "MD5 ('a')", "spaced md5 span");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized sha2"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_SHA2_FUNCTION,
        "wrapped sha2"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT MD5(), SHA('a','b'), SHA1(), SHA2('a'), SHA2('a',256,512);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_MD5_ARGUMENT_COUNT_ERROR,
        "md5 empty argument count error"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_SHA_ARGUMENT_COUNT_ERROR,
        "sha extra argument count error"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_SHA1_ARGUMENT_COUNT_ERROR,
        "sha1 empty argument count error"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_SHA2_ARGUMENT_COUNT_ERROR,
        "sha2 missing length argument count error"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 4U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_SHA2_ARGUMENT_COUNT_ERROR,
        "sha2 extra argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO MD5('abc'), SHA(NULL), SHA1('abc'), SHA2('abc',256);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "digest do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_MD5_FUNCTION,
        "do md5"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_SHA_FUNCTION,
        "do sha"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 2U),
        MYLITE_SQL_AST_SHA1_FUNCTION,
        "do sha1"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 3U),
        MYLITE_SQL_AST_SHA2_FUNCTION,
        "do sha2"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE digest_names (md5 INT, sha INT, sha1 INT, sha2 INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "digest identifiers"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_unhex_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT UNHEX('4D'), UNHEX(X'3431'), UNHEX(+1), UNHEX(v) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_UNHEX_FUNCTION, "unhex string function");
    failures += parser_test_expect_span_text(expression, "UNHEX('4D')", "unhex string span");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "unhex string"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_UNHEX_FUNCTION, "unhex binary function");
    failures += parser_test_expect_literal(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_HEX,
        "unhex hex literal"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_UNHEX_FUNCTION, "unhex signed function");
    failures += parser_test_expect_operator(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "unhex positive argument"
    );
    expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures +=
        parser_test_expect_node(expression, MYLITE_SQL_AST_UNHEX_FUNCTION, "unhex column function");
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "unhex column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "unhex from table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT UNHEX ('F'), (UNHEX(NULL)) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(expression, MYLITE_SQL_AST_UNHEX_FUNCTION, "spaced unhex");
    failures += parser_test_expect_span_text(expression, "UNHEX ('F')", "spaced unhex span");
    expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized unhex"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression, 0U),
        MYLITE_SQL_AST_UNHEX_FUNCTION,
        "wrapped unhex"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT UNHEX();", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_UNHEX_ARGUMENT_COUNT_ERROR,
        "empty unhex argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT UNHEX('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        expression,
        MYLITE_SQL_AST_UNHEX_ARGUMENT_COUNT_ERROR,
        "two unhex argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DO UNHEX('41'), UNHEX(NULL);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "unhex do");
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_UNHEX_FUNCTION,
        "do unhex"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 1U),
        MYLITE_SQL_AST_UNHEX_FUNCTION,
        "do null unhex"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unhex_names (unhex INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "unhex identifier");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_pi_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures +=
        parser_test_parse_sql("SELECT PI(), Pi(), pi() FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_PI_FUNCTION, "pi function");
    failures += parser_test_expect_span_text(first_expression, "PI()", "pi function span");
    failures += parser_test_expect_child_count(first_expression, 0U, "pi argument count");
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_PI_FUNCTION, "mixed pi function");
    failures += parser_test_expect_span_text(second_expression, "Pi()", "mixed pi span");
    failures +=
        parser_test_expect_node(third_expression, MYLITE_SQL_AST_PI_FUNCTION, "lower pi function");
    failures += parser_test_expect_span_text(third_expression, "pi()", "lower pi span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "pi from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT PI (), (PI());", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    parenthesized = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_PI_FUNCTION, "spaced pi function");
    failures += parser_test_expect_span_text(first_expression, "PI ()", "spaced pi span");
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized pi"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parenthesized, 0U),
        MYLITE_SQL_AST_PI_FUNCTION,
        "wrapped pi"
    );
    failures += parser_test_expect_span_text(parenthesized, "(PI())", "parenthesized pi span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT PI(1), PI(NULL), PI(1, 2);", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_PI_ARGUMENT_COUNT_ERROR,
        "pi integer argument error"
    );
    arguments = parser_test_child_at(first_expression, 0U);
    failures += parser_test_expect_node(
        arguments,
        MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST,
        "pi argument list"
    );
    failures += parser_test_expect_child_count(arguments, 1U, "pi one argument count");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_PI_ARGUMENT_COUNT_ERROR,
        "pi null argument error"
    );
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_PI_ARGUMENT_COUNT_ERROR,
        "pi multiple argument error"
    );
    arguments = parser_test_child_at(third_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 2U, "pi multiple argument count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("CREATE TABLE pi (pi INT);", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "pi identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT PI;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare pi identifier");
    failures += parser_test_expect_span_text(first_expression, "PI", "bare pi span");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_rand_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT RAND(), Rand(), rand() FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_RAND_FUNCTION, "rand function");
    failures += parser_test_expect_span_text(first_expression, "RAND()", "rand function span");
    failures += parser_test_expect_child_count(first_expression, 0U, "rand argument count");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_RAND_FUNCTION,
        "mixed rand function"
    );
    failures += parser_test_expect_span_text(second_expression, "Rand()", "mixed rand span");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_RAND_FUNCTION,
        "lower rand function"
    );
    failures += parser_test_expect_span_text(third_expression, "rand()", "lower rand span");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "rand from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT RAND (), (RAND());", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    parenthesized = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_RAND_FUNCTION, "spaced rand");
    failures += parser_test_expect_span_text(first_expression, "RAND ()", "spaced rand span");
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "wrapped rand"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parenthesized, 0U),
        MYLITE_SQL_AST_RAND_FUNCTION,
        "rand child"
    );
    failures += parser_test_expect_span_text(parenthesized, "(RAND())", "parenthesized rand span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT RAND(1), RAND(1, 2);", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_RAND_SEED_FUNCTION,
        "rand seed function"
    );
    failures += parser_test_expect_child_count(first_expression, 1U, "rand seed argument count");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR,
        "rand argument count error"
    );
    failures +=
        parser_test_expect_child_count(second_expression, 1U, "rand count marker child count");
    failures += parser_test_expect_child_count(
        parser_test_child_at(second_expression, 0U),
        1U,
        "rand extra arguments"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t ORDER BY RAND(1) DESC;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    parenthesized = parser_test_child_at(statement, 2U);
    failures +=
        parser_test_expect_node(parenthesized, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "rand order clause");
    first_expression = parser_test_child_at(parenthesized, 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_RAND_SEED_FUNCTION,
        "rand order key"
    );
    failures +=
        parser_test_expect_child_count(first_expression, 1U, "rand order seed argument count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM t ORDER BY RAND(1, 2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    parenthesized = parser_test_child_at(statement, 2U);
    first_expression = parser_test_child_at(parenthesized, 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR,
        "rand order argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DO RAND(), RAND(1);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(statement, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(statement, 0U), 1U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_RAND_FUNCTION, "do rand function");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_RAND_SEED_FUNCTION,
        "do rand seed"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT COUNT(*) AS c FROM t WHERE RAND() < 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE t SET option_value = RAND(1) WHERE option_name = 'a';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO t (option_name, option_value) VALUES ('b', RAND(1));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE rand (rand INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "rand identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT RAND;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IDENTIFIER,
        "bare rand identifier"
    );
    failures += parser_test_expect_span_text(first_expression, "RAND", "bare rand span");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_any_value_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *group_clause = NULL;
    const struct mylite_sql_ast_node *having_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT ANY_VALUE(1), any_value(NULL), Any_Value('abc') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ANY_VALUE_FUNCTION,
        "any_value integer"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "ANY_VALUE(1)", "any_value integer span");
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL,
        "integer arg"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_ANY_VALUE_FUNCTION,
        "any_value null"
    );
    failures +=
        parser_test_expect_span_text(second_expression, "any_value(NULL)", "any_value null span");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_ANY_VALUE_FUNCTION,
        "any_value string"
    );
    failures +=
        parser_test_expect_span_text(third_expression, "Any_Value('abc')", "any_value string span");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "any_value dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT g, ANY_VALUE(t.v) AS av FROM app.t GROUP BY g HAVING av IS NOT NULL "
        "ORDER BY av DESC;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    group_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_GROUP_BY_CLAUSE);
    having_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_HAVING_CLAUSE);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_ANY_VALUE_FUNCTION,
        "grouped any_value"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(second_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified any_value argument"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(group_clause, 0U),
        "g",
        "any_value group key"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(having_clause, 0U), 0U),
        "av",
        "any_value having alias"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(order_clause, 0U),
        "av",
        "any_value order alias"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "any_value order desc"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT g, ANY_VALUE(t.v) AS av FROM app.t GROUP BY g ORDER BY ANY_VALUE(t.v) DESC;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    order_clause = parser_test_first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    first_expression = parser_test_child_at(order_clause, 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ANY_VALUE_FUNCTION,
        "grouped order any_value expression"
    );
    failures += parser_test_expect_span_text(
        first_expression,
        "ANY_VALUE(t.v)",
        "grouped order any_value expression span"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "any_value expression order desc"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ANY_VALUE (1);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ANY_VALUE_FUNCTION,
        "spaced any_value"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "ANY_VALUE (1)", "spaced any_value span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ANY_VALUE();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ANY_VALUE_ARGUMENT_COUNT_ERROR,
        "empty any_value arity"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ANY_VALUE(1, 2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = parser_test_child_at(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(result.root, 0U), 0U), 0U),
        0U
    );
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ANY_VALUE_ARGUMENT_COUNT_ERROR,
        "multi any_value arity"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE any_value(id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "any_value identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ANY_VALUE(*) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT ANY_VALUE(DISTINCT v) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_validate_password_strength_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *function = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT VALIDATE_PASSWORD_STRENGTH('abc'), validate_password_strength(NULL), "
        "VaLiDaTe_PaSsWoRd_StReNgTh(col) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    function = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        function,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "validate password strength generic function"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(function, 0U),
        "VALIDATE_PASSWORD_STRENGTH",
        "validate password strength uppercase name"
    );
    arguments = parser_test_child_at(function, 1U);
    failures += parser_test_expect_node(
        arguments,
        MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST,
        "validate password strength arguments"
    );
    failures += parser_test_expect_child_count(arguments, 1U, "validate password strength arity");

    function = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        function,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "validate password strength lowercase generic function"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(function, 0U),
        "validate_password_strength",
        "validate password strength lowercase name"
    );

    function = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        function,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "validate password strength mixed-case generic function"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(function, 0U),
        "VaLiDaTe_PaSsWoRd_StReNgTh",
        "validate password strength mixed-case name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT VALIDATE_PASSWORD_STRENGTH(), VALIDATE_PASSWORD_STRENGTH('a','b');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    function = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        function,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "validate password strength zero-argument generic function"
    );
    failures += parser_test_expect_child_count(
        function,
        1U,
        "validate password strength zero-argument child count"
    );
    function = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        function,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "validate password strength extra-argument generic function"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(function, 1U),
        2U,
        "validate password strength extra-argument parse count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM vp WHERE VALIDATE_PASSWORD_STRENGTH(p) = 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    where_clause = parser_test_first_child_kind(select, MYLITE_SQL_AST_WHERE_CLAUSE);
    predicate = parser_test_child_at(where_clause, 0U);
    failures += parser_test_expect_node(
        predicate,
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "validate password strength where predicate"
    );
    failures += parser_test_expect_operator(
        predicate,
        MYLITE_SQL_AST_OPERATOR_EQUAL,
        "validate password strength where operator"
    );
    function = parser_test_child_at(predicate, 0U);
    failures += parser_test_expect_node(
        function,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "validate password strength where lhs"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(predicate, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "validate password strength where rhs"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO VALIDATE_PASSWORD_STRENGTH('abc'), VALIDATE_PASSWORD_STRENGTH(NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    expression_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_DO_STATEMENT,
        "validate password strength do"
    );
    failures += parser_test_expect_child_count(
        expression_list,
        2U,
        "validate password strength do expression count"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(expression_list, 0U),
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        "validate password strength do expression"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_sqrt_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT SQRT(4), Sqrt(9), sqrt(16) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_SQRT_FUNCTION, "sqrt function");
    failures += parser_test_expect_span_text(first_expression, "SQRT(4)", "sqrt function span");
    failures += parser_test_expect_child_count(first_expression, 1U, "sqrt argument count");
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL,
        "sqrt argument"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_SQRT_FUNCTION,
        "mixed sqrt function"
    );
    failures += parser_test_expect_span_text(second_expression, "Sqrt(9)", "mixed sqrt span");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_SQRT_FUNCTION,
        "lower sqrt function"
    );
    failures += parser_test_expect_span_text(third_expression, "sqrt(16)", "lower sqrt span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "sqrt from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT SQRT (20), (SQRT(NULL));", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    parenthesized = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_SQRT_FUNCTION,
        "spaced sqrt function"
    );
    failures += parser_test_expect_span_text(first_expression, "SQRT (20)", "spaced sqrt span");
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized sqrt"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parenthesized, 0U),
        MYLITE_SQL_AST_SQRT_FUNCTION,
        "wrapped sqrt"
    );
    failures +=
        parser_test_expect_span_text(parenthesized, "(SQRT(NULL))", "parenthesized sqrt span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT SQRT(), SQRT(1, 2);", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_SQRT_ARGUMENT_COUNT_ERROR,
        "sqrt empty argument error"
    );
    failures +=
        parser_test_expect_child_count(first_expression, 0U, "sqrt empty error child count");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_SQRT_ARGUMENT_COUNT_ERROR,
        "sqrt multiple argument error"
    );
    arguments = parser_test_child_at(second_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 1U, "sqrt multiple argument count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE sqrt (sqrt INT);", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "sqrt identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT SQRT;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_IDENTIFIER,
        "bare sqrt identifier"
    );
    failures += parser_test_expect_span_text(first_expression, "SQRT", "bare sqrt span");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_angle_conversion_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT DEGREES(1), Degrees(2), radians(180) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_DEGREES_FUNCTION,
        "degrees function"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "DEGREES(1)", "degrees function span");
    failures += parser_test_expect_child_count(first_expression, 1U, "degrees argument count");
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL,
        "degrees argument"
    );
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_DEGREES_FUNCTION,
        "mixed degrees function"
    );
    failures += parser_test_expect_span_text(second_expression, "Degrees(2)", "mixed degrees span");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_RADIANS_FUNCTION,
        "lower radians function"
    );
    failures +=
        parser_test_expect_span_text(third_expression, "radians(180)", "lower radians span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "angle from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT DEGREES (2), (RADIANS(NULL));", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    parenthesized = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_DEGREES_FUNCTION,
        "spaced degrees"
    );
    failures +=
        parser_test_expect_span_text(first_expression, "DEGREES (2)", "spaced degrees span");
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "wrapped radians"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parenthesized, 0U),
        MYLITE_SQL_AST_RADIANS_FUNCTION,
        "radians child"
    );
    failures += parser_test_expect_span_text(
        parenthesized,
        "(RADIANS(NULL))",
        "parenthesized radians span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT DEGREES(), DEGREES(1, 2), RADIANS(), RADIANS(1, 2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_DEGREES_ARGUMENT_COUNT_ERROR,
        "degrees empty argument error"
    );
    failures +=
        parser_test_expect_child_count(first_expression, 0U, "degrees empty error child count");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_DEGREES_ARGUMENT_COUNT_ERROR,
        "degrees multiple argument error"
    );
    arguments = parser_test_child_at(second_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 1U, "degrees multiple argument count");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_RADIANS_ARGUMENT_COUNT_ERROR,
        "radians empty argument error"
    );
    failures +=
        parser_test_expect_child_count(third_expression, 0U, "radians empty error child count");
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 3U), 0U),
        MYLITE_SQL_AST_RADIANS_ARGUMENT_COUNT_ERROR,
        "radians multiple argument error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE degrees (radians INT);", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "angle identifiers");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT DEGREES, RADIANS;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare degrees");
    failures += parser_test_expect_span_text(first_expression, "DEGREES", "bare degrees span");
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_IDENTIFIER, "bare radians");
    failures += parser_test_expect_span_text(second_expression, "RADIANS", "bare radians span");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_inverse_trig_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT ACOS(1), Acos(0), asin(-1) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_ACOS_FUNCTION, "acos function");
    failures += parser_test_expect_span_text(first_expression, "ACOS(1)", "acos function span");
    failures += parser_test_expect_child_count(first_expression, 1U, "acos argument count");
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL,
        "acos arg"
    );
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_ACOS_FUNCTION, "mixed acos");
    failures += parser_test_expect_span_text(second_expression, "Acos(0)", "mixed acos span");
    failures +=
        parser_test_expect_node(third_expression, MYLITE_SQL_AST_ASIN_FUNCTION, "lower asin");
    failures += parser_test_expect_span_text(third_expression, "asin(-1)", "lower asin span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "inverse trig dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT ACOS (0), (ASIN(NULL));", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    parenthesized = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_ACOS_FUNCTION, "spaced acos");
    failures += parser_test_expect_span_text(first_expression, "ACOS (0)", "spaced acos span");
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "wrapped asin"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parenthesized, 0U),
        MYLITE_SQL_AST_ASIN_FUNCTION,
        "asin child"
    );
    failures +=
        parser_test_expect_span_text(parenthesized, "(ASIN(NULL))", "parenthesized asin span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT ACOS(1) AS a, ASIN(0) s;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_ACOS_FUNCTION, "aliased acos");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 1U),
        "a",
        "acos alias"
    );
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_ASIN_FUNCTION, "aliased asin");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        "s",
        "asin alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT ACOS(), ACOS(1, 2), ASIN(), ASIN(1, 2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ACOS_ARGUMENT_COUNT_ERROR,
        "acos empty argument error"
    );
    failures +=
        parser_test_expect_child_count(first_expression, 0U, "acos empty error child count");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_ACOS_ARGUMENT_COUNT_ERROR,
        "acos multiple argument error"
    );
    arguments = parser_test_child_at(second_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 1U, "acos multiple argument count");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_ASIN_ARGUMENT_COUNT_ERROR,
        "asin empty argument error"
    );
    failures +=
        parser_test_expect_child_count(third_expression, 0U, "asin empty error child count");
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 3U), 0U),
        MYLITE_SQL_AST_ASIN_ARGUMENT_COUNT_ERROR,
        "asin multiple argument error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE acos (asin INT);", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "inverse trig ids");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ACOS, ASIN;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare acos");
    failures += parser_test_expect_span_text(first_expression, "ACOS", "bare acos span");
    failures += parser_test_expect_node(second_expression, MYLITE_SQL_AST_IDENTIFIER, "bare asin");
    failures += parser_test_expect_span_text(second_expression, "ASIN", "bare asin span");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_direct_trig_functions(void) {
    enum { cot_multiple_argument_select_item_index = 5U };

    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *fourth_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT SIN(1), Cos(0), tan(-1), COT(2) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    fourth_expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_SIN_FUNCTION, "sin function");
    failures += parser_test_expect_span_text(first_expression, "SIN(1)", "sin function span");
    failures += parser_test_expect_child_count(first_expression, 1U, "sin argument count");
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL,
        "sin arg"
    );
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_COS_FUNCTION, "mixed cos");
    failures += parser_test_expect_span_text(second_expression, "Cos(0)", "mixed cos span");
    failures += parser_test_expect_node(third_expression, MYLITE_SQL_AST_TAN_FUNCTION, "lower tan");
    failures += parser_test_expect_span_text(third_expression, "tan(-1)", "lower tan span");
    failures +=
        parser_test_expect_node(fourth_expression, MYLITE_SQL_AST_COT_FUNCTION, "cot function");
    failures += parser_test_expect_span_text(fourth_expression, "COT(2)", "cot function span");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "direct trig dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT SIN (0), (COT(NULL));", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    parenthesized = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_SIN_FUNCTION, "spaced sin");
    failures += parser_test_expect_span_text(first_expression, "SIN (0)", "spaced sin span");
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "wrapped cot"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parenthesized, 0U),
        MYLITE_SQL_AST_COT_FUNCTION,
        "cot child"
    );
    failures +=
        parser_test_expect_span_text(parenthesized, "(COT(NULL))", "parenthesized cot span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT SIN(1) AS s, COS(0) c;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_SIN_FUNCTION, "aliased sin");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 1U),
        "s",
        "sin alias"
    );
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_COS_FUNCTION, "aliased cos");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        "c",
        "cos alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT SIN(), SIN(1, 2), COS(), TAN(1, 2), COT(), COT(1, 2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_SIN_ARGUMENT_COUNT_ERROR,
        "sin empty argument error"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_SIN_ARGUMENT_COUNT_ERROR,
        "sin multiple argument error"
    );
    arguments =
        parser_test_child_at(parser_test_child_at(parser_test_child_at(select_list, 1U), 0U), 0U);
    failures += parser_test_expect_child_count(arguments, 1U, "sin multiple argument count");
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 2U), 0U),
        MYLITE_SQL_AST_COS_ARGUMENT_COUNT_ERROR,
        "cos empty argument error"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 3U), 0U),
        MYLITE_SQL_AST_TAN_ARGUMENT_COUNT_ERROR,
        "tan multiple argument error"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 4U), 0U),
        MYLITE_SQL_AST_COT_ARGUMENT_COUNT_ERROR,
        "cot empty argument error"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, cot_multiple_argument_select_item_index),
            0U
        ),
        MYLITE_SQL_AST_COT_ARGUMENT_COUNT_ERROR,
        "cot multiple argument error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DO SIN(1), COS(NULL), TAN(TRUE), COT(2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(select, MYLITE_SQL_AST_DO_STATEMENT, "direct trig do");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE sin (cos INT, tan INT, cot INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "direct trig ids");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT SIN, COS, TAN, COT;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    fourth_expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures += parser_test_expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare sin");
    failures += parser_test_expect_span_text(first_expression, "SIN", "bare sin span");
    failures += parser_test_expect_node(second_expression, MYLITE_SQL_AST_IDENTIFIER, "bare cos");
    failures += parser_test_expect_span_text(second_expression, "COS", "bare cos span");
    failures += parser_test_expect_node(third_expression, MYLITE_SQL_AST_IDENTIFIER, "bare tan");
    failures += parser_test_expect_span_text(third_expression, "TAN", "bare tan span");
    failures += parser_test_expect_node(fourth_expression, MYLITE_SQL_AST_IDENTIFIER, "bare cot");
    failures += parser_test_expect_span_text(fourth_expression, "COT", "bare cot span");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_atan_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *fourth_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT ATAN(1), Atan(0, -1), atan2(-1), ATAN2(1, -1) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    fourth_expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_ATAN_FUNCTION, "atan function");
    failures += parser_test_expect_span_text(first_expression, "ATAN(1)", "atan function span");
    failures += parser_test_expect_child_count(first_expression, 1U, "atan one-argument count");
    failures += parser_test_expect_node(
        parser_test_child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL,
        "atan arg"
    );
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_ATAN_FUNCTION, "mixed atan");
    failures += parser_test_expect_span_text(second_expression, "Atan(0, -1)", "mixed atan span");
    failures += parser_test_expect_child_count(second_expression, 2U, "atan two-argument count");
    failures +=
        parser_test_expect_node(third_expression, MYLITE_SQL_AST_ATAN2_FUNCTION, "lower atan2");
    failures += parser_test_expect_span_text(third_expression, "atan2(-1)", "lower atan2 span");
    failures += parser_test_expect_child_count(third_expression, 1U, "atan2 one-argument count");
    failures +=
        parser_test_expect_node(fourth_expression, MYLITE_SQL_AST_ATAN2_FUNCTION, "atan2 two args");
    failures += parser_test_expect_child_count(fourth_expression, 2U, "atan2 two-argument count");
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "atan dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT ATAN (0), (ATAN2(NULL, 1));", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    parenthesized = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_ATAN_FUNCTION, "spaced atan");
    failures += parser_test_expect_span_text(first_expression, "ATAN (0)", "spaced atan span");
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "wrapped atan2"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parenthesized, 0U),
        MYLITE_SQL_AST_ATAN2_FUNCTION,
        "atan2 child"
    );
    failures +=
        parser_test_expect_span_text(parenthesized, "(ATAN2(NULL, 1))", "parenthesized atan2 span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT ATAN(1) AS a, ATAN2(1,-1) t;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_ATAN_FUNCTION, "aliased atan");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 1U),
        "a",
        "atan alias"
    );
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_ATAN2_FUNCTION, "aliased atan2");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 1U),
        "t",
        "atan2 alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT ATAN(), ATAN(1, 2, 3), ATAN2(), ATAN2(1, 2, 3);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    fourth_expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_ATAN_ARGUMENT_COUNT_ERROR,
        "atan empty argument error"
    );
    failures +=
        parser_test_expect_child_count(first_expression, 0U, "atan empty error child count");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_ATAN_ARGUMENT_COUNT_ERROR,
        "atan multiple argument error"
    );
    arguments = parser_test_child_at(second_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 1U, "atan multiple argument count");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_ATAN2_ARGUMENT_COUNT_ERROR,
        "atan2 empty argument error"
    );
    failures +=
        parser_test_expect_child_count(third_expression, 0U, "atan2 empty error child count");
    failures += parser_test_expect_node(
        fourth_expression,
        MYLITE_SQL_AST_ATAN2_ARGUMENT_COUNT_ERROR,
        "atan2 multiple argument error"
    );
    arguments = parser_test_child_at(fourth_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 1U, "atan2 multiple argument count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE atan (atan2 INT);", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "atan ids");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT ATAN, ATAN2;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures += parser_test_expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare atan");
    failures += parser_test_expect_span_text(first_expression, "ATAN", "bare atan span");
    failures += parser_test_expect_node(second_expression, MYLITE_SQL_AST_IDENTIFIER, "bare atan2");
    failures += parser_test_expect_span_text(second_expression, "ATAN2", "bare atan2 span");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_exp_log_power_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *fourth_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SELECT EXP(1), Ln(2), log(10, 100), LOG10(100), LOG2(8), pow(2,3), "
        "POWER(3,2) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(select, 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    fourth_expression = parser_test_child_at(parser_test_child_at(select_list, 3U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_EXP_FUNCTION, "exp function");
    failures += parser_test_expect_span_text(first_expression, "EXP(1)", "exp span");
    failures += parser_test_expect_child_count(first_expression, 1U, "exp child count");
    failures +=
        parser_test_expect_node(second_expression, MYLITE_SQL_AST_LN_FUNCTION, "mixed ln function");
    failures += parser_test_expect_span_text(second_expression, "Ln(2)", "mixed ln span");
    failures +=
        parser_test_expect_node(third_expression, MYLITE_SQL_AST_LOG_FUNCTION, "two-argument log");
    failures +=
        parser_test_expect_child_count(third_expression, 2U, "two-argument log child count");
    failures +=
        parser_test_expect_node(fourth_expression, MYLITE_SQL_AST_LOG10_FUNCTION, "log10 function");
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 4U), 0U),
        MYLITE_SQL_AST_LOG2_FUNCTION,
        "log2 function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, exp_log_power_pow_index), 0U),
        MYLITE_SQL_AST_POW_FUNCTION,
        "pow function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, exp_log_power_power_index), 0U),
        MYLITE_SQL_AST_POWER_FUNCTION,
        "power function"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "log power dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT EXP (1), (POWER(NULL, 2));", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    parenthesized = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    failures +=
        parser_test_expect_node(first_expression, MYLITE_SQL_AST_EXP_FUNCTION, "spaced exp");
    failures += parser_test_expect_span_text(first_expression, "EXP (1)", "spaced exp span");
    failures += parser_test_expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "wrapped power"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parenthesized, 0U),
        MYLITE_SQL_AST_POWER_FUNCTION,
        "power child"
    );
    failures +=
        parser_test_expect_span_text(parenthesized, "(POWER(NULL, 2))", "wrapped power span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT EXP(), EXP(1, 2), LN(), LOG10(1, 2), LOG2(), POW(), POW(2), "
        "POWER(2, 3, 4);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    first_expression = parser_test_child_at(parser_test_child_at(select_list, 0U), 0U);
    second_expression = parser_test_child_at(parser_test_child_at(select_list, 1U), 0U);
    third_expression = parser_test_child_at(parser_test_child_at(select_list, 2U), 0U);
    failures += parser_test_expect_node(
        first_expression,
        MYLITE_SQL_AST_EXP_ARGUMENT_COUNT_ERROR,
        "exp empty argument error"
    );
    failures += parser_test_expect_child_count(first_expression, 0U, "exp empty child count");
    failures += parser_test_expect_node(
        second_expression,
        MYLITE_SQL_AST_EXP_ARGUMENT_COUNT_ERROR,
        "exp multiple argument error"
    );
    arguments = parser_test_child_at(second_expression, 0U);
    failures += parser_test_expect_child_count(arguments, 1U, "exp extra arguments");
    failures += parser_test_expect_node(
        third_expression,
        MYLITE_SQL_AST_LN_ARGUMENT_COUNT_ERROR,
        "ln empty argument error"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 3U), 0U),
        MYLITE_SQL_AST_LOG10_ARGUMENT_COUNT_ERROR,
        "log10 multiple argument error"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 4U), 0U),
        MYLITE_SQL_AST_LOG2_ARGUMENT_COUNT_ERROR,
        "log2 empty argument error"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, exp_log_power_pow_index), 0U),
        MYLITE_SQL_AST_POW_ARGUMENT_COUNT_ERROR,
        "pow empty argument error"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, exp_log_power_power_index), 0U),
        MYLITE_SQL_AST_POW_ARGUMENT_COUNT_ERROR,
        "pow missing argument error"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, exp_log_power_wrong_arity_power_index),
            0U
        ),
        MYLITE_SQL_AST_POWER_ARGUMENT_COUNT_ERROR,
        "power multiple argument error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT LOG(), LOG(1,2,3);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE exp (log INT, pow INT);", MYLITE_SQL_PARSE_OK, &result);
    select = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "math identifiers");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT EXP, LN, LOG, LOG10, LOG2, POW, POWER;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    for (size_t index = 0U; index < exp_log_power_identifier_count; ++index) {
        failures += parser_test_expect_node(
            parser_test_child_at(parser_test_child_at(select_list, index), 0U),
            MYLITE_SQL_AST_IDENTIFIER,
            "bare exp log power identifier"
        );
    }
    mylite_sql_parse_result_deinit(&result);

    return failures;
}
