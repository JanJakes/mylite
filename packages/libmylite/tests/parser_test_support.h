#ifndef PARSER_TEST_SUPPORT_H
#define PARSER_TEST_SUPPORT_H

#include "sql/mylite_ast.h"
#include "sql/mylite_parser.h"

#include <stddef.h>

enum {
    small_integer_column_count = 6,
    signed_integer_column_count = 6,
    alias_integer_column_count = 10,
    display_width_column_count = 16,
    bool_alias_column_count = 3,
    integer_default_column_count = 5,
    decimal_column_count = 6,
    approximate_column_count = 11,
    year_column_count = 3,
    date_column_count = 3,
    datetime_column_count = 3,
    time_column_count = 3,
    timestamp_column_count = 3,
    decimal_fixed_column_index = 5,
    alias_int1_unsigned_column_index = 5,
    alias_int2_signed_column_index = 6,
    alias_int3_unsigned_column_index = 7,
    alias_int4_unsigned_column_index = 8,
    alias_int8_unsigned_column_index = 9,
    display_width_int_unsigned_column_index = 8,
    display_width_tinyint_signed_column_index = 9,
    display_width_tinyint_unsigned_column_index = 10,
    display_width_int1_column_index = 11,
    display_width_int8_column_index = 15,
    mediumint_unsigned_column_index = 5,
    signed_integer_column_index = 4,
    signed_bigint_column_index = 5,
    exp_log_power_pow_index = 5,
    exp_log_power_power_index = 6,
    exp_log_power_wrong_arity_power_index = 7,
    exp_log_power_identifier_count = 7,
    char_function_variadic_argument_count = 5,
    reverse_table_option_count = 5,
    storage_table_option_count = 7,
    storage_stats_auto_recalc_option_index = 5,
    storage_stats_sample_pages_option_index = 6,
    interval_parser_threshold_count = 6,
    export_set_parser_argument_count = 5,
};

struct parser_test_parse_modes {
    unsigned int value;
};

int parser_test_parse_sql(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    struct mylite_sql_parse_result *out_result
);
int parser_test_parse_sql_with_ignore_space(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    struct mylite_sql_parse_result *out_result
);
int parser_test_parse_sql_with_modes(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    struct parser_test_parse_modes modes,
    struct mylite_sql_parse_result *out_result
);
const struct mylite_sql_ast_node *parser_test_child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
);
const struct mylite_sql_ast_node *parser_test_first_child_kind(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
);
int parser_test_expect_node(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind expected_kind,
    const char *context
);
int parser_test_expect_child_count(
    const struct mylite_sql_ast_node *node,
    size_t expected,
    const char *context
);
int parser_test_expect_span_text(
    const struct mylite_sql_ast_node *node,
    const char *expected,
    const char *context
);
int parser_test_expect_literal(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_literal_kind expected,
    const char *context
);
int parser_test_expect_operator(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_operator expected,
    const char *context
);
int parser_test_expect_true(int condition, const char *context);
int parser_test_expect_integer_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_integer_type expected_type,
    int expected_unsigned,
    const char *context
);
int parser_test_expect_varchar_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    const char *context
);
int parser_test_expect_national_varchar_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    const char *context
);
int parser_test_expect_char_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    int expected_explicit_length,
    const char *context
);
int parser_test_expect_national_char_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    int expected_explicit_length,
    const char *context
);
int parser_test_expect_text_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_text_type expected,
    const char *context
);
int parser_test_expect_text_type_length(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    const char *context
);
int parser_test_expect_binary_string_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_binary_string_type expected,
    const char *context
);
int parser_test_expect_bit_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    int expected_explicit_length,
    const char *context
);
int parser_test_expect_year_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_width,
    int expected_explicit_width,
    const char *context
);
int parser_test_expect_decimal_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_decimal_type expected,
    const char *expected_precision,
    const char *expected_scale,
    int expected_unsigned,
    const char *context
);
int parser_test_expect_approximate_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_approximate_type expected,
    const char *expected_precision,
    const char *expected_scale,
    int expected_unsigned,
    const char *context
);
int parser_test_expect_integer_display_width(
    const struct mylite_sql_ast_node *node,
    const char *expected_width,
    const char *context
);
int parser_test_expect_integer_bool_alias(
    const struct mylite_sql_ast_node *node,
    const char *context
);
int parser_test_expect_integer_serial_alias(
    const struct mylite_sql_ast_node *node,
    const char *context
);
int parser_test_expect_nullability(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_nullability expected,
    const char *context
);
int parser_test_expect_order_direction(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_order_direction expected,
    const char *context
);
int parser_test_expect_column_visibility(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_column_visibility expected,
    const char *context
);

#endif
