#include "sql/mylite_ast.h"
#include "sql/mylite_parser.h"

#include <stdio.h>
#include <string.h>

enum {
    small_integer_column_count = 6,
    signed_integer_column_count = 6,
    alias_integer_column_count = 10,
    display_width_column_count = 16,
    bool_alias_column_count = 3,
    integer_default_column_count = 5,
    decimal_column_count = 6,
    approximate_column_count = 9,
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
    reverse_table_option_count = 5,
};

static int test_empty_script(void);
static int test_use_statements(void);
static int test_select_expression_list(void);
static int test_current_database_functions(void);
static int test_current_user_identity_functions(void);
static int test_current_role_function(void);
static int test_if_function(void);
static int test_ifnull_function(void);
static int test_coalesce_function(void);
static int test_concat_function(void);
static int test_concat_ws_function(void);
static int test_replace_function(void);
static int test_field_function(void);
static int test_json_valid_function(void);
static int test_json_extract_functions(void);
static int test_json_construction_functions(void);
static int test_json_introspection_functions(void);
static int test_cast_binary_expression(void);
static int test_date_add_second_function(void);
static int test_date_format_function(void);
static int test_datediff_function(void);
static int test_unix_timestamp_function(void);
static int test_temporal_extract_functions(void);
static int test_scalar_subquery_expression(void);
static int test_nullif_function(void);
static int test_isnull_function(void);
static int test_abs_function(void);
static int test_sign_function(void);
static int test_rounding_functions(void);
static int test_base_conversion_functions(void);
static int test_bit_count_function(void);
static int test_numeric_format_truncate_crc32_functions(void);
static int test_hex_function(void);
static int test_unhex_function(void);
static int test_charset_collation_functions(void);
static int test_string_length_functions(void);
static int test_string_case_functions(void);
static int test_string_trim_functions(void);
static int test_string_slice_functions(void);
static int test_string_padding_functions(void);
static int test_string_search_functions(void);
static int test_find_in_set_function(void);
static int test_regexp_like_function(void);
static int test_pi_function(void);
static int test_rand_function(void);
static int test_sqrt_function(void);
static int test_angle_conversion_functions(void);
static int test_inverse_trig_functions(void);
static int test_direct_trig_functions(void);
static int test_atan_functions(void);
static int test_exp_log_power_functions(void);
static int test_case_operator(void);
static int test_do_statement(void);
static int test_set_fixed_system_variable_statement(void);
static int test_sql_prepared_statement_lifecycle(void);
static int test_set_transaction_statement(void);
static int test_version_function(void);
static int test_connection_id_function(void);
static int test_row_count_function(void);
static int test_found_rows_function(void);
static int test_last_insert_id_function(void);
static int test_diagnostics_count_system_variables(void);
static int test_count_star_aggregate(void);
static int test_min_max_aggregate(void);
static int test_sum_aggregate(void);
static int test_avg_aggregate(void);
static int test_bitwise_aggregate(void);
static int test_group_concat_aggregate(void);
static int test_unary_and_parenthesized_expression(void);
static int test_literal_categories(void);
static int test_qualified_identifier_keyword_part(void);
static int test_schema_lifecycle_statements(void);
static int test_table_lifecycle_statements(void);
static int test_create_table_comment_option_statements(void);
static int test_alter_table_comment_statements(void);
static int test_show_full_tables_statements(void);
static int test_show_table_status_where_statement(void);
static int test_table_binary_charset_options(void);
static int test_alter_table_column_position_statements(void);
static int test_column_position_nonreserved_identifier_statements(void);
static int test_column_charset_collation_attribute_statements(void);
static int test_empty_insert_values_statements(void);
static int test_table_maintenance_statements(void);
static int test_temporary_table_lifecycle_statements(void);
static int test_serial_alias_statements(void);
static int test_varchar_type_statements(void);
static int test_char_type_statements(void);
static int test_text_type_statements(void);
static int test_json_type_statements(void);
static int test_enum_type_statements(void);
static int test_set_type_statements(void);
static int test_bit_type_statements(void);
static int test_year_type_statements(void);
static int test_decimal_type_statements(void);
static int test_approximate_type_statements(void);
static int test_date_type_statements(void);
static int test_datetime_type_statements(void);
static int test_time_type_statements(void);
static int test_timestamp_type_statements(void);
static int test_current_date_time_function_statements(void);
static int test_utc_date_time_function_statements(void);
static int test_create_table_primary_key_statements(void);
static int test_create_table_foreign_key_statements(void);
static int test_create_index_statements(void);
static int test_drop_index_statements(void);
static int test_alter_table_add_primary_key_statements(void);
static int test_alter_table_add_index_statements(void);
static int test_alter_table_add_foreign_key_statements(void);
static int test_alter_table_drop_foreign_key_statements(void);
static int test_alter_table_drop_index_statements(void);
static int test_alter_table_rename_index_statements(void);
static int test_alter_table_index_visibility_statements(void);
static int test_alter_table_check_constraint_statements(void);
static int test_alter_table_drop_primary_key_statements(void);
static int test_alter_table_auto_increment_option_statements(void);
static int test_create_table_like_statements(void);
static int test_create_table_select_statements(void);
static int test_alter_table_default_charset_collation_statements(void);
static int test_alter_table_order_by_statements(void);
static int test_alter_table_algorithm_lock_option_statements(void);
static int test_alter_table_force_statements(void);
static int test_show_columns_introspection_statements(void);
static int test_show_triggers_empty_introspection_statements(void);
static int test_show_events_empty_introspection_statements(void);
static int test_show_open_tables_empty_introspection_statements(void);
static int test_show_routine_status_empty_introspection_statements(void);
static int test_show_processlist_introspection_statements(void);
static int test_show_grants_statement(void);
static int test_show_warnings_diagnostics_statements(void);
static int test_show_errors_diagnostics_statements(void);
static int test_show_index_empty_introspection_statements(void);
static int test_show_variables_statement(void);
static int test_show_status_statement(void);
static int test_select_where_predicates(void);
static int test_select_order_limit_clauses(void);
static int test_select_group_by_clause(void);
static int test_select_distinct_clause(void);
static int test_select_sql_calc_found_rows_clause(void);
static int test_select_noop_modifier_clause(void);
static int test_select_locking_clause(void);
static int test_select_all_clause(void);
static int test_select_union_clause(void);
static int test_select_table_alias_clause(void);
static int test_select_inner_join_clause(void);
static int test_select_item_alias_clause(void);
static int test_insert_select_statement(void);
static int test_insert_modifier_statements(void);
static int test_insert_on_duplicate_key_update_statement(void);
static int test_replace_select_statement(void);
static int test_replace_modifier_statements(void);
static int test_delete_statement(void);
static int test_update_statement(void);
static int test_transaction_control_statements(void);
static int test_table_lock_statements(void);
static int test_comments_are_skipped(void);
static int test_syntax_errors(void);
static int test_lexer_errors(void);
static int parse_sql(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    struct mylite_sql_parse_result *out_result
);
static int parse_sql_with_ignore_space(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    struct mylite_sql_parse_result *out_result
);
static const struct mylite_sql_ast_node *child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
);
static const struct mylite_sql_ast_node *first_child_kind(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
);
static int expect_node(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind expected_kind,
    const char *context
);
static int expect_child_count(
    const struct mylite_sql_ast_node *node,
    size_t expected,
    const char *context
);
static int expect_span_text(
    const struct mylite_sql_ast_node *node,
    const char *expected,
    const char *context
);
static int expect_literal(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_literal_kind expected,
    const char *context
);
static int expect_operator(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_operator expected,
    const char *context
);
static int expect_true(int condition, const char *context);
static int expect_integer_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_integer_type expected_type,
    int expected_unsigned,
    const char *context
);
static int expect_varchar_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    const char *context
);
static int expect_national_varchar_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    const char *context
);
static int expect_char_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    int expected_explicit_length,
    const char *context
);
static int expect_national_char_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    int expected_explicit_length,
    const char *context
);
static int expect_text_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_text_type expected,
    const char *context
);
static int expect_binary_string_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_binary_string_type expected,
    const char *context
);
static int expect_bit_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    int expected_explicit_length,
    const char *context
);
static int expect_year_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_width,
    int expected_explicit_width,
    const char *context
);
static int expect_decimal_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_decimal_type expected,
    const char *expected_precision,
    const char *expected_scale,
    int expected_unsigned,
    const char *context
);
static int expect_approximate_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_approximate_type expected,
    const char *expected_precision,
    int expected_unsigned,
    const char *context
);
static int expect_integer_display_width(
    const struct mylite_sql_ast_node *node,
    const char *expected_width,
    const char *context
);
static int expect_integer_bool_alias(const struct mylite_sql_ast_node *node, const char *context);
static int expect_integer_serial_alias(const struct mylite_sql_ast_node *node, const char *context);
static int expect_nullability(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_nullability expected,
    const char *context
);
static int expect_order_direction(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_order_direction expected,
    const char *context
);
static int expect_column_visibility(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_column_visibility expected,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_empty_script();
    failures += test_use_statements();
    failures += test_select_expression_list();
    failures += test_current_database_functions();
    failures += test_current_user_identity_functions();
    failures += test_current_role_function();
    failures += test_if_function();
    failures += test_ifnull_function();
    failures += test_coalesce_function();
    failures += test_concat_function();
    failures += test_concat_ws_function();
    failures += test_replace_function();
    failures += test_field_function();
    failures += test_json_valid_function();
    failures += test_json_extract_functions();
    failures += test_json_construction_functions();
    failures += test_json_introspection_functions();
    failures += test_cast_binary_expression();
    failures += test_date_add_second_function();
    failures += test_date_format_function();
    failures += test_datediff_function();
    failures += test_unix_timestamp_function();
    failures += test_temporal_extract_functions();
    failures += test_scalar_subquery_expression();
    failures += test_nullif_function();
    failures += test_isnull_function();
    failures += test_abs_function();
    failures += test_sign_function();
    failures += test_rounding_functions();
    failures += test_base_conversion_functions();
    failures += test_bit_count_function();
    failures += test_numeric_format_truncate_crc32_functions();
    failures += test_hex_function();
    failures += test_unhex_function();
    failures += test_charset_collation_functions();
    failures += test_string_length_functions();
    failures += test_string_case_functions();
    failures += test_string_trim_functions();
    failures += test_string_slice_functions();
    failures += test_string_padding_functions();
    failures += test_string_search_functions();
    failures += test_find_in_set_function();
    failures += test_regexp_like_function();
    failures += test_pi_function();
    failures += test_rand_function();
    failures += test_sqrt_function();
    failures += test_angle_conversion_functions();
    failures += test_inverse_trig_functions();
    failures += test_direct_trig_functions();
    failures += test_atan_functions();
    failures += test_exp_log_power_functions();
    failures += test_case_operator();
    failures += test_do_statement();
    failures += test_set_fixed_system_variable_statement();
    failures += test_sql_prepared_statement_lifecycle();
    failures += test_set_transaction_statement();
    failures += test_version_function();
    failures += test_connection_id_function();
    failures += test_row_count_function();
    failures += test_found_rows_function();
    failures += test_last_insert_id_function();
    failures += test_diagnostics_count_system_variables();
    failures += test_count_star_aggregate();
    failures += test_min_max_aggregate();
    failures += test_sum_aggregate();
    failures += test_avg_aggregate();
    failures += test_bitwise_aggregate();
    failures += test_group_concat_aggregate();
    failures += test_unary_and_parenthesized_expression();
    failures += test_literal_categories();
    failures += test_qualified_identifier_keyword_part();
    failures += test_schema_lifecycle_statements();
    failures += test_table_lifecycle_statements();
    failures += test_create_table_comment_option_statements();
    failures += test_alter_table_comment_statements();
    failures += test_show_full_tables_statements();
    failures += test_show_table_status_where_statement();
    failures += test_table_binary_charset_options();
    failures += test_alter_table_column_position_statements();
    failures += test_column_position_nonreserved_identifier_statements();
    failures += test_column_charset_collation_attribute_statements();
    failures += test_empty_insert_values_statements();
    failures += test_table_maintenance_statements();
    failures += test_temporary_table_lifecycle_statements();
    failures += test_serial_alias_statements();
    failures += test_varchar_type_statements();
    failures += test_char_type_statements();
    failures += test_text_type_statements();
    failures += test_json_type_statements();
    failures += test_enum_type_statements();
    failures += test_set_type_statements();
    failures += test_bit_type_statements();
    failures += test_year_type_statements();
    failures += test_decimal_type_statements();
    failures += test_approximate_type_statements();
    failures += test_date_type_statements();
    failures += test_datetime_type_statements();
    failures += test_time_type_statements();
    failures += test_timestamp_type_statements();
    failures += test_current_date_time_function_statements();
    failures += test_utc_date_time_function_statements();
    failures += test_create_table_primary_key_statements();
    failures += test_create_table_foreign_key_statements();
    failures += test_create_index_statements();
    failures += test_drop_index_statements();
    failures += test_alter_table_add_primary_key_statements();
    failures += test_alter_table_add_index_statements();
    failures += test_alter_table_add_foreign_key_statements();
    failures += test_alter_table_drop_foreign_key_statements();
    failures += test_alter_table_drop_index_statements();
    failures += test_alter_table_rename_index_statements();
    failures += test_alter_table_index_visibility_statements();
    failures += test_alter_table_check_constraint_statements();
    failures += test_alter_table_drop_primary_key_statements();
    failures += test_alter_table_auto_increment_option_statements();
    failures += test_create_table_like_statements();
    failures += test_create_table_select_statements();
    failures += test_alter_table_default_charset_collation_statements();
    failures += test_alter_table_order_by_statements();
    failures += test_alter_table_algorithm_lock_option_statements();
    failures += test_alter_table_force_statements();
    failures += test_show_columns_introspection_statements();
    failures += test_show_triggers_empty_introspection_statements();
    failures += test_show_events_empty_introspection_statements();
    failures += test_show_open_tables_empty_introspection_statements();
    failures += test_show_routine_status_empty_introspection_statements();
    failures += test_show_processlist_introspection_statements();
    failures += test_show_grants_statement();
    failures += test_show_warnings_diagnostics_statements();
    failures += test_show_errors_diagnostics_statements();
    failures += test_show_index_empty_introspection_statements();
    failures += test_show_variables_statement();
    failures += test_show_status_statement();
    failures += test_select_where_predicates();
    failures += test_select_order_limit_clauses();
    failures += test_select_group_by_clause();
    failures += test_select_distinct_clause();
    failures += test_select_sql_calc_found_rows_clause();
    failures += test_select_noop_modifier_clause();
    failures += test_select_locking_clause();
    failures += test_select_all_clause();
    failures += test_select_union_clause();
    failures += test_select_table_alias_clause();
    failures += test_select_inner_join_clause();
    failures += test_select_item_alias_clause();
    failures += test_insert_select_statement();
    failures += test_insert_modifier_statements();
    failures += test_insert_on_duplicate_key_update_statement();
    failures += test_replace_select_statement();
    failures += test_replace_modifier_statements();
    failures += test_delete_statement();
    failures += test_update_statement();
    failures += test_transaction_control_statements();
    failures += test_table_lock_statements();
    failures += test_comments_are_skipped();
    failures += test_syntax_errors();
    failures += test_lexer_errors();

    return failures == 0 ? 0 : 1;
}

static int test_empty_script(void) {
    struct mylite_sql_parse_result result;
    int failures = 0;

    failures += parse_sql("", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(result.root, MYLITE_SQL_AST_SCRIPT, "empty root");
    failures += expect_child_count(result.root, 0U, "empty root");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 1U, "trailing semicolon root");
    failures += expect_node(
        child_at(result.root, 0U),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "trailing semicolon statement"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_use_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *first_use = NULL;
    const struct mylite_sql_ast_node *second_use = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("USE mylite_seed; USE `select`;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 2U, "use root");

    first_use = child_at(result.root, 0U);
    second_use = child_at(result.root, 1U);
    failures += expect_node(first_use, MYLITE_SQL_AST_USE_STATEMENT, "first use");
    failures += expect_span_text(child_at(first_use, 0U), "mylite_seed", "first schema");
    failures += expect_node(second_use, MYLITE_SQL_AST_USE_STATEMENT, "second use");
    failures += expect_span_text(child_at(second_use, 0U), "`select`", "second schema");

    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET NAMES utf8mb4;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SET_NAMES_STATEMENT, "set names statement");
    failures += expect_child_count(statement, 1U, "set names child count");
    failures += expect_span_text(child_at(statement, 0U), "utf8mb4", "set names charset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET NAMES names;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SET_NAMES_STATEMENT, "set names names");
    failures += expect_child_count(statement, 1U, "set names names child count");
    failures += expect_span_text(child_at(statement, 0U), "names", "names charset identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SET NAMES 'utf8mb4' COLLATE `utf8mb4_0900_ai_ci`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SET_NAMES_STATEMENT, "set names collate statement");
    failures += expect_child_count(statement, 2U, "set names collate child count");
    failures += expect_literal(
        child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "set names string charset"
    );
    failures +=
        expect_span_text(child_at(statement, 1U), "`utf8mb4_0900_ai_ci`", "set names collation");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET NAMES DEFAULT;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        child_at(statement, 0U),
        MYLITE_SQL_AST_SET_CHARACTER_SET_DEFAULT_TARGET,
        "set names default target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET CHARACTER SET UTF8MB4;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT,
        "set character set statement"
    );
    failures += expect_span_text(child_at(statement, 0U), "UTF8MB4", "set character set target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET CHARSET DEFAULT;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT, "set charset statement");
    failures += expect_node(
        child_at(statement, 0U),
        MYLITE_SQL_AST_SET_CHARACTER_SET_DEFAULT_TARGET,
        "set charset default target"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_expression_list(void) {
    enum {
        expected_select_item_count = 10,
        percent_item_index = 1,
        mod_operator_item_index = 2,
        mod_function_item_index = 3,
        div_operator_item_index = 4,
        slash_operator_item_index = 5,
        string_item_index = 6,
        true_item_index = 7,
        false_item_index = 8,
        null_item_index = 9,
        div_precedence_item_index = 4,
        div_associativity_item_index = 5,
        comparison_item_count = 8,
        comparison_arithmetic_precedence_item_index = 0,
        comparison_associativity_item_index = 1,
        greater_associativity_item_index = 2,
        null_safe_equality_item_index = 3,
        angle_not_equal_item_index = 4,
        bang_not_equal_item_index = 5,
        less_equal_item_index = 6,
        greater_equal_item_index = 7,
        logical_item_count = 9,
        logical_and_comparison_item_index = 0,
        logical_and_arithmetic_item_index = 1,
        logical_or_item_index = 2,
        logical_xor_item_index = 3,
        logical_not_item_index = 4,
        logical_not_group_item_index = 5,
        logical_or_precedence_item_index = 6,
        logical_xor_precedence_item_index = 7,
        logical_result_comparison_item_index = 8,
        is_item_count = 10,
        is_true_item_index = 0,
        is_false_item_index = 1,
        is_unknown_item_index = 2,
        is_not_true_item_index = 3,
        is_null_item_index = 4,
        is_not_null_item_index = 5,
        is_not_precedence_item_index = 6,
        is_comparison_precedence_item_index = 7,
        is_logical_precedence_item_index = 8,
        is_parenthesized_comparison_item_index = 9,
        bitwise_item_count = 10,
        bitwise_or_item_index = 0,
        bitwise_and_item_index = 1,
        bitwise_xor_item_index = 2,
        bitwise_shift_item_index = 3,
        bitwise_shift_arithmetic_item_index = 4,
        bitwise_not_item_index = 5,
        bitwise_not_arithmetic_item_index = 6,
        bitwise_and_or_item_index = 7,
        bitwise_xor_and_item_index = 8,
        bitwise_logical_xor_item_index = 9,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_item = NULL;
    const struct mylite_sql_ast_node *add = NULL;
    const struct mylite_sql_ast_node *multiply = NULL;
    const struct mylite_sql_ast_node *percent = NULL;
    const struct mylite_sql_ast_node *mod_operator = NULL;
    const struct mylite_sql_ast_node *mod_function = NULL;
    const struct mylite_sql_ast_node *div_operator = NULL;
    const struct mylite_sql_ast_node *slash_operator = NULL;
    const struct mylite_sql_ast_node *comparison = NULL;
    const struct mylite_sql_ast_node *logical = NULL;
    const struct mylite_sql_ast_node *is_expression = NULL;
    const struct mylite_sql_ast_node *bitwise = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT 1 + 2 * 3, 5 % 2, 5 MOD 2, MOD(5,2), 5 DIV 2, 5 / 2, "
        "'text', TRUE, FALSE, NULL FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );

    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_item = child_at(select_list, 0U);
    add = child_at(first_item, 0U);
    multiply = child_at(add, 1U);
    percent = child_at(child_at(select_list, percent_item_index), 0U);
    mod_operator = child_at(child_at(select_list, mod_operator_item_index), 0U);
    mod_function = child_at(child_at(select_list, mod_function_item_index), 0U);
    div_operator = child_at(child_at(select_list, div_operator_item_index), 0U);
    slash_operator = child_at(child_at(select_list, slash_operator_item_index), 0U);

    failures += expect_node(select, MYLITE_SQL_AST_SELECT_STATEMENT, "select statement");
    failures += expect_node(select_list, MYLITE_SQL_AST_SELECT_LIST, "select list");
    failures += expect_child_count(select_list, expected_select_item_count, "select list");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "from dual");

    failures += expect_node(add, MYLITE_SQL_AST_BINARY_EXPRESSION, "add expression");
    failures += expect_operator(add, MYLITE_SQL_AST_OPERATOR_ADD, "add expression");
    failures += expect_span_text(child_at(add, 0U), "1", "add left");
    failures += expect_node(multiply, MYLITE_SQL_AST_BINARY_EXPRESSION, "multiply expression");
    failures += expect_operator(multiply, MYLITE_SQL_AST_OPERATOR_MULTIPLY, "multiply expression");
    failures += expect_span_text(child_at(multiply, 0U), "2", "multiply left");
    failures += expect_span_text(child_at(multiply, 1U), "3", "multiply right");
    failures += expect_node(percent, MYLITE_SQL_AST_BINARY_EXPRESSION, "percent expression");
    failures += expect_operator(percent, MYLITE_SQL_AST_OPERATOR_MODULO, "percent operator");
    failures += expect_span_text(child_at(percent, 0U), "5", "percent left");
    failures += expect_span_text(child_at(percent, 1U), "2", "percent right");
    failures += expect_node(mod_operator, MYLITE_SQL_AST_BINARY_EXPRESSION, "mod expression");
    failures += expect_operator(mod_operator, MYLITE_SQL_AST_OPERATOR_MODULO, "mod operator");
    failures += expect_span_text(child_at(mod_operator, 0U), "5", "mod left");
    failures += expect_span_text(child_at(mod_operator, 1U), "2", "mod right");
    failures += expect_node(mod_function, MYLITE_SQL_AST_MOD_FUNCTION, "mod function");
    failures += expect_child_count(mod_function, 2U, "mod function argument count");
    failures += expect_span_text(mod_function, "MOD(5,2)", "mod function span");
    failures += expect_node(div_operator, MYLITE_SQL_AST_BINARY_EXPRESSION, "div expression");
    failures +=
        expect_operator(div_operator, MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE, "div operator");
    failures += expect_span_text(child_at(div_operator, 0U), "5", "div left");
    failures += expect_span_text(child_at(div_operator, 1U), "2", "div right");
    failures += expect_node(slash_operator, MYLITE_SQL_AST_BINARY_EXPRESSION, "slash expression");
    failures += expect_operator(slash_operator, MYLITE_SQL_AST_OPERATOR_DIVIDE, "slash operator");
    failures += expect_span_text(child_at(slash_operator, 0U), "5", "slash left");
    failures += expect_span_text(child_at(slash_operator, 1U), "2", "slash right");

    failures += expect_literal(
        child_at(child_at(select_list, string_item_index), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "string literal"
    );
    failures += expect_literal(
        child_at(child_at(select_list, true_item_index), 0U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "true literal"
    );
    failures += expect_literal(
        child_at(child_at(select_list, false_item_index), 0U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "false literal"
    );
    failures += expect_literal(
        child_at(child_at(select_list, null_item_index), 0U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "null literal"
    );

    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT 1+2=3, 1<2=1, 3>2>1, NULL<=>NULL, 1<>2, 1!=1, 2<=2, 3>=4;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, comparison_item_count, "comparison select list");
    comparison = child_at(child_at(select_list, comparison_arithmetic_precedence_item_index), 0U);
    failures += expect_operator(comparison, MYLITE_SQL_AST_OPERATOR_EQUAL, "comparison equality");
    failures += expect_operator(
        child_at(comparison, 0U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "comparison arithmetic precedence"
    );
    comparison = child_at(child_at(select_list, comparison_associativity_item_index), 0U);
    failures +=
        expect_operator(comparison, MYLITE_SQL_AST_OPERATOR_EQUAL, "comparison associativity");
    failures += expect_operator(
        child_at(comparison, 0U),
        MYLITE_SQL_AST_OPERATOR_LESS,
        "comparison left associativity"
    );
    comparison = child_at(child_at(select_list, greater_associativity_item_index), 0U);
    failures +=
        expect_operator(comparison, MYLITE_SQL_AST_OPERATOR_GREATER, "greater associativity");
    failures += expect_operator(
        child_at(comparison, 0U),
        MYLITE_SQL_AST_OPERATOR_GREATER,
        "greater left associativity"
    );
    failures += expect_operator(
        child_at(child_at(select_list, null_safe_equality_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL,
        "null safe equality expression"
    );
    failures += expect_operator(
        child_at(child_at(select_list, angle_not_equal_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_NOT_EQUAL,
        "angle not equal expression"
    );
    failures += expect_operator(
        child_at(child_at(select_list, bang_not_equal_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_NOT_EQUAL,
        "bang not equal expression"
    );
    failures += expect_operator(
        child_at(child_at(select_list, less_equal_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_LESS_EQUAL,
        "less equal expression"
    );
    failures += expect_operator(
        child_at(child_at(select_list, greater_equal_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL,
        "greater equal expression"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT 1<2 AND 2<3, 1+2 AND 0, 1 OR 0, 1 XOR 0, NOT 1<2, "
        "NOT (1>2), 0 OR 0 AND 1, 1 XOR 1 AND 0, (1 AND 1)=1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, logical_item_count, "logical select list");
    logical = child_at(child_at(select_list, logical_and_comparison_item_index), 0U);
    failures += expect_operator(logical, MYLITE_SQL_AST_OPERATOR_LOGICAL_AND, "logical and");
    failures += expect_operator(
        child_at(logical, 0U),
        MYLITE_SQL_AST_OPERATOR_LESS,
        "logical and comparison left"
    );
    logical = child_at(child_at(select_list, logical_and_arithmetic_item_index), 0U);
    failures +=
        expect_operator(logical, MYLITE_SQL_AST_OPERATOR_LOGICAL_AND, "logical arithmetic and");
    failures += expect_operator(
        child_at(logical, 0U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "logical and arithmetic precedence"
    );
    failures += expect_operator(
        child_at(child_at(select_list, logical_or_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_OR,
        "logical or"
    );
    failures += expect_operator(
        child_at(child_at(select_list, logical_xor_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR,
        "logical xor"
    );
    logical = child_at(child_at(select_list, logical_not_item_index), 0U);
    failures += expect_operator(logical, MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT, "logical not");
    failures += expect_operator(
        child_at(logical, 0U),
        MYLITE_SQL_AST_OPERATOR_LESS,
        "logical not comparison precedence"
    );
    failures += expect_operator(
        child_at(child_at(select_list, logical_not_group_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT,
        "logical not group"
    );
    logical = child_at(child_at(select_list, logical_or_precedence_item_index), 0U);
    failures += expect_operator(logical, MYLITE_SQL_AST_OPERATOR_LOGICAL_OR, "or precedence");
    failures += expect_operator(
        child_at(logical, 1U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_AND,
        "and binds tighter than or"
    );
    logical = child_at(child_at(select_list, logical_xor_precedence_item_index), 0U);
    failures += expect_operator(logical, MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR, "xor precedence");
    failures += expect_operator(
        child_at(logical, 1U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_AND,
        "and binds tighter than xor"
    );
    comparison = child_at(child_at(select_list, logical_result_comparison_item_index), 0U);
    failures +=
        expect_operator(comparison, MYLITE_SQL_AST_OPERATOR_EQUAL, "logical result comparison");
    logical = child_at(comparison, 0U);
    failures += expect_node(
        logical,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized logical comparison operand"
    );
    failures += expect_operator(
        child_at(logical, 0U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_AND,
        "logical result compared"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT 1 IS TRUE, 0 IS FALSE, NULL IS UNKNOWN, 1 IS NOT TRUE, "
        "1 IS NULL, 1 IS NOT NULL, NOT 1 IS TRUE, 1 = 1 IS TRUE, "
        "1 IS TRUE AND 0, (1 IS TRUE)=1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, is_item_count, "scalar is select list");
    failures += expect_operator(
        child_at(child_at(select_list, is_true_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_IS_TRUE,
        "scalar is true expression"
    );
    failures += expect_operator(
        child_at(child_at(select_list, is_false_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_IS_FALSE,
        "scalar is false expression"
    );
    failures += expect_operator(
        child_at(child_at(select_list, is_unknown_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN,
        "scalar is unknown expression"
    );
    failures += expect_operator(
        child_at(child_at(select_list, is_not_true_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE,
        "scalar is not true expression"
    );
    failures += expect_operator(
        child_at(child_at(select_list, is_null_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_IS_NULL,
        "scalar is null expression"
    );
    failures += expect_operator(
        child_at(child_at(select_list, is_not_null_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL,
        "scalar is not null expression"
    );
    logical = child_at(child_at(select_list, is_not_precedence_item_index), 0U);
    failures += expect_operator(logical, MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT, "not over is");
    failures += expect_operator(
        child_at(logical, 0U),
        MYLITE_SQL_AST_OPERATOR_IS_TRUE,
        "is binds tighter than not"
    );
    is_expression = child_at(child_at(select_list, is_comparison_precedence_item_index), 0U);
    failures +=
        expect_operator(is_expression, MYLITE_SQL_AST_OPERATOR_IS_TRUE, "comparison then is");
    failures += expect_operator(
        child_at(is_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_EQUAL,
        "comparison binds before is"
    );
    logical = child_at(child_at(select_list, is_logical_precedence_item_index), 0U);
    failures += expect_operator(logical, MYLITE_SQL_AST_OPERATOR_LOGICAL_AND, "is then and");
    failures += expect_operator(
        child_at(logical, 0U),
        MYLITE_SQL_AST_OPERATOR_IS_TRUE,
        "is binds tighter than and"
    );
    comparison = child_at(child_at(select_list, is_parenthesized_comparison_item_index), 0U);
    failures +=
        expect_operator(comparison, MYLITE_SQL_AST_OPERATOR_EQUAL, "parenthesized is compare");
    is_expression = child_at(comparison, 0U);
    failures += expect_node(
        is_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized is comparison operand"
    );
    failures += expect_operator(
        child_at(is_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_IS_TRUE,
        "parenthesized is compared"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT 1+5%2*3, 5*3%4, 5%3%2, -(5%2), 1+5 DIV 2*3, 5 DIV 3 DIV 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    add = child_at(child_at(select_list, 0U), 0U);
    multiply = child_at(add, 1U);
    percent = child_at(multiply, 0U);
    failures += expect_operator(add, MYLITE_SQL_AST_OPERATOR_ADD, "modulo addition precedence");
    failures += expect_operator(multiply, MYLITE_SQL_AST_OPERATOR_MULTIPLY, "modulo multiply");
    failures += expect_operator(percent, MYLITE_SQL_AST_OPERATOR_MODULO, "modulo before multiply");
    percent = child_at(child_at(select_list, 1U), 0U);
    failures += expect_operator(percent, MYLITE_SQL_AST_OPERATOR_MODULO, "multiply before modulo");
    failures += expect_operator(
        child_at(percent, 0U),
        MYLITE_SQL_AST_OPERATOR_MULTIPLY,
        "modulo left multiplication"
    );
    percent = child_at(child_at(select_list, 2U), 0U);
    failures += expect_operator(percent, MYLITE_SQL_AST_OPERATOR_MODULO, "modulo associativity");
    failures += expect_operator(
        child_at(percent, 0U),
        MYLITE_SQL_AST_OPERATOR_MODULO,
        "modulo left associativity"
    );
    failures += expect_operator(
        child_at(child_at(child_at(child_at(select_list, 3U), 0U), 0U), 0U),
        MYLITE_SQL_AST_OPERATOR_MODULO,
        "unary modulo child"
    );
    div_operator = child_at(child_at(select_list, div_precedence_item_index), 0U);
    multiply = child_at(div_operator, 1U);
    failures +=
        expect_operator(div_operator, MYLITE_SQL_AST_OPERATOR_ADD, "div addition precedence");
    failures += expect_operator(multiply, MYLITE_SQL_AST_OPERATOR_MULTIPLY, "div multiply");
    failures += expect_operator(
        child_at(multiply, 0U),
        MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE,
        "div before multiply"
    );
    div_operator = child_at(child_at(select_list, div_associativity_item_index), 0U);
    failures +=
        expect_operator(div_operator, MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE, "div associativity");
    failures += expect_operator(
        child_at(div_operator, 0U),
        MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE,
        "div left associativity"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1+5/2*3, 5/3/2, 5/2 DIV 1;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    add = child_at(child_at(select_list, 0U), 0U);
    multiply = child_at(add, 1U);
    slash_operator = child_at(multiply, 0U);
    failures += expect_operator(add, MYLITE_SQL_AST_OPERATOR_ADD, "slash addition precedence");
    failures += expect_operator(multiply, MYLITE_SQL_AST_OPERATOR_MULTIPLY, "slash multiply");
    failures +=
        expect_operator(slash_operator, MYLITE_SQL_AST_OPERATOR_DIVIDE, "slash before multiply");
    slash_operator = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_operator(slash_operator, MYLITE_SQL_AST_OPERATOR_DIVIDE, "slash associativity");
    failures += expect_operator(
        child_at(slash_operator, 0U),
        MYLITE_SQL_AST_OPERATOR_DIVIDE,
        "slash left associativity"
    );
    div_operator = child_at(child_at(select_list, 2U), 0U);
    failures += expect_operator(
        div_operator,
        MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE,
        "slash div same precedence"
    );
    failures += expect_operator(
        child_at(div_operator, 0U),
        MYLITE_SQL_AST_OPERATOR_DIVIDE,
        "slash before div"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT 1|2, 1&2, 1^2, 1<<2, 1<<2+1, ~1, ~(1+2), 1&2|4, "
        "1^3&2, 1 XOR 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, bitwise_item_count, "bitwise select list");
    failures += expect_operator(
        child_at(child_at(select_list, bitwise_or_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_OR,
        "bitwise or"
    );
    failures += expect_operator(
        child_at(child_at(select_list, bitwise_and_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_AND,
        "bitwise and"
    );
    failures += expect_operator(
        child_at(child_at(select_list, bitwise_xor_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_XOR,
        "bitwise xor"
    );
    failures += expect_operator(
        child_at(child_at(select_list, bitwise_shift_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_LEFT_SHIFT,
        "left shift"
    );
    bitwise = child_at(child_at(select_list, bitwise_shift_arithmetic_item_index), 0U);
    failures += expect_operator(bitwise, MYLITE_SQL_AST_OPERATOR_LEFT_SHIFT, "shift arithmetic");
    failures += expect_operator(
        child_at(bitwise, 1U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "addition binds tighter than shift"
    );
    bitwise = child_at(child_at(select_list, bitwise_not_item_index), 0U);
    failures += expect_operator(bitwise, MYLITE_SQL_AST_OPERATOR_BITWISE_NOT, "bitwise not");
    bitwise = child_at(child_at(select_list, bitwise_not_arithmetic_item_index), 0U);
    failures += expect_operator(bitwise, MYLITE_SQL_AST_OPERATOR_BITWISE_NOT, "bitwise not group");
    failures += expect_node(
        child_at(bitwise, 0U),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "bitwise not parenthesized child"
    );
    failures += expect_operator(
        child_at(child_at(bitwise, 0U), 0U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "bitwise not sees parenthesized arithmetic"
    );
    bitwise = child_at(child_at(select_list, bitwise_and_or_item_index), 0U);
    failures += expect_operator(bitwise, MYLITE_SQL_AST_OPERATOR_BITWISE_OR, "and before or");
    failures += expect_operator(
        child_at(bitwise, 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_AND,
        "bitwise and binds tighter than or"
    );
    bitwise = child_at(child_at(select_list, bitwise_xor_and_item_index), 0U);
    failures += expect_operator(bitwise, MYLITE_SQL_AST_OPERATOR_BITWISE_AND, "xor before and");
    failures += expect_operator(
        child_at(bitwise, 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_XOR,
        "bitwise xor binds tighter than and"
    );
    failures += expect_operator(
        child_at(child_at(select_list, bitwise_logical_xor_item_index), 0U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR,
        "keyword xor remains logical xor"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_current_database_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    int failures = 0;

    failures += parse_sql("SELECT DATABASE(), SCHEMA();", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_DATABASE_FUNCTION, "database function");
    failures += expect_span_text(first_expression, "DATABASE()", "database function span");
    failures += expect_node(second_expression, MYLITE_SQL_AST_SCHEMA_FUNCTION, "schema function");
    failures += expect_span_text(second_expression, "SCHEMA()", "schema function span");
    failures += expect_child_count(first_expression, 0U, "database function child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT database(), schema() FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_DATABASE_FUNCTION, "lower database function");
    failures += expect_span_text(first_expression, "database()", "lower database span");
    failures +=
        expect_node(second_expression, MYLITE_SQL_AST_SCHEMA_FUNCTION, "lower schema function");
    failures += expect_span_text(second_expression, "schema()", "lower schema span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "function from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DATABASE (), (SCHEMA());", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_DATABASE_FUNCTION, "spaced database function");
    failures += expect_span_text(first_expression, "DATABASE ()", "spaced database span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized schema function"
    );
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_SCHEMA_FUNCTION,
        "wrapped schema function"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_current_user_identity_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *fourth_expression = NULL;
    int failures = 0;

    failures +=
        parse_sql("SELECT USER(), CURRENT_USER(), CURRENT_USER;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_USER_FUNCTION, "user function");
    failures += expect_span_text(first_expression, "USER()", "user function span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_CURRENT_USER_FUNCTION,
        "current user function"
    );
    failures += expect_span_text(second_expression, "CURRENT_USER()", "current user function span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_CURRENT_USER_FUNCTION,
        "bare current user function"
    );
    failures += expect_span_text(third_expression, "CURRENT_USER", "bare current user span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT user(), current_user FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_USER_FUNCTION, "lower user function");
    failures += expect_span_text(first_expression, "user()", "lower user span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_CURRENT_USER_FUNCTION,
        "lower current user keyword"
    );
    failures += expect_span_text(second_expression, "current_user", "lower current user span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "identity from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT SESSION_USER(), SYSTEM_USER(), session_user(), system_user() FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    fourth_expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_SESSION_USER_FUNCTION,
        "session user function"
    );
    failures += expect_span_text(first_expression, "SESSION_USER()", "session user span");
    failures +=
        expect_node(second_expression, MYLITE_SQL_AST_SYSTEM_USER_FUNCTION, "system user function");
    failures += expect_span_text(second_expression, "SYSTEM_USER()", "system user span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_SESSION_USER_FUNCTION,
        "lower session user function"
    );
    failures += expect_span_text(third_expression, "session_user()", "lower session user span");
    failures += expect_node(
        fourth_expression,
        MYLITE_SQL_AST_SYSTEM_USER_FUNCTION,
        "lower system user function"
    );
    failures += expect_span_text(fourth_expression, "system_user()", "lower system user span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "alias from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT SESSION_USER(/* inside */), SYSTEM_USER(/* inside */);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_SESSION_USER_FUNCTION,
        "commented session user function"
    );
    failures += expect_span_text(
        first_expression,
        "SESSION_USER(/* inside */)",
        "commented session user span"
    );
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_SYSTEM_USER_FUNCTION,
        "commented system user function"
    );
    failures += expect_span_text(
        second_expression,
        "SYSTEM_USER(/* inside */)",
        "commented system user span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT USER (), (CURRENT_USER);", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_USER_FUNCTION, "spaced user function");
    failures += expect_span_text(first_expression, "USER ()", "spaced user span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized current user keyword"
    );
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_CURRENT_USER_FUNCTION,
        "wrapped current user keyword"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT (SESSION_USER()), (System_User());", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized session user"
    );
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_SESSION_USER_FUNCTION,
        "wrapped session user"
    );
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized system user"
    );
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_SYSTEM_USER_FUNCTION,
        "wrapped system user"
    );
    failures += expect_span_text(second_expression, "(System_User())", "wrapped system user span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE user (user INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "user identifier table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE session_user (system_user INT, session_user INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    failures += expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "identity alias identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE session (local INT, global INT, off INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    failures += expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "nonreserved SET keyword identifier table"
    );
    failures += expect_span_text(child_at(select, 0U), "session", "session table identifier");
    failures += expect_span_text(
        child_at(child_at(child_at(select, 1U), 0U), 0U),
        "local",
        "local column identifier"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(select, 1U), 1U), 0U),
        "global",
        "global column identifier"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(select, 1U), 2U), 0U),
        "off",
        "off column identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT USER;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare user identifier");
    failures += expect_span_text(first_expression, "USER", "bare user span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SESSION_USER, SYSTEM_USER;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare session user identifier");
    failures +=
        expect_node(second_expression, MYLITE_SQL_AST_IDENTIFIER, "bare system user identifier");
    failures += expect_span_text(first_expression, "SESSION_USER", "bare session user span");
    failures += expect_span_text(second_expression, "SYSTEM_USER", "bare system user span");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_current_role_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT CURRENT_ROLE(), Current_Role(), current_role() FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION,
        "current role function"
    );
    failures += expect_span_text(first_expression, "CURRENT_ROLE()", "current role span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION,
        "mixed-case current role function"
    );
    failures +=
        expect_span_text(second_expression, "Current_Role()", "mixed-case current role span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION,
        "lower current role function"
    );
    failures += expect_span_text(third_expression, "current_role()", "lower current role span");
    failures +=
        expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "current role from dual");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT CURRENT_ROLE (), (CURRENT_ROLE());", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION,
        "spaced current role function"
    );
    failures += expect_span_text(first_expression, "CURRENT_ROLE ()", "spaced current role span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized current role function"
    );
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION,
        "wrapped current role function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT CURRENT_ROLE(1), CURRENT_ROLE(NULL), CURRENT_ROLE('x'), CURRENT_ROLE(1, 2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR,
        "current role integer argument error"
    );
    failures += expect_span_text(first_expression, "CURRENT_ROLE(1)", "current role integer span");
    arguments = child_at(first_expression, 0U);
    failures +=
        expect_node(arguments, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, "current role argument list");
    failures += expect_child_count(arguments, 1U, "current role one argument count");
    failures += expect_literal(
        child_at(arguments, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "current role integer argument"
    );
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR,
        "current role null argument error"
    );
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR,
        "current role string argument error"
    );
    arguments = child_at(child_at(child_at(select_list, 3U), 0U), 0U);
    failures += expect_child_count(arguments, 2U, "current role multiple argument count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE current_role (current_role INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures +=
        expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "current role identifier table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CURRENT_ROLE;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare current role identifier");
    failures += expect_span_text(first_expression, "CURRENT_ROLE", "bare current role span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CURRENT_ROLE() LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_if_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    const struct mylite_sql_ast_node *nested = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT IF(1,2,3), If(TRUE,NULL,FALSE), if(+0,-1,+1) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IF_FUNCTION, "if function");
    failures += expect_span_text(first_expression, "IF(1,2,3)", "if function span");
    failures += expect_child_count(first_expression, 3U, "if function argument count");
    failures +=
        expect_literal(child_at(first_expression, 0U), MYLITE_SQL_AST_LITERAL_INTEGER, "if cond");
    failures +=
        expect_literal(child_at(first_expression, 1U), MYLITE_SQL_AST_LITERAL_INTEGER, "if true");
    failures +=
        expect_literal(child_at(first_expression, 2U), MYLITE_SQL_AST_LITERAL_INTEGER, "if false");
    failures += expect_node(second_expression, MYLITE_SQL_AST_IF_FUNCTION, "mixed-case if");
    failures +=
        expect_literal(child_at(second_expression, 0U), MYLITE_SQL_AST_LITERAL_TRUE, "if TRUE");
    failures +=
        expect_literal(child_at(second_expression, 1U), MYLITE_SQL_AST_LITERAL_NULL, "if NULL");
    failures +=
        expect_literal(child_at(second_expression, 2U), MYLITE_SQL_AST_LITERAL_FALSE, "if FALSE");
    failures += expect_node(third_expression, MYLITE_SQL_AST_IF_FUNCTION, "lower if");
    failures += expect_operator(
        child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "if positive condition"
    );
    failures += expect_operator(
        child_at(third_expression, 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "if negative branch"
    );
    failures += expect_operator(
        child_at(third_expression, 2U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "if positive branch"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "if from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT IF (1,2,3), (IF(1,2,3)), IF(IF(1,1,0),2,3);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    nested = child_at(third_expression, 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IF_FUNCTION, "spaced if function");
    failures += expect_span_text(first_expression, "IF (1,2,3)", "spaced if span");
    failures +=
        expect_node(parenthesized, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "parenthesized if");
    failures +=
        expect_node(child_at(parenthesized, 0U), MYLITE_SQL_AST_IF_FUNCTION, "wrapped if function");
    failures += expect_span_text(parenthesized, "(IF(1,2,3))", "parenthesized if span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_IF_FUNCTION, "outer nested if");
    failures += expect_node(nested, MYLITE_SQL_AST_IF_FUNCTION, "inner nested if");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT IF(1, 2 + 3, 'x');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IF_FUNCTION, "deferred if args");
    failures += expect_node(
        child_at(first_expression, 1U),
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "deferred if arithmetic argument"
    );
    failures += expect_literal(
        child_at(first_expression, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "deferred if string argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT IF();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT IF(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT IF(1,2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT IF(1,2,3,4);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_ifnull_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    const struct mylite_sql_ast_node *nested = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT IFNULL(1,2), IfNull(TRUE,NULL), ifnull(+0,-1) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IFNULL_FUNCTION, "ifnull function");
    failures += expect_span_text(first_expression, "IFNULL(1,2)", "ifnull function span");
    failures += expect_child_count(first_expression, 2U, "ifnull function argument count");
    failures += expect_literal(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "ifnull value"
    );
    failures += expect_literal(
        child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "ifnull fallback"
    );
    failures += expect_node(second_expression, MYLITE_SQL_AST_IFNULL_FUNCTION, "mixed ifnull");
    failures +=
        expect_literal(child_at(second_expression, 0U), MYLITE_SQL_AST_LITERAL_TRUE, "ifnull TRUE");
    failures +=
        expect_literal(child_at(second_expression, 1U), MYLITE_SQL_AST_LITERAL_NULL, "ifnull NULL");
    failures += expect_node(third_expression, MYLITE_SQL_AST_IFNULL_FUNCTION, "lower ifnull");
    failures += expect_operator(
        child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "ifnull positive value"
    );
    failures += expect_operator(
        child_at(third_expression, 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "ifnull negative fallback"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "ifnull from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT IFNULL (NULL,10), (IFNULL(NULL,10)), IFNULL(IFNULL(NULL,1),2), "
        "IFNULL(IF(0,NULL,4),5);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    nested = child_at(third_expression, 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IFNULL_FUNCTION, "spaced ifnull");
    failures += expect_span_text(first_expression, "IFNULL (NULL,10)", "spaced ifnull span");
    failures +=
        expect_node(parenthesized, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "parenthesized ifnull");
    failures +=
        expect_node(child_at(parenthesized, 0U), MYLITE_SQL_AST_IFNULL_FUNCTION, "wrapped ifnull");
    failures += expect_span_text(parenthesized, "(IFNULL(NULL,10))", "parenthesized ifnull span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_IFNULL_FUNCTION, "outer ifnull");
    failures += expect_node(nested, MYLITE_SQL_AST_IFNULL_FUNCTION, "inner ifnull");
    failures += expect_node(
        child_at(child_at(child_at(select_list, 3U), 0U), 0U),
        MYLITE_SQL_AST_IF_FUNCTION,
        "nested if in ifnull"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT IFNULL(1, 2 + 3);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IFNULL_FUNCTION, "deferred ifnull");
    failures += expect_node(
        child_at(first_expression, 1U),
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "deferred ifnull arithmetic fallback"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT IFNULL();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR,
        "empty ifnull argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT IFNULL(1);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR,
        "one ifnull argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT IFNULL(1,2,3);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR,
        "three ifnull argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_coalesce_function(void) {
    enum {
        coalesce_nested_ifnull_item_index = 4,
        coalesce_nested_if_item_index = 5,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    const struct mylite_sql_ast_node *nested = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT COALESCE(1), Coalesce(TRUE,NULL), coalesce(+0,-1,NULL) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_COALESCE_FUNCTION, "coalesce function");
    failures += expect_span_text(first_expression, "COALESCE(1)", "coalesce function span");
    failures += expect_child_count(first_expression, 1U, "coalesce function child count");
    arguments = child_at(first_expression, 0U);
    failures += expect_node(arguments, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, "coalesce args");
    failures += expect_child_count(arguments, 1U, "coalesce one argument");
    failures +=
        expect_literal(child_at(arguments, 0U), MYLITE_SQL_AST_LITERAL_INTEGER, "coalesce value");
    failures += expect_node(second_expression, MYLITE_SQL_AST_COALESCE_FUNCTION, "mixed coalesce");
    arguments = child_at(second_expression, 0U);
    failures += expect_child_count(arguments, 2U, "coalesce two arguments");
    failures +=
        expect_literal(child_at(arguments, 0U), MYLITE_SQL_AST_LITERAL_TRUE, "coalesce TRUE");
    failures +=
        expect_literal(child_at(arguments, 1U), MYLITE_SQL_AST_LITERAL_NULL, "coalesce NULL");
    failures += expect_node(third_expression, MYLITE_SQL_AST_COALESCE_FUNCTION, "lower coalesce");
    arguments = child_at(third_expression, 0U);
    failures += expect_child_count(arguments, 3U, "coalesce three arguments");
    failures += expect_operator(
        child_at(arguments, 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "coalesce positive value"
    );
    failures += expect_operator(
        child_at(arguments, 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "coalesce negative fallback"
    );
    failures += expect_literal(
        child_at(arguments, 2U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "coalesce null fallback"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "coalesce from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT COALESCE (NULL,10), (COALESCE(NULL,10)), "
        "COALESCE(NULL, COALESCE(NULL,1), 2), COALESCE(IF(0,NULL,4),5), "
        "COALESCE(NULL, IFNULL(COALESCE(NULL,NULL),9)), "
        "COALESCE(NULL, IF(COALESCE(NULL,0),1,2));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    arguments = child_at(third_expression, 0U);
    nested = child_at(arguments, 1U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_COALESCE_FUNCTION, "spaced coalesce");
    failures += expect_span_text(first_expression, "COALESCE (NULL,10)", "spaced coalesce span");
    failures += expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized coalesce"
    );
    failures += expect_node(
        child_at(parenthesized, 0U),
        MYLITE_SQL_AST_COALESCE_FUNCTION,
        "wrapped coalesce"
    );
    failures +=
        expect_span_text(parenthesized, "(COALESCE(NULL,10))", "parenthesized coalesce span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_COALESCE_FUNCTION, "outer coalesce");
    failures += expect_node(nested, MYLITE_SQL_AST_COALESCE_FUNCTION, "inner coalesce");
    failures += expect_node(
        child_at(child_at(child_at(child_at(select_list, 3U), 0U), 0U), 0U),
        MYLITE_SQL_AST_IF_FUNCTION,
        "nested if in coalesce"
    );
    failures += expect_node(
        child_at(
            child_at(child_at(child_at(select_list, coalesce_nested_ifnull_item_index), 0U), 0U),
            1U
        ),
        MYLITE_SQL_AST_IFNULL_FUNCTION,
        "nested ifnull in coalesce"
    );
    failures += expect_node(
        child_at(
            child_at(child_at(child_at(select_list, coalesce_nested_if_item_index), 0U), 0U),
            1U
        ),
        MYLITE_SQL_AST_IF_FUNCTION,
        "nested if in coalesce"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT COALESCE(1, 2 + 3, 'x');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_COALESCE_FUNCTION, "deferred coalesce");
    arguments = child_at(first_expression, 0U);
    failures += expect_node(
        child_at(arguments, 1U),
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "deferred coalesce arithmetic argument"
    );
    failures += expect_literal(
        child_at(arguments, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "deferred coalesce string argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT COALESCE();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT COALESCE(NULL, COALESCE());", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COALESCE(1,,2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_concat_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT CONCAT('a', 'b'), concat(v, '-', id) AS label FROM t ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_CONCAT_FUNCTION, "concat function");
    failures += expect_span_text(first_expression, "CONCAT('a', 'b')", "concat function span");
    arguments = child_at(first_expression, 0U);
    failures += expect_node(arguments, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, "concat args");
    failures += expect_child_count(arguments, 2U, "concat two arguments");
    failures +=
        expect_literal(child_at(arguments, 0U), MYLITE_SQL_AST_LITERAL_STRING, "concat first");
    failures +=
        expect_literal(child_at(arguments, 1U), MYLITE_SQL_AST_LITERAL_STRING, "concat second");
    failures += expect_node(second_expression, MYLITE_SQL_AST_CONCAT_FUNCTION, "lower concat");
    arguments = child_at(second_expression, 0U);
    failures += expect_child_count(arguments, 3U, "concat row arguments");
    failures += expect_node(child_at(arguments, 0U), MYLITE_SQL_AST_IDENTIFIER, "concat column");
    failures += expect_node(
        child_at(child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "concat alias"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "concat from table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CONCAT();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CONCAT_ARGUMENT_COUNT_ERROR,
        "concat zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT concat FROM t;", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "concat identifier");
    failures += expect_span_text(first_expression, "concat", "concat identifier span");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_concat_ws_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT CONCAT_WS('-', 'a', 'b'), concat_ws('-', v, id) AS label "
        "FROM t ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_CONCAT_WS_FUNCTION, "concat_ws function");
    failures += expect_span_text(first_expression, "CONCAT_WS('-', 'a', 'b')", "concat_ws span");
    arguments = child_at(first_expression, 0U);
    failures += expect_node(arguments, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, "concat_ws args");
    failures += expect_child_count(arguments, 3U, "concat_ws three arguments");
    failures += expect_literal(
        child_at(arguments, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "concat_ws separator"
    );
    failures +=
        expect_literal(child_at(arguments, 1U), MYLITE_SQL_AST_LITERAL_STRING, "concat_ws first");
    failures +=
        expect_literal(child_at(arguments, 2U), MYLITE_SQL_AST_LITERAL_STRING, "concat_ws second");
    failures +=
        expect_node(second_expression, MYLITE_SQL_AST_CONCAT_WS_FUNCTION, "lower concat_ws");
    arguments = child_at(second_expression, 0U);
    failures += expect_child_count(arguments, 3U, "concat_ws row arguments");
    failures += expect_node(child_at(arguments, 1U), MYLITE_SQL_AST_IDENTIFIER, "concat_ws column");
    failures += expect_node(
        child_at(child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "concat_ws alias"
    );
    failures +=
        expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "concat_ws from table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CONCAT_WS();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CONCAT_WS_ARGUMENT_COUNT_ERROR,
        "concat_ws zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CONCAT_WS(',');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CONCAT_WS_FUNCTION,
        "concat_ws one argument function"
    );
    arguments = child_at(first_expression, 0U);
    failures += expect_child_count(arguments, 1U, "concat_ws one argument");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT concat_ws FROM t;", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "concat_ws identifier");
    failures += expect_span_text(first_expression, "concat_ws", "concat_ws identifier span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DO CONCAT_WS('-', 'a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    first_expression = child_at(expression_list, 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_CONCAT_WS_FUNCTION, "do concat_ws");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT CONCAT_WS (',', 'a', 'b') FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_CONCAT_WS_FUNCTION, "concat_ws whitespace");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_replace_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT REPLACE('abcabc', 'a', 'x'), replace(v, 'a', 'x') AS label "
        "FROM t ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_REPLACE_FUNCTION, "replace function");
    failures += expect_span_text(first_expression, "REPLACE('abcabc', 'a', 'x')", "replace span");
    failures += expect_child_count(first_expression, 3U, "replace three children");
    failures += expect_literal(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "replace str"
    );
    failures += expect_literal(
        child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "replace search"
    );
    failures += expect_literal(
        child_at(first_expression, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "replace replacement"
    );
    failures += expect_node(second_expression, MYLITE_SQL_AST_REPLACE_FUNCTION, "lower replace");
    failures +=
        expect_node(child_at(second_expression, 0U), MYLITE_SQL_AST_IDENTIFIER, "replace column");
    failures += expect_node(
        child_at(child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "replace alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DO REPLACE('abc', 'b', 'B');", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    first_expression = child_at(expression_list, 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_REPLACE_FUNCTION, "do replace");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT REPLACE ('abc', 'a', 'A') FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_REPLACE_FUNCTION, "replace whitespace");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT REPLACE();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    failures += parse_sql("SELECT REPLACE(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    failures += parse_sql("SELECT REPLACE(1, 2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    failures += parse_sql("SELECT REPLACE(1, 2, 3, 4);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    failures += parse_sql("SELECT REPLACE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);

    return failures;
}

static int test_field_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT FIELD('b', 'a', 'b'), field(v, 'x', 'y') AS pos "
        "FROM t ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_FIELD_FUNCTION, "field function");
    failures += expect_span_text(first_expression, "FIELD('b', 'a', 'b')", "field span");
    arguments = child_at(first_expression, 0U);
    failures += expect_node(arguments, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, "field args");
    failures += expect_child_count(arguments, 3U, "field three arguments");
    failures +=
        expect_literal(child_at(arguments, 0U), MYLITE_SQL_AST_LITERAL_STRING, "field search");
    failures +=
        expect_literal(child_at(arguments, 1U), MYLITE_SQL_AST_LITERAL_STRING, "field first");
    failures +=
        expect_literal(child_at(arguments, 2U), MYLITE_SQL_AST_LITERAL_STRING, "field second");
    failures += expect_node(second_expression, MYLITE_SQL_AST_FIELD_FUNCTION, "lower field");
    arguments = child_at(second_expression, 0U);
    failures += expect_child_count(arguments, 3U, "field row arguments");
    failures += expect_node(child_at(arguments, 0U), MYLITE_SQL_AST_IDENTIFIER, "field column");
    failures += expect_node(
        child_at(child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "field alias"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "field from table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT FIELD();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_FIELD_ARGUMENT_COUNT_ERROR,
        "field zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT FIELD('x');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_FIELD_FUNCTION,
        "field one argument runtime marker"
    );
    arguments = child_at(first_expression, 0U);
    failures += expect_child_count(arguments, 1U, "field one argument count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DO FIELD('x', 'a'), FIELD(NULL, NULL);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "field do");
    failures +=
        expect_node(child_at(expression_list, 0U), MYLITE_SQL_AST_FIELD_FUNCTION, "do field");
    failures +=
        expect_node(child_at(expression_list, 1U), MYLITE_SQL_AST_FIELD_FUNCTION, "do field null");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE field (field INT); SELECT field FROM field;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT FIELD('b', 'a', 'b') FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_json_valid_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT JSON_VALID('{\"a\":1}'), json_valid(payload) AS ok "
        "FROM t WHERE JSON_VALID(payload) ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_JSON_VALID_FUNCTION, "json_valid function");
    failures += expect_span_text(first_expression, "JSON_VALID('{\"a\":1}')", "json_valid span");
    failures += expect_child_count(first_expression, 1U, "json_valid argument count");
    failures += expect_literal(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "json_valid string argument"
    );
    failures +=
        expect_node(second_expression, MYLITE_SQL_AST_JSON_VALID_FUNCTION, "lower json_valid");
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "json_valid column"
    );
    failures += expect_node(
        child_at(child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "json_valid alias"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "json_valid table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT JSON_VALID();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_VALID_ARGUMENT_COUNT_ERROR,
        "json_valid zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT JSON_VALID('{}', '{}');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_VALID_ARGUMENT_COUNT_ERROR,
        "json_valid two argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DO JSON_VALID('{}'), JSON_VALID(NULL);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "json_valid do");
    failures += expect_node(
        child_at(expression_list, 0U),
        MYLITE_SQL_AST_JSON_VALID_FUNCTION,
        "do json_valid"
    );
    failures += expect_node(
        child_at(expression_list, 1U),
        MYLITE_SQL_AST_JSON_VALID_FUNCTION,
        "do json_valid null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE json_valid (json_valid INT); SELECT json_valid FROM json_valid;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT JSON_VALID('{\"a\":1}') FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_json_extract_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_item = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *operator_expression = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT JSON_EXTRACT('{\"a\":1}', '$.a'), JSON_UNQUOTE('\"x\"') AS value FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_item = child_at(select_list, 1U);
    second_expression = child_at(second_item, 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION,
        "json_extract function"
    );
    failures +=
        expect_span_text(first_expression, "JSON_EXTRACT('{\"a\":1}', '$.a')", "json_extract span");
    failures += expect_child_count(first_expression, 2U, "json_extract argument count");
    failures += expect_literal(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "json_extract document"
    );
    failures += expect_literal(
        child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "json_extract path"
    );
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION,
        "json_unquote function"
    );
    failures += expect_span_text(second_expression, "JSON_UNQUOTE('\"x\"')", "json_unquote span");
    failures += expect_child_count(second_expression, 1U, "json_unquote argument count");
    failures +=
        expect_node(child_at(second_item, 1U), MYLITE_SQL_AST_IDENTIFIER, "json_unquote alias");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "json from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT j->'$.a', t.s->>'$.b' FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    operator_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(operator_expression, MYLITE_SQL_AST_BINARY_EXPRESSION, "json arrow");
    failures += expect_operator(
        operator_expression,
        MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT,
        "json arrow operator"
    );
    failures += expect_span_text(child_at(operator_expression, 0U), "j", "json arrow column");
    failures += expect_span_text(child_at(operator_expression, 1U), "'$.a'", "json arrow path");

    operator_expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(operator_expression, MYLITE_SQL_AST_BINARY_EXPRESSION, "json unquote arrow");
    failures += expect_operator(
        operator_expression,
        MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT,
        "json unquote arrow operator"
    );
    failures +=
        expect_span_text(child_at(operator_expression, 0U), "t.s", "json unquote arrow column");
    failures +=
        expect_span_text(child_at(operator_expression, 1U), "'$.b'", "json unquote arrow path");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT JSON_EXTRACT();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR,
        "json_extract zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT JSON_EXTRACT('{\"a\":1}');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR,
        "json_extract one argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT JSON_EXTRACT('{}', '$', '$.b');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION,
        "json_extract multipath function"
    );
    failures += expect_child_count(first_expression, 1U, "json_extract multipath child count");
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST,
        "json_extract multipath list"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT JSON_UNQUOTE();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_UNQUOTE_ARGUMENT_COUNT_ERROR,
        "json_unquote zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT JSON_UNQUOTE('\"a\"', '\"b\"');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_UNQUOTE_ARGUMENT_COUNT_ERROR,
        "json_unquote many argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE json_extract (json_unquote INT); SELECT json_unquote FROM json_extract;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT '{\"a\":1}'->'$.a';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_json_construction_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT JSON_ARRAY(), JSON_ARRAY(1, 'x', NULL, TRUE), "
        "JSON_OBJECT(), JSON_OBJECT('a', 1, 'b', NULL) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_ARRAY_FUNCTION,
        "json_array empty function"
    );
    failures += expect_child_count(first_expression, 0U, "json_array empty argument count");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_JSON_ARRAY_FUNCTION,
        "json_array populated function"
    );
    failures += expect_child_count(second_expression, 1U, "json_array argument-list child count");
    failures += expect_child_count(
        child_at(second_expression, 0U),
        4U,
        "json_array populated argument count"
    );

    first_expression = child_at(child_at(select_list, 2U), 0U);
    second_expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_OBJECT_FUNCTION,
        "json_object empty function"
    );
    failures += expect_child_count(first_expression, 0U, "json_object empty argument count");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_JSON_OBJECT_FUNCTION,
        "json_object populated function"
    );
    failures += expect_child_count(second_expression, 1U, "json_object argument-list child count");
    failures += expect_child_count(
        child_at(second_expression, 0U),
        4U,
        "json_object populated argument count"
    );
    failures +=
        expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "json constructors dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DO JSON_ARRAY(1), JSON_OBJECT('a', 1);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "json constructors do");
    failures += expect_node(
        child_at(expression_list, 0U),
        MYLITE_SQL_AST_JSON_ARRAY_FUNCTION,
        "do json_array"
    );
    failures += expect_node(
        child_at(expression_list, 1U),
        MYLITE_SQL_AST_JSON_OBJECT_FUNCTION,
        "do json_object"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE json_array (json_object INT); SELECT json_object FROM json_array;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT JSON_ARRAY(*);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_json_introspection_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT JSON_TYPE('{\"a\":1}'), JSON_LENGTH('{\"a\":[1,2]}'), "
        "JSON_LENGTH(j, '$.a') FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_JSON_TYPE_FUNCTION, "json_type function");
    failures += expect_span_text(first_expression, "JSON_TYPE('{\"a\":1}')", "json_type span");
    failures += expect_child_count(first_expression, 1U, "json_type argument count");
    failures += expect_literal(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "json_type document"
    );
    failures +=
        expect_node(second_expression, MYLITE_SQL_AST_JSON_LENGTH_FUNCTION, "json_length function");
    failures +=
        expect_span_text(second_expression, "JSON_LENGTH('{\"a\":[1,2]}')", "json_length span");
    failures += expect_child_count(second_expression, 1U, "json_length argument count");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_JSON_LENGTH_FUNCTION,
        "json_length path function"
    );
    failures += expect_child_count(third_expression, 2U, "json_length path argument count");
    failures += expect_node(child_at(third_expression, 0U), MYLITE_SQL_AST_IDENTIFIER, "json doc");
    failures +=
        expect_literal(child_at(third_expression, 1U), MYLITE_SQL_AST_LITERAL_STRING, "json path");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT JSON_TYPE(JSON_EXTRACT('{\"a\":[1]}', '$.a')), "
        "JSON_LENGTH(JSON_EXTRACT('{\"a\":[1]}', '$.a'));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION,
        "json_type nested json_extract"
    );
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION,
        "json_length nested json_extract"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("DO JSON_TYPE('{\"a\":1}'), JSON_LENGTH('[1,2]');", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "json introspection do");
    failures += expect_node(
        child_at(expression_list, 0U),
        MYLITE_SQL_AST_JSON_TYPE_FUNCTION,
        "do json_type"
    );
    failures += expect_node(
        child_at(expression_list, 1U),
        MYLITE_SQL_AST_JSON_LENGTH_FUNCTION,
        "do json_length"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT JSON_TYPE();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR,
        "json_type zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT JSON_TYPE('{}', '$');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR,
        "json_type many argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT JSON_LENGTH();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR,
        "json_length zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT JSON_LENGTH('{}', '$', '$.a');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR,
        "json_length many argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE json_type (json_length INT); SELECT json_length FROM json_type;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
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
    int failures = 0;

    failures += parse_sql(
        "SELECT CAST('ABC' AS BINARY) AS binary, CAST(+12 AS BINARY) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_item = child_at(select_list, 0U);
    first_expression = child_at(first_item, 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CAST_BINARY_EXPRESSION,
        "cast binary expression"
    );
    failures += expect_span_text(first_expression, "CAST('ABC' AS BINARY)", "cast binary span");
    failures += expect_literal(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "cast binary string input"
    );
    failures += expect_node(child_at(first_item, 1U), MYLITE_SQL_AST_IDENTIFIER, "binary alias");
    failures += expect_span_text(child_at(first_item, 1U), "binary", "binary alias span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_CAST_BINARY_EXPRESSION,
        "cast binary signed expression"
    );
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_UNARY_EXPRESSION,
        "cast binary signed input"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "cast from dual");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT CAST('ABC' AS BINARY(5));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT CAST('ABC' AS CHAR), CAST('1' AS SIGNED), "
        "CAST('1' AS SIGNED INTEGER), CAST('1' AS SIGNED INT), "
        "CAST('1' AS UNSIGNED), CAST('1' AS UNSIGNED INTEGER), "
        "CAST('1' AS UNSIGNED INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    failures += expect_node(
        child_at(child_at(select_list, cast_basic_char_item), 0U),
        MYLITE_SQL_AST_CAST_CHAR_EXPRESSION,
        "cast char expression"
    );
    failures += expect_node(
        child_at(child_at(select_list, cast_basic_signed_item), 0U),
        MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION,
        "cast signed expression"
    );
    failures += expect_node(
        child_at(child_at(select_list, cast_basic_signed_integer_item), 0U),
        MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION,
        "cast signed integer expression"
    );
    failures += expect_node(
        child_at(child_at(select_list, cast_basic_signed_int_item), 0U),
        MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION,
        "cast signed int expression"
    );
    failures += expect_node(
        child_at(child_at(select_list, cast_basic_unsigned_item), 0U),
        MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION,
        "cast unsigned expression"
    );
    failures += expect_node(
        child_at(child_at(select_list, cast_basic_unsigned_integer_item), 0U),
        MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION,
        "cast unsigned integer expression"
    );
    failures += expect_node(
        child_at(child_at(select_list, cast_basic_unsigned_int_item), 0U),
        MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION,
        "cast unsigned int expression"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CAST('ABC' AS CHAR(5));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CAST('1' AS INT);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT CONVERT('ABC' USING BINARY) AS binary, (CONVERT('x' USING BINARY)), "
        "CONVERT(123 USING BINARY) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_item = child_at(select_list, 0U);
    first_expression = child_at(first_item, 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION,
        "convert using binary expression"
    );
    failures += expect_span_text(
        first_expression,
        "CONVERT('ABC' USING BINARY)",
        "convert using binary span"
    );
    failures += expect_literal(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "convert using binary string input"
    );
    failures += expect_node(child_at(first_item, 1U), MYLITE_SQL_AST_IDENTIFIER, "convert alias");
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION,
        "parenthesized convert using binary expression"
    );
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION,
        "convert using binary runtime-deferred operand"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "convert from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT CONVERT('ABC', BINARY) AS binary, (CONVERT('x', BINARY)), "
        "CONVERT(+123 USING utf8mb4) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_item = child_at(select_list, 0U);
    first_expression = child_at(first_item, 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION,
        "convert binary type expression"
    );
    failures +=
        expect_span_text(first_expression, "CONVERT('ABC', BINARY)", "convert binary type span");
    failures += expect_literal(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "convert binary type string input"
    );
    failures += expect_node(child_at(first_item, 1U), MYLITE_SQL_AST_IDENTIFIER, "binary alias");
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION,
        "parenthesized convert binary type expression"
    );
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION,
        "convert using charset expression"
    );
    failures += expect_node(
        child_at(third_expression, 0U),
        MYLITE_SQL_AST_UNARY_EXPRESSION,
        "convert using charset signed input"
    );
    failures += expect_node(
        child_at(third_expression, 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "convert using charset identifier"
    );
    failures += expect_span_text(
        child_at(third_expression, 1U),
        "utf8mb4",
        "convert using charset identifier span"
    );
    failures += expect_node(
        child_at(select, 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "convert syntax expansion from dual"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DO CONVERT(NULL, BINARY), CONVERT(FALSE USING utf8mb4);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "convert syntax do statement");
    failures += expect_node(
        child_at(expression_list, 0U),
        MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION,
        "convert binary type do expression"
    );
    failures += expect_node(
        child_at(expression_list, 1U),
        MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION,
        "convert using charset do expression"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT CONVERT('ABC', BINARY(5));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT CONVERT('ABC', CHAR), CONVERT('1', SIGNED), "
        "CONVERT('1', SIGNED INTEGER), CONVERT('1', SIGNED INT), "
        "CONVERT('1', UNSIGNED), CONVERT('1', UNSIGNED INTEGER), "
        "CONVERT('1', UNSIGNED INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    failures += expect_node(
        child_at(child_at(select_list, convert_basic_char_item), 0U),
        MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION,
        "convert char type expression"
    );
    failures += expect_node(
        child_at(child_at(select_list, convert_basic_signed_item), 0U),
        MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION,
        "convert signed type expression"
    );
    failures += expect_node(
        child_at(child_at(select_list, convert_basic_signed_integer_item), 0U),
        MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION,
        "convert signed integer type expression"
    );
    failures += expect_node(
        child_at(child_at(select_list, convert_basic_signed_int_item), 0U),
        MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION,
        "convert signed int type expression"
    );
    failures += expect_node(
        child_at(child_at(select_list, convert_basic_unsigned_item), 0U),
        MYLITE_SQL_AST_CONVERT_UNSIGNED_TYPE_EXPRESSION,
        "convert unsigned type expression"
    );
    failures += expect_node(
        child_at(child_at(select_list, convert_basic_unsigned_integer_item), 0U),
        MYLITE_SQL_AST_CONVERT_UNSIGNED_TYPE_EXPRESSION,
        "convert unsigned integer type expression"
    );
    failures += expect_node(
        child_at(child_at(select_list, convert_basic_unsigned_int_item), 0U),
        MYLITE_SQL_AST_CONVERT_UNSIGNED_TYPE_EXPRESSION,
        "convert unsigned int type expression"
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT CONVERT('ABC', CHAR(5));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CONVERT('1', INTEGER);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT CONVERT('ABC', BINARY, 1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("CREATE TABLE t (cast INT); SELECT cast FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("CREATE TABLE t (binary INT);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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

    failures += parse_sql(
        "SELECT DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND), "
        "Date_Add('2008-01-02', INTERVAL -1 SECOND) AS shifted FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_DATE_ADD_FUNCTION, "date_add function");
    failures += expect_span_text(
        first_expression,
        "DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
        "date_add span"
    );
    failures += expect_child_count(first_expression, 2U, "date_add child count");
    failures += expect_literal(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "date_add date argument"
    );
    failures += expect_literal(
        child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "date_add interval argument"
    );
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_DATE_ADD_FUNCTION,
        "mixed-case date_add function"
    );
    failures += expect_node(
        child_at(second_expression, 1U),
        MYLITE_SQL_AST_UNARY_EXPRESSION,
        "signed date_add interval argument"
    );
    failures += expect_node(child_at(statement, 1U), MYLITE_SQL_AST_FROM_DUAL, "date_add dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 SECOND), "
        "ADDDATE('2008-01-02', INTERVAL +1 SECOND) AS adddate_alias, "
        "SUBDATE('2008-01-02', INTERVAL -1 SECOND) AS subdate_alias FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_DATE_SUB_FUNCTION, "date_sub function");
    failures += expect_span_text(
        first_expression,
        "DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
        "date_sub span"
    );
    failures += expect_child_count(first_expression, 2U, "date_sub child count");
    failures += expect_node(second_expression, MYLITE_SQL_AST_ADDDATE_FUNCTION, "adddate function");
    failures += expect_node(
        child_at(second_expression, 1U),
        MYLITE_SQL_AST_UNARY_EXPRESSION,
        "signed adddate interval argument"
    );
    failures += expect_node(third_expression, MYLITE_SQL_AST_SUBDATE_FUNCTION, "subdate function");
    failures += expect_node(
        child_at(third_expression, 1U),
        MYLITE_SQL_AST_UNARY_EXPRESSION,
        "signed subdate interval argument"
    );
    failures +=
        expect_node(child_at(statement, 1U), MYLITE_SQL_AST_FROM_DUAL, "date_sub aliases dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DO DATE_ADD(NULL, INTERVAL +0 SECOND), DATE_SUB(NULL, INTERVAL +0 SECOND), "
        "ADDDATE('2024-02-29', INTERVAL NULL SECOND), "
        "SUBDATE('2024-02-29', INTERVAL NULL SECOND);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "date_add do");
    failures += expect_node(
        child_at(expression_list, 0U),
        MYLITE_SQL_AST_DATE_ADD_FUNCTION,
        "do date_add function"
    );
    failures += expect_node(
        child_at(expression_list, 1U),
        MYLITE_SQL_AST_DATE_SUB_FUNCTION,
        "do date_sub function"
    );
    failures += expect_node(
        child_at(expression_list, 2U),
        MYLITE_SQL_AST_ADDDATE_FUNCTION,
        "do adddate null interval"
    );
    failures += expect_node(
        child_at(expression_list, 3U),
        MYLITE_SQL_AST_SUBDATE_FUNCTION,
        "do subdate null interval"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE date_add_keywords (date_add INT, second INT); "
        "SELECT date_add, second FROM date_add_keywords;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("CREATE TABLE date_add(id INT);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("CREATE TABLE date_add (id INT);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("CREATE TABLE date_sub(id INT);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("CREATE TABLE date_sub (id INT);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("CREATE TABLE adddate(id INT);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("CREATE TABLE subdate(id INT);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT DATE_ADD ('2008-01-02', INTERVAL 1 SECOND);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT DATE_SUB ('2008-01-02', INTERVAL 1 SECOND);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT ADDDATE ('2008-01-02', INTERVAL 1 SECOND);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT SUBDATE ('2008-01-02', INTERVAL 1 SECOND);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql_with_ignore_space(
        "SELECT DATE_ADD ('2008-01-02', INTERVAL 1 SECOND);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql_with_ignore_space(
        "SELECT DATE_SUB ('2008-01-02', INTERVAL 1 SECOND), "
        "ADDDATE ('2008-01-02', INTERVAL 1 SECOND), "
        "SUBDATE ('2008-01-02', INTERVAL 1 SECOND);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql_with_ignore_space(
        "CREATE TABLE date_add (id INT);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql_with_ignore_space(
        "CREATE TABLE `date_add` (id INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql_with_ignore_space(
        "CREATE TABLE date_sub (id INT);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql_with_ignore_space(
        "CREATE TABLE `date_sub` (id INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql_with_ignore_space("CREATE TABLE adddate(id INT);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql_with_ignore_space("CREATE TABLE subdate(id INT);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql_with_ignore_space("CREATE TABLE t (second INT);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT DATE_ADD('2008-01-02', INTERVAL 1 MINUTE);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("CREATE TABLE interval (interval INT);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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

    failures += parse_sql(
        "SELECT DATE_FORMAT('2008-01-02 13:29:17', '%Y'), "
        "Date_Format(option_value, '%H.%i') = 0.42 AS matched FROM options;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_DATE_FORMAT_FUNCTION, "date_format function");
    failures += expect_span_text(
        first_expression,
        "DATE_FORMAT('2008-01-02 13:29:17', '%Y')",
        "date_format span"
    );
    failures += expect_child_count(first_expression, 2U, "date_format child count");
    failures += expect_literal(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "date_format value argument"
    );
    failures += expect_literal(
        child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "date_format format argument"
    );
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "date_format numeric comparison"
    );
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_DATE_FORMAT_FUNCTION,
        "date_format comparison left"
    );
    failures += expect_node(
        child_at(child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "date_format alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DO DATE_FORMAT(NULL, '%Y'), DATE_FORMAT('2008-01-02', NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "date_format do");
    failures += expect_node(
        child_at(expression_list, 0U),
        MYLITE_SQL_AST_DATE_FORMAT_FUNCTION,
        "do date_format null value"
    );
    failures += expect_node(
        child_at(expression_list, 1U),
        MYLITE_SQL_AST_DATE_FORMAT_FUNCTION,
        "do date_format null format"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DATE_FORMAT();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_DATE_FORMAT_ARGUMENT_COUNT_ERROR,
        "date_format zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DATE_FORMAT('2008-01-02');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_DATE_FORMAT_ARGUMENT_COUNT_ERROR,
        "date_format one argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT DATE_FORMAT('2008-01-02', '%Y', 'extra');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_DATE_FORMAT_ARGUMENT_COUNT_ERROR,
        "date_format extra argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT DATE_FORMAT ('2008-01-02', '%Y') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE date_format (date_format INT); SELECT date_format FROM date_format;",
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

    failures += parse_sql(
        "SELECT DATEDIFF('2008-01-02', '2008-01-01'), "
        "DateDiff(option_value, created_at) AS days FROM options;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_DATEDIFF_FUNCTION, "datediff");
    failures +=
        expect_span_text(first_expression, "DATEDIFF('2008-01-02', '2008-01-01')", "datediff span");
    failures += expect_child_count(first_expression, 2U, "datediff child count");
    failures +=
        expect_literal(child_at(first_expression, 0U), MYLITE_SQL_AST_LITERAL_STRING, "left");
    failures +=
        expect_literal(child_at(first_expression, 1U), MYLITE_SQL_AST_LITERAL_STRING, "right");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_DATEDIFF_FUNCTION,
        "datediff column function"
    );
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "datediff column left"
    );
    failures += expect_node(
        child_at(second_expression, 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "datediff column right"
    );
    failures += expect_node(
        child_at(child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "datediff alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DO DATEDIFF(NULL, '2008-01-01');", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "datediff do");
    failures +=
        expect_node(child_at(expression_list, 0U), MYLITE_SQL_AST_DATEDIFF_FUNCTION, "do datediff");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DATEDIFF();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR,
        "datediff zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DATEDIFF('2008-01-02');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR,
        "datediff one argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT DATEDIFF('2008-01-03', '2008-01-02', '2008-01-01');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR,
        "datediff extra argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT DATEDIFF ('2008-01-02', '2008-01-01') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE datediff (datediff INT); SELECT datediff FROM datediff;",
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

    failures += parse_sql(
        "SELECT UNIX_TIMESTAMP(), Unix_Timestamp('1970-01-01 00:00:01') AS ts, "
        "UNIX_TIMESTAMP(d) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION,
        "unix_timestamp no-argument function"
    );
    failures += expect_child_count(first_expression, 0U, "unix_timestamp no-argument count");
    failures +=
        expect_span_text(first_expression, "UNIX_TIMESTAMP()", "unix_timestamp no-argument span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION,
        "unix_timestamp literal function"
    );
    failures += expect_child_count(second_expression, 1U, "unix_timestamp literal count");
    failures += expect_literal(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "unix_timestamp literal argument"
    );
    failures += expect_node(
        child_at(child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "unix_timestamp alias"
    );
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION,
        "unix_timestamp column function"
    );
    failures += expect_node(
        child_at(third_expression, 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "unix_timestamp column argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("DO UNIX_TIMESTAMP(), UNIX_TIMESTAMP(NULL);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "unix_timestamp do");
    failures += expect_node(
        child_at(expression_list, 0U),
        MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION,
        "do unix_timestamp no-argument"
    );
    failures += expect_node(
        child_at(expression_list, 1U),
        MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION,
        "do unix_timestamp null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT UNIX_TIMESTAMP('1970-01-01', 'extra');", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_UNIX_TIMESTAMP_ARGUMENT_COUNT_ERROR,
        "unix_timestamp extra argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT UNIX_TIMESTAMP ('1970-01-01 00:00:01') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unix_timestamp (unix_timestamp INT); "
        "SELECT unix_timestamp FROM unix_timestamp;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_temporal_extract_functions(void) {
    enum {
        temporal_extract_dayofweek_projection_index = 5,
        temporal_extract_dayofyear_projection_index = 6,
        temporal_extract_last_day_projection_index = 7,
        temporal_extract_time_projection_index = 8,
        temporal_extract_hour_projection_index = 9,
        temporal_extract_minute_projection_index = 10,
        temporal_extract_second_projection_index = 11,
        temporal_extract_do_hour_index = 6
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT DATE('2008-01-02 13:29:17'), YEAR(d), MONTH(d), DAY(d), "
        "DAYOFMONTH(d), DAYOFWEEK(d), DAYOFYEAR(d), LAST_DAY(d), TIME(dt), HOUR(tm), "
        "MINUTE(tm), SECOND(tm) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    failures += expect_node(
        child_at(child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_DATE_FUNCTION,
        "date extract function"
    );
    failures += expect_node(
        child_at(child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_YEAR_FUNCTION,
        "year extract function"
    );
    failures += expect_node(
        child_at(child_at(select_list, 2U), 0U),
        MYLITE_SQL_AST_MONTH_FUNCTION,
        "month extract function"
    );
    failures += expect_node(
        child_at(child_at(select_list, 3U), 0U),
        MYLITE_SQL_AST_DAY_FUNCTION,
        "day extract function"
    );
    failures += expect_node(
        child_at(child_at(select_list, 4U), 0U),
        MYLITE_SQL_AST_DAYOFMONTH_FUNCTION,
        "dayofmonth extract function"
    );
    failures += expect_node(
        child_at(child_at(select_list, temporal_extract_dayofweek_projection_index), 0U),
        MYLITE_SQL_AST_DAYOFWEEK_FUNCTION,
        "dayofweek extract function"
    );
    failures += expect_node(
        child_at(child_at(select_list, temporal_extract_dayofyear_projection_index), 0U),
        MYLITE_SQL_AST_DAYOFYEAR_FUNCTION,
        "dayofyear extract function"
    );
    failures += expect_node(
        child_at(child_at(select_list, temporal_extract_last_day_projection_index), 0U),
        MYLITE_SQL_AST_LAST_DAY_FUNCTION,
        "last_day extract function"
    );
    failures += expect_node(
        child_at(child_at(select_list, temporal_extract_time_projection_index), 0U),
        MYLITE_SQL_AST_TIME_FUNCTION,
        "time extract function"
    );
    failures += expect_node(
        child_at(child_at(select_list, temporal_extract_hour_projection_index), 0U),
        MYLITE_SQL_AST_HOUR_FUNCTION,
        "hour extract function"
    );
    failures += expect_node(
        child_at(child_at(select_list, temporal_extract_minute_projection_index), 0U),
        MYLITE_SQL_AST_MINUTE_FUNCTION,
        "minute extract function"
    );
    expression = child_at(child_at(select_list, temporal_extract_second_projection_index), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_SECOND_FUNCTION, "second extract function");
    failures += expect_child_count(expression, 1U, "second extract child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT DATE ('2008-01-02 13:29:17'), DAYOFWEEK ('2008-01-02') AS dow, "
        "DAYOFYEAR ('2008-01-02') AS doy, LAST_DAY ('2008-01-02') AS month_end, "
        "TIME ('13:29:17') AS tm, HOUR ('13:29:17') AS h FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DO DATE(NULL), DAYOFWEEK('2008-01-02'), DAYOFYEAR('2008-01-02'), "
        "LAST_DAY('2008-01-02'), TIME('13:29:17'), YEAR('2008-01-02'), HOUR('13:29:17');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "temporal extract do");
    failures +=
        expect_node(child_at(expression_list, 0U), MYLITE_SQL_AST_DATE_FUNCTION, "do date extract");
    failures += expect_node(
        child_at(expression_list, 1U),
        MYLITE_SQL_AST_DAYOFWEEK_FUNCTION,
        "do dayofweek extract"
    );
    failures += expect_node(
        child_at(expression_list, 2U),
        MYLITE_SQL_AST_DAYOFYEAR_FUNCTION,
        "do dayofyear extract"
    );
    failures += expect_node(
        child_at(expression_list, 3U),
        MYLITE_SQL_AST_LAST_DAY_FUNCTION,
        "do last_day extract"
    );
    failures +=
        expect_node(child_at(expression_list, 4U), MYLITE_SQL_AST_TIME_FUNCTION, "do time extract");
    failures += expect_node(
        child_at(expression_list, temporal_extract_do_hour_index),
        MYLITE_SQL_AST_HOUR_FUNCTION,
        "do hour extract"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DAYOFMONTH();", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_DAYOFMONTH_ARGUMENT_COUNT_ERROR,
        "dayofmonth zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DAYOFWEEK();", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_DAYOFWEEK_ARGUMENT_COUNT_ERROR,
        "dayofweek zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DAYOFYEAR('2008-01-02', 'x');", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_DAYOFYEAR_ARGUMENT_COUNT_ERROR,
        "dayofyear extra argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT LAST_DAY();", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_LAST_DAY_ARGUMENT_COUNT_ERROR,
        "last_day zero argument marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE temporal_extract_keywords(day INT, dayofmonth INT, dayofweek INT, "
        "dayofyear INT, last_day INT, hour INT, minute INT, month INT, second INT, year INT, "
        "date DATE, time TIME);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DATE();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT TIME();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT HOUR();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_scalar_subquery_expression(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *inner_select = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT (SELECT DATABASE()), CONCAT('x', (SELECT 1 FROM DUAL)) AS c;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_SCALAR_SUBQUERY, "scalar subquery");
    failures += expect_span_text(first_expression, "(SELECT DATABASE())", "scalar subquery span");
    inner_select = child_at(first_expression, 0U);
    failures +=
        expect_node(inner_select, MYLITE_SQL_AST_SELECT_STATEMENT, "scalar subquery select");
    failures += expect_node(
        child_at(child_at(child_at(inner_select, 0U), 0U), 0U),
        MYLITE_SQL_AST_DATABASE_FUNCTION,
        "scalar subquery inner database"
    );

    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_CONCAT_FUNCTION,
        "concat with scalar subquery"
    );
    arguments = child_at(second_expression, 0U);
    failures += expect_literal(
        child_at(arguments, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "concat scalar subquery string"
    );
    failures += expect_node(
        child_at(arguments, 1U),
        MYLITE_SQL_AST_SCALAR_SUBQUERY,
        "concat scalar subquery argument"
    );
    failures += expect_node(
        child_at(child_at(child_at(arguments, 1U), 0U), 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "scalar subquery from dual"
    );
    failures +=
        expect_node(child_at(child_at(select_list, 1U), 1U), MYLITE_SQL_AST_IDENTIFIER, "alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT (SELECT DATABASE(), SCHEMA());", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_SCALAR_SUBQUERY,
        "multi-column scalar subquery marker"
    );
    failures += expect_child_count(
        child_at(child_at(first_expression, 0U), 0U),
        2U,
        "multi-column scalar subquery select list"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_nullif_function(void) {
    enum {
        nullif_nested_if_item_index = 3,
        nullif_nested_ifnull_item_index = 4,
        nullif_nested_coalesce_item_index = 5,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT NULLIF(1,1), NullIf(TRUE,FALSE), nullif(+0,-0) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_NULLIF_FUNCTION, "nullif function");
    failures += expect_span_text(first_expression, "NULLIF(1,1)", "nullif function span");
    failures += expect_child_count(first_expression, 2U, "nullif function argument count");
    failures += expect_literal(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "nullif left value"
    );
    failures += expect_literal(
        child_at(first_expression, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "nullif right value"
    );
    failures += expect_node(second_expression, MYLITE_SQL_AST_NULLIF_FUNCTION, "mixed nullif");
    failures +=
        expect_literal(child_at(second_expression, 0U), MYLITE_SQL_AST_LITERAL_TRUE, "nullif TRUE");
    failures += expect_literal(
        child_at(second_expression, 1U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "nullif FALSE"
    );
    failures += expect_node(third_expression, MYLITE_SQL_AST_NULLIF_FUNCTION, "lower nullif");
    failures += expect_operator(
        child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "nullif positive value"
    );
    failures += expect_operator(
        child_at(third_expression, 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "nullif negative comparison value"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "nullif from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT NULLIF (1,1), (NULLIF(1,2)), NULLIF(NULLIF(1,1),1), "
        "NULLIF(IF(1,2,3),2), NULLIF(IFNULL(NULL,4),4), "
        "NULLIF(COALESCE(NULL,6),6);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_NULLIF_FUNCTION, "spaced nullif");
    failures += expect_span_text(first_expression, "NULLIF (1,1)", "spaced nullif span");
    failures +=
        expect_node(parenthesized, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "parenthesized nullif");
    failures +=
        expect_node(child_at(parenthesized, 0U), MYLITE_SQL_AST_NULLIF_FUNCTION, "wrapped nullif");
    failures += expect_span_text(parenthesized, "(NULLIF(1,2))", "parenthesized nullif span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_NULLIF_FUNCTION, "outer nullif");
    failures +=
        expect_node(child_at(third_expression, 0U), MYLITE_SQL_AST_NULLIF_FUNCTION, "inner nullif");
    failures += expect_node(
        child_at(child_at(child_at(select_list, nullif_nested_if_item_index), 0U), 0U),
        MYLITE_SQL_AST_IF_FUNCTION,
        "nested if in nullif"
    );
    failures += expect_node(
        child_at(child_at(child_at(select_list, nullif_nested_ifnull_item_index), 0U), 0U),
        MYLITE_SQL_AST_IFNULL_FUNCTION,
        "nested ifnull in nullif"
    );
    failures += expect_node(
        child_at(child_at(child_at(select_list, nullif_nested_coalesce_item_index), 0U), 0U),
        MYLITE_SQL_AST_COALESCE_FUNCTION,
        "nested coalesce in nullif"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT NULLIF(1, 2 + 3);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_NULLIF_FUNCTION, "deferred nullif");
    failures += expect_node(
        child_at(first_expression, 1U),
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "deferred nullif arithmetic argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT NULLIF();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR,
        "empty nullif argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT NULLIF(1);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR,
        "one nullif argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT NULLIF(1,2,3);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR,
        "three nullif argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT NULLIF(1,,2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_isnull_function(void) {
    enum {
        isnull_nested_if_item_index = 3,
        isnull_nested_ifnull_item_index = 4,
        isnull_nested_coalesce_item_index = 5,
        isnull_nested_nullif_item_index = 6,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT ISNULL(NULL), IsNull(TRUE), isnull(+0) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_ISNULL_FUNCTION, "isnull function");
    failures += expect_span_text(first_expression, "ISNULL(NULL)", "isnull function span");
    failures += expect_child_count(first_expression, 1U, "isnull function argument count");
    failures +=
        expect_literal(child_at(first_expression, 0U), MYLITE_SQL_AST_LITERAL_NULL, "isnull NULL");
    failures += expect_node(second_expression, MYLITE_SQL_AST_ISNULL_FUNCTION, "mixed isnull");
    failures +=
        expect_literal(child_at(second_expression, 0U), MYLITE_SQL_AST_LITERAL_TRUE, "isnull TRUE");
    failures += expect_node(third_expression, MYLITE_SQL_AST_ISNULL_FUNCTION, "lower isnull");
    failures += expect_operator(
        child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "isnull positive value"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "isnull from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT ISNULL (NULL), (ISNULL(NULL)), ISNULL(ISNULL(NULL)), "
        "ISNULL(IF(0,NULL,2)), ISNULL(IFNULL(NULL,4)), "
        "ISNULL(COALESCE(NULL,NULL)), ISNULL(NULLIF(1,1));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_ISNULL_FUNCTION, "spaced isnull");
    failures += expect_span_text(first_expression, "ISNULL (NULL)", "spaced isnull span");
    failures +=
        expect_node(parenthesized, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "parenthesized isnull");
    failures +=
        expect_node(child_at(parenthesized, 0U), MYLITE_SQL_AST_ISNULL_FUNCTION, "wrapped isnull");
    failures += expect_span_text(parenthesized, "(ISNULL(NULL))", "parenthesized isnull span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_ISNULL_FUNCTION, "outer isnull");
    failures +=
        expect_node(child_at(third_expression, 0U), MYLITE_SQL_AST_ISNULL_FUNCTION, "inner isnull");
    failures += expect_node(
        child_at(child_at(child_at(select_list, isnull_nested_if_item_index), 0U), 0U),
        MYLITE_SQL_AST_IF_FUNCTION,
        "nested if in isnull"
    );
    failures += expect_node(
        child_at(child_at(child_at(select_list, isnull_nested_ifnull_item_index), 0U), 0U),
        MYLITE_SQL_AST_IFNULL_FUNCTION,
        "nested ifnull in isnull"
    );
    failures += expect_node(
        child_at(child_at(child_at(select_list, isnull_nested_coalesce_item_index), 0U), 0U),
        MYLITE_SQL_AST_COALESCE_FUNCTION,
        "nested coalesce in isnull"
    );
    failures += expect_node(
        child_at(child_at(child_at(select_list, isnull_nested_nullif_item_index), 0U), 0U),
        MYLITE_SQL_AST_NULLIF_FUNCTION,
        "nested nullif in isnull"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ISNULL(1 + 2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_ISNULL_FUNCTION, "deferred isnull");
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_BINARY_EXPRESSION,
        "deferred isnull arithmetic argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ISNULL();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR,
        "empty isnull argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT ISNULL(1,2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR,
        "two isnull argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT ISNULL(1,2,3);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR,
        "three isnull argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ISNULL(,1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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

    failures +=
        parse_sql("SELECT ABS(-64), Abs(NULL), abs(~0) FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_ABS_FUNCTION, "abs function");
    failures += expect_span_text(first_expression, "ABS(-64)", "abs function span");
    failures += expect_child_count(first_expression, 1U, "abs argument count");
    failures += expect_operator(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "abs negative integer"
    );
    failures += expect_node(second_expression, MYLITE_SQL_AST_ABS_FUNCTION, "mixed abs");
    failures +=
        expect_literal(child_at(second_expression, 0U), MYLITE_SQL_AST_LITERAL_NULL, "abs NULL");
    failures += expect_node(third_expression, MYLITE_SQL_AST_ABS_FUNCTION, "lower abs");
    failures += expect_operator(
        child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_NOT,
        "abs bitwise argument"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "abs from dual");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT ABS (1), (ABS(1)), ABS(IFNULL(NULL,-7));", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_ABS_FUNCTION, "spaced abs");
    failures += expect_span_text(first_expression, "ABS (1)", "spaced abs span");
    failures +=
        expect_node(parenthesized, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "parenthesized abs");
    failures +=
        expect_node(child_at(parenthesized, 0U), MYLITE_SQL_AST_ABS_FUNCTION, "wrapped abs");
    failures += expect_span_text(parenthesized, "(ABS(1))", "parenthesized abs span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_ABS_FUNCTION, "ifnull abs value");
    failures +=
        expect_node(child_at(third_expression, 0U), MYLITE_SQL_AST_IFNULL_FUNCTION, "abs ifnull");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ABS();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_ABS_ARGUMENT_COUNT_ERROR,
        "empty abs argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT ABS(1,2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_ABS_ARGUMENT_COUNT_ERROR,
        "two abs argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE abs (abs INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "abs identifier");
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

    failures += parse_sql(
        "SELECT SIGN(-64), Sign(NULL), sign(~0) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_SIGN_FUNCTION, "sign function");
    failures += expect_span_text(first_expression, "SIGN(-64)", "sign function span");
    failures += expect_child_count(first_expression, 1U, "sign argument count");
    failures += expect_operator(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "sign negative integer"
    );
    failures += expect_node(second_expression, MYLITE_SQL_AST_SIGN_FUNCTION, "mixed sign");
    failures +=
        expect_literal(child_at(second_expression, 0U), MYLITE_SQL_AST_LITERAL_NULL, "sign NULL");
    failures += expect_node(third_expression, MYLITE_SQL_AST_SIGN_FUNCTION, "lower sign");
    failures += expect_operator(
        child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_NOT,
        "sign bitwise argument"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "sign from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT SIGN (1), (SIGN(1)), SIGN(IFNULL(NULL,-7));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_SIGN_FUNCTION, "spaced sign");
    failures += expect_span_text(first_expression, "SIGN (1)", "spaced sign span");
    failures +=
        expect_node(parenthesized, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "parenthesized sign");
    failures +=
        expect_node(child_at(parenthesized, 0U), MYLITE_SQL_AST_SIGN_FUNCTION, "wrapped sign");
    failures += expect_span_text(parenthesized, "(SIGN(1))", "parenthesized sign span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_SIGN_FUNCTION, "ifnull sign value");
    failures +=
        expect_node(child_at(third_expression, 0U), MYLITE_SQL_AST_IFNULL_FUNCTION, "sign ifnull");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SIGN();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_SIGN_ARGUMENT_COUNT_ERROR,
        "empty sign argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT SIGN(1,2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_SIGN_ARGUMENT_COUNT_ERROR,
        "two sign argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE sign (sign INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "sign identifier");
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

    failures += parse_sql(
        "SELECT CEIL(-64), Ceiling(NULL), floor(~0), ROUND(1+2) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    fourth_expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_CEIL_FUNCTION, "ceil function");
    failures += expect_span_text(first_expression, "CEIL(-64)", "ceil function span");
    failures += expect_child_count(first_expression, 1U, "ceil argument count");
    failures += expect_operator(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "ceil negative integer"
    );
    failures += expect_node(second_expression, MYLITE_SQL_AST_CEILING_FUNCTION, "mixed ceiling");
    failures += expect_literal(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "ceiling NULL"
    );
    failures += expect_node(third_expression, MYLITE_SQL_AST_FLOOR_FUNCTION, "lower floor");
    failures += expect_operator(
        child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_NOT,
        "floor bitwise argument"
    );
    failures += expect_node(fourth_expression, MYLITE_SQL_AST_ROUND_FUNCTION, "round function");
    failures += expect_operator(
        child_at(fourth_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "round arithmetic argument"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "rounding from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT CEIL (1), (CEILING(1)), FLOOR(IFNULL(NULL,-7));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_CEIL_FUNCTION, "spaced ceil");
    failures += expect_span_text(first_expression, "CEIL (1)", "spaced ceil span");
    failures += expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized ceiling"
    );
    failures += expect_node(
        child_at(parenthesized, 0U),
        MYLITE_SQL_AST_CEILING_FUNCTION,
        "wrapped ceiling"
    );
    failures += expect_span_text(parenthesized, "(CEILING(1))", "parenthesized ceiling span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_FLOOR_FUNCTION, "floor ifnull value");
    failures +=
        expect_node(child_at(third_expression, 0U), MYLITE_SQL_AST_IFNULL_FUNCTION, "floor ifnull");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ROUND(123,-1);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_ROUND_FUNCTION, "two-argument round function");
    failures += expect_child_count(first_expression, 2U, "two-argument round child count");
    failures += expect_literal(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "two-argument round value"
    );
    failures += expect_operator(
        child_at(first_expression, 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "two-argument round places"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CEIL();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CEIL_ARGUMENT_COUNT_ERROR,
        "empty ceil argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CEILING(1,2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CEILING_ARGUMENT_COUNT_ERROR,
        "two ceiling argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT FLOOR();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_FLOOR_ARGUMENT_COUNT_ERROR,
        "empty floor argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT ROUND();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_ROUND_ARGUMENT_COUNT_ERROR,
        "empty round argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT ROUND(1,2,3);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_ROUND_ARGUMENT_COUNT_ERROR,
        "extra round argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE ceil (ceiling INT, floor INT, round INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "rounding identifiers");
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

    failures += parse_sql(
        "SELECT BIT_COUNT(64), Bit_Count(NULL), bit_count(~0) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_BIT_COUNT_FUNCTION, "bit_count function");
    failures += expect_span_text(first_expression, "BIT_COUNT(64)", "bit_count function span");
    failures += expect_child_count(first_expression, 1U, "bit_count argument count");
    failures += expect_literal(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "bit_count integer"
    );
    failures +=
        expect_node(second_expression, MYLITE_SQL_AST_BIT_COUNT_FUNCTION, "mixed bit_count");
    failures += expect_literal(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "bit_count NULL"
    );
    failures += expect_node(third_expression, MYLITE_SQL_AST_BIT_COUNT_FUNCTION, "lower bit_count");
    failures += expect_operator(
        child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_NOT,
        "bit_count bitwise argument"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "bit_count from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT BIT_COUNT (1), (BIT_COUNT(1)), BIT_COUNT(IFNULL(NULL,7));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_BIT_COUNT_FUNCTION, "spaced bit_count");
    failures += expect_span_text(first_expression, "BIT_COUNT (1)", "spaced bit_count span");
    failures += expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized bit_count"
    );
    failures += expect_node(
        child_at(parenthesized, 0U),
        MYLITE_SQL_AST_BIT_COUNT_FUNCTION,
        "wrapped bit_count"
    );
    failures += expect_span_text(parenthesized, "(BIT_COUNT(1))", "parenthesized bit_count span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_BIT_COUNT_FUNCTION, "nested value");
    failures += expect_node(
        child_at(third_expression, 0U),
        MYLITE_SQL_AST_IFNULL_FUNCTION,
        "ifnull bit_count value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT BIT_COUNT();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_BIT_COUNT_ARGUMENT_COUNT_ERROR,
        "empty bit_count argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT BIT_COUNT(1,2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_BIT_COUNT_ARGUMENT_COUNT_ERROR,
        "two bit_count argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bit_count (bit_count INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "bit_count identifier");
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

    failures += parse_sql(
        "SELECT CRC32('MySQL'), FORMAT(12332.123456,4), TRUNCATE(-1.999,1) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_CRC32_FUNCTION, "crc32 function");
    failures += expect_span_text(expression, "CRC32('MySQL')", "crc32 span");
    failures += expect_literal(child_at(expression, 0U), MYLITE_SQL_AST_LITERAL_STRING, "crc32");
    failures += expect_node(second_expression, MYLITE_SQL_AST_FORMAT_FUNCTION, "format function");
    failures += expect_child_count(second_expression, 2U, "format argument count");
    failures += expect_literal(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_DECIMAL,
        "format value"
    );
    failures += expect_literal(
        child_at(second_expression, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "format places"
    );
    failures +=
        expect_node(third_expression, MYLITE_SQL_AST_TRUNCATE_FUNCTION, "truncate function");
    failures += expect_operator(
        child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "truncate signed value"
    );
    failures +=
        expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "numeric functions dual");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT crc32 (X'616263'), format(+1, -1);", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_CRC32_FUNCTION, "spaced crc32");
    failures += expect_literal(child_at(expression, 0U), MYLITE_SQL_AST_LITERAL_HEX, "crc32 hex");
    failures += expect_node(second_expression, MYLITE_SQL_AST_FORMAT_FUNCTION, "lower format");
    failures += expect_operator(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "format positive value"
    );
    failures += expect_operator(
        child_at(second_expression, 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "format negative places"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CRC32();", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_CRC32_ARGUMENT_COUNT_ERROR,
        "crc32 empty argument count"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT FORMAT(1);", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_FORMAT_ARGUMENT_COUNT_ERROR,
        "format one argument count"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT FORMAT(1,2,'de_DE');", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_FORMAT_LOCALE_UNSUPPORTED,
        "format locale unsupported"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT TRUNCATE(1);", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_TRUNCATE_ARGUMENT_COUNT_ERROR,
        "truncate one argument count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE numeric_names (crc32 INT, format INT, truncate INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    failures +=
        expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "numeric function identifiers");
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

    failures += parse_sql(
        "SELECT HEX('abc'), HEX(X'0061'), HEX(-1), HEX(v) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_HEX_FUNCTION, "hex string function");
    failures += expect_span_text(expression, "HEX('abc')", "hex string span");
    failures +=
        expect_literal(child_at(expression, 0U), MYLITE_SQL_AST_LITERAL_STRING, "hex string");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_HEX_FUNCTION, "hex binary function");
    failures += expect_literal(child_at(expression, 0U), MYLITE_SQL_AST_LITERAL_HEX, "hex literal");
    expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_HEX_FUNCTION, "hex signed function");
    failures += expect_operator(
        child_at(expression, 0U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "hex negative argument"
    );
    expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_HEX_FUNCTION, "hex column function");
    failures += expect_node(child_at(expression, 0U), MYLITE_SQL_AST_IDENTIFIER, "hex column");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "hex from table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT HEX ('a'), (HEX(255)) FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_HEX_FUNCTION, "spaced hex");
    failures += expect_span_text(expression, "HEX ('a')", "spaced hex span");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(expression, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "parenthesized hex");
    failures += expect_node(child_at(expression, 0U), MYLITE_SQL_AST_HEX_FUNCTION, "wrapped hex");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT HEX();", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_HEX_ARGUMENT_COUNT_ERROR,
        "empty hex argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT HEX('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_HEX_ARGUMENT_COUNT_ERROR,
        "two hex argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DO HEX('abc'), HEX(NULL);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "hex do");
    failures += expect_node(child_at(expression_list, 0U), MYLITE_SQL_AST_HEX_FUNCTION, "do hex");
    failures +=
        expect_node(child_at(expression_list, 1U), MYLITE_SQL_AST_HEX_FUNCTION, "do null hex");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE hex_names (hex INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "hex identifier");
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

    failures += parse_sql(
        "SELECT UNHEX('4D'), UNHEX(X'3431'), UNHEX(+1), UNHEX(v) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_UNHEX_FUNCTION, "unhex string function");
    failures += expect_span_text(expression, "UNHEX('4D')", "unhex string span");
    failures +=
        expect_literal(child_at(expression, 0U), MYLITE_SQL_AST_LITERAL_STRING, "unhex string");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_UNHEX_FUNCTION, "unhex binary function");
    failures +=
        expect_literal(child_at(expression, 0U), MYLITE_SQL_AST_LITERAL_HEX, "unhex hex literal");
    expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_UNHEX_FUNCTION, "unhex signed function");
    failures += expect_operator(
        child_at(expression, 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "unhex positive argument"
    );
    expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_UNHEX_FUNCTION, "unhex column function");
    failures += expect_node(child_at(expression, 0U), MYLITE_SQL_AST_IDENTIFIER, "unhex column");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "unhex from table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT UNHEX ('F'), (UNHEX(NULL)) FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_UNHEX_FUNCTION, "spaced unhex");
    failures += expect_span_text(expression, "UNHEX ('F')", "spaced unhex span");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(expression, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "parenthesized unhex");
    failures +=
        expect_node(child_at(expression, 0U), MYLITE_SQL_AST_UNHEX_FUNCTION, "wrapped unhex");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT UNHEX();", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_UNHEX_ARGUMENT_COUNT_ERROR,
        "empty unhex argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT UNHEX('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_UNHEX_ARGUMENT_COUNT_ERROR,
        "two unhex argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DO UNHEX('41'), UNHEX(NULL);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "unhex do");
    failures +=
        expect_node(child_at(expression_list, 0U), MYLITE_SQL_AST_UNHEX_FUNCTION, "do unhex");
    failures +=
        expect_node(child_at(expression_list, 1U), MYLITE_SQL_AST_UNHEX_FUNCTION, "do null unhex");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE unhex_names (unhex INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "unhex identifier");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_charset_collation_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT CHARSET('abc'), COLLATION(v), CHARSET(CAST('ABC' AS BINARY)) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_CHARSET_FUNCTION, "charset function");
    failures += expect_span_text(expression, "CHARSET('abc')", "charset span");
    failures +=
        expect_literal(child_at(expression, 0U), MYLITE_SQL_AST_LITERAL_STRING, "charset literal");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_COLLATION_FUNCTION, "collation function");
    failures +=
        expect_node(child_at(expression, 0U), MYLITE_SQL_AST_IDENTIFIER, "collation column");
    expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_CHARSET_FUNCTION, "charset cast binary");
    failures += expect_node(
        child_at(expression, 0U),
        MYLITE_SQL_AST_CAST_BINARY_EXPRESSION,
        "charset cast binary argument"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "charset from table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT CHARSET ('a'), (COLLATION(CONVERT('A' USING BINARY))) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_CHARSET_FUNCTION, "spaced charset");
    failures += expect_span_text(expression, "CHARSET ('a')", "spaced charset span");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(expression, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "wrapped collation");
    failures += expect_node(
        child_at(expression, 0U),
        MYLITE_SQL_AST_COLLATION_FUNCTION,
        "wrapped collation function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CHARSET();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    failures += parse_sql("SELECT COLLATION('a', 'b');", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);

    failures += parse_sql("DO CHARSET('abc'), COLLATION(NULL);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "charset do");
    failures +=
        expect_node(child_at(expression_list, 0U), MYLITE_SQL_AST_CHARSET_FUNCTION, "do charset");
    failures += expect_node(
        child_at(expression_list, 1U),
        MYLITE_SQL_AST_COLLATION_FUNCTION,
        "do collation"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE charset_names (charset INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "charset identifier");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_string_length_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT LENGTH(v), OCTET_LENGTH('a'), BIT_LENGTH(1), CHAR_LENGTH(NULL), "
        "CHARACTER_LENGTH(name) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_LENGTH_FUNCTION, "length function");
    failures += expect_span_text(expression, "LENGTH(v)", "length span");
    failures += expect_node(child_at(expression, 0U), MYLITE_SQL_AST_IDENTIFIER, "length column");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(expression, MYLITE_SQL_AST_OCTET_LENGTH_FUNCTION, "octet_length function");
    failures += expect_literal(
        child_at(expression, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "octet_length literal"
    );
    expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_BIT_LENGTH_FUNCTION, "bit_length function");
    expression = child_at(child_at(select_list, 3U), 0U);
    failures +=
        expect_node(expression, MYLITE_SQL_AST_CHAR_LENGTH_FUNCTION, "char_length function");
    expression = child_at(child_at(select_list, 4U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_CHARACTER_LENGTH_FUNCTION,
        "character_length function"
    );
    failures +=
        expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "string length from table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT LENGTH ('a'), (CHAR_LENGTH('a')), BIT_LENGTH(+1) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_LENGTH_FUNCTION, "spaced length");
    failures += expect_span_text(expression, "LENGTH ('a')", "spaced length span");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized char_length"
    );
    failures += expect_node(
        child_at(expression, 0U),
        MYLITE_SQL_AST_CHAR_LENGTH_FUNCTION,
        "wrapped char_length"
    );
    expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_BIT_LENGTH_FUNCTION, "signed bit_length");
    failures += expect_operator(
        child_at(expression, 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "bit_length signed argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT LENGTH();", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_LENGTH_ARGUMENT_COUNT_ERROR,
        "empty length argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CHARACTER_LENGTH('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_CHARACTER_LENGTH_ARGUMENT_COUNT_ERROR,
        "two character_length argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DO LENGTH('a'), OCTET_LENGTH('a'), BIT_LENGTH('a'), CHAR_LENGTH('a');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "string length do");
    failures +=
        expect_node(child_at(expression_list, 0U), MYLITE_SQL_AST_LENGTH_FUNCTION, "do length");
    failures += expect_node(
        child_at(expression_list, 3U),
        MYLITE_SQL_AST_CHAR_LENGTH_FUNCTION,
        "do char_length"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE lengths (length INT, char_length INT, bit_length INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    failures +=
        expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "string length identifiers");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_string_case_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT LOWER(v), LCASE('A'), UPPER(1), UCASE(NULL) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_LOWER_FUNCTION, "lower function");
    failures += expect_span_text(expression, "LOWER(v)", "lower span");
    failures += expect_node(child_at(expression, 0U), MYLITE_SQL_AST_IDENTIFIER, "lower column");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_LCASE_FUNCTION, "lcase function");
    failures +=
        expect_literal(child_at(expression, 0U), MYLITE_SQL_AST_LITERAL_STRING, "lcase literal");
    expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_UPPER_FUNCTION, "upper function");
    failures +=
        expect_literal(child_at(expression, 0U), MYLITE_SQL_AST_LITERAL_INTEGER, "upper integer");
    expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_UCASE_FUNCTION, "ucase function");
    failures += expect_literal(child_at(expression, 0U), MYLITE_SQL_AST_LITERAL_NULL, "ucase null");
    failures +=
        expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "string case from table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT LOWER ('A'), (UPPER('a')), LCASE(+1), UCASE(DATABASE()) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_LOWER_FUNCTION, "spaced lower");
    failures += expect_span_text(expression, "LOWER ('A')", "spaced lower span");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(expression, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "parenthesized upper");
    failures +=
        expect_node(child_at(expression, 0U), MYLITE_SQL_AST_UPPER_FUNCTION, "wrapped upper");
    expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_LCASE_FUNCTION, "signed lcase");
    failures += expect_operator(
        child_at(expression, 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "lcase signed argument"
    );
    expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_UCASE_FUNCTION, "database ucase");
    failures += expect_node(
        child_at(expression, 0U),
        MYLITE_SQL_AST_DATABASE_FUNCTION,
        "ucase database argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT LOWER();", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_LOWER_ARGUMENT_COUNT_ERROR,
        "empty lower argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT LCASE();", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_LCASE_ARGUMENT_COUNT_ERROR,
        "empty lcase argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT UPPER('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_UPPER_ARGUMENT_COUNT_ERROR,
        "two upper argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT UCASE('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_UCASE_ARGUMENT_COUNT_ERROR,
        "two ucase argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DO LOWER('A'), LCASE('B'), UPPER('c'), UCASE('d');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "string case do");
    failures +=
        expect_node(child_at(expression_list, 0U), MYLITE_SQL_AST_LOWER_FUNCTION, "do lower");
    failures +=
        expect_node(child_at(expression_list, 3U), MYLITE_SQL_AST_UCASE_FUNCTION, "do ucase");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE case_words (lower INT, lcase INT, upper INT, ucase INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    failures +=
        expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "string case identifiers");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_string_trim_functions(void) {
    enum {
        trim_trailing_item_index = 5,
        trim_both_default_item_index = 6,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT LTRIM(v), RTRIM('a'), TRIM('  a  '), TRIM('x' FROM 'xxaxx'), "
        "TRIM(LEADING 'x' FROM 'xxa'), TRIM(TRAILING 'x' FROM 'axx'), "
        "TRIM(BOTH FROM ' a ') FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_LTRIM_FUNCTION, "ltrim function");
    failures += expect_span_text(expression, "LTRIM(v)", "ltrim span");
    failures += expect_node(child_at(expression, 0U), MYLITE_SQL_AST_IDENTIFIER, "ltrim column");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_RTRIM_FUNCTION, "rtrim function");
    failures +=
        expect_literal(child_at(expression, 0U), MYLITE_SQL_AST_LITERAL_STRING, "rtrim literal");
    expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_TRIM_FUNCTION, "trim function");
    failures += expect_child_count(expression, 1U, "trim default child count");
    expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_TRIM_FUNCTION, "trim remove function");
    failures += expect_child_count(expression, 2U, "trim remove child count");
    failures +=
        expect_literal(child_at(expression, 0U), MYLITE_SQL_AST_LITERAL_STRING, "trim value child");
    failures += expect_literal(
        child_at(expression, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "trim remove child"
    );
    expression = child_at(child_at(select_list, 4U), 0U);
    failures +=
        expect_node(expression, MYLITE_SQL_AST_TRIM_LEADING_FUNCTION, "trim leading function");
    failures += expect_child_count(expression, 2U, "trim leading child count");
    expression = child_at(child_at(select_list, trim_trailing_item_index), 0U);
    failures +=
        expect_node(expression, MYLITE_SQL_AST_TRIM_TRAILING_FUNCTION, "trim trailing function");
    expression = child_at(child_at(select_list, trim_both_default_item_index), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_TRIM_FUNCTION, "trim both default function");
    failures += expect_child_count(expression, 1U, "trim both default child count");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "trim from table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT LTRIM ('  a'), (RTRIM('a  ')), TRIM(LEADING FROM '  a'), "
        "TRIM(TRAILING FROM 'a  '), TRIM(+1 FROM 1112111) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_LTRIM_FUNCTION, "spaced ltrim");
    failures += expect_span_text(expression, "LTRIM ('  a')", "spaced ltrim span");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(expression, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "parenthesized rtrim");
    failures +=
        expect_node(child_at(expression, 0U), MYLITE_SQL_AST_RTRIM_FUNCTION, "wrapped rtrim");
    expression = child_at(child_at(select_list, 2U), 0U);
    failures +=
        expect_node(expression, MYLITE_SQL_AST_TRIM_LEADING_FUNCTION, "trim leading default");
    failures += expect_child_count(expression, 1U, "trim leading default child count");
    expression = child_at(child_at(select_list, 3U), 0U);
    failures +=
        expect_node(expression, MYLITE_SQL_AST_TRIM_TRAILING_FUNCTION, "trim trailing default");
    failures += expect_child_count(expression, 1U, "trim trailing default child count");
    expression = child_at(child_at(select_list, 4U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_TRIM_FUNCTION, "signed trim remove");
    failures += expect_operator(
        child_at(expression, 1U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "trim signed remove argument"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT LTRIM();", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_LTRIM_ARGUMENT_COUNT_ERROR,
        "empty ltrim argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT RTRIM('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        expression,
        MYLITE_SQL_AST_RTRIM_ARGUMENT_COUNT_ERROR,
        "two rtrim argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT TRIM();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT TRIM('a', 'b');", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DO LTRIM(' a'), RTRIM('a '), TRIM(' a '), TRIM('x' FROM 'xax');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "trim do");
    failures +=
        expect_node(child_at(expression_list, 0U), MYLITE_SQL_AST_LTRIM_FUNCTION, "do ltrim");
    failures += expect_node(child_at(expression_list, 3U), MYLITE_SQL_AST_TRIM_FUNCTION, "do trim");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE trim_words (ltrim INT, rtrim INT, trim INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "trim identifiers");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_string_slice_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    const struct mylite_sql_ast_node *from_join = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT LEFT(v, 2), RIGHT('abc', +1), LEFT(DATABASE(), 6) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_LEFT_FUNCTION, "left function");
    failures += expect_span_text(expression, "LEFT(v, 2)", "left span");
    failures += expect_node(child_at(expression, 0U), MYLITE_SQL_AST_IDENTIFIER, "left column");
    failures +=
        expect_literal(child_at(expression, 1U), MYLITE_SQL_AST_LITERAL_INTEGER, "left length");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_RIGHT_FUNCTION, "right function");
    failures +=
        expect_literal(child_at(expression, 0U), MYLITE_SQL_AST_LITERAL_STRING, "right literal");
    failures += expect_operator(
        child_at(expression, 1U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "right signed length"
    );
    expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_LEFT_FUNCTION, "database left");
    failures += expect_node(
        child_at(expression, 0U),
        MYLITE_SQL_AST_DATABASE_FUNCTION,
        "left database argument"
    );
    failures +=
        expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "string slice from table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT SUBSTRING(v, 2), SUBSTR('abcdef', 2, 3), MID(v FROM -4 FOR 2) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_SUBSTRING_FUNCTION, "substring function");
    failures += expect_span_text(expression, "SUBSTRING(v, 2)", "substring span");
    failures += expect_child_count(expression, 2U, "substring arity");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_SUBSTR_FUNCTION, "substr function");
    failures += expect_child_count(expression, 3U, "substr arity");
    expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_MID_FUNCTION, "mid function");
    failures += expect_span_text(expression, "MID(v FROM -4 FOR 2)", "mid from span");
    failures += expect_operator(
        child_at(expression, 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "mid negative position"
    );
    failures +=
        expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "substring from table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT SUBSTRING('abcdef' FROM 2), SUBSTR('abcdef' FROM 2 FOR 3), "
        "MID('abcdef' FROM 2 FOR 3) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_SUBSTRING_FUNCTION, "substring from");
    failures += expect_child_count(expression, 2U, "substring from arity");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_SUBSTR_FUNCTION, "substr from for");
    expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_MID_FUNCTION, "mid from for");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT SUBSTRING_INDEX(v, '.', 2), SUBSTRING_INDEX ('abc', 'b', -1) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION, "substring_index");
    failures += expect_span_text(expression, "SUBSTRING_INDEX(v, '.', 2)", "substring_index span");
    failures += expect_child_count(expression, 3U, "substring_index arity");
    failures += expect_node(child_at(expression, 0U), MYLITE_SQL_AST_IDENTIFIER, "index value");
    failures +=
        expect_literal(child_at(expression, 1U), MYLITE_SQL_AST_LITERAL_STRING, "index delimiter");
    failures +=
        expect_literal(child_at(expression, 2U), MYLITE_SQL_AST_LITERAL_INTEGER, "index count");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(expression, MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION, "spaced substring_index");
    failures += expect_span_text(
        expression,
        "SUBSTRING_INDEX ('abc', 'b', -1)",
        "spaced substring_index span"
    );
    failures += expect_operator(
        child_at(expression, 2U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "substring_index signed count"
    );
    failures +=
        expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "substring_index from table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT LEFT ('abc', 1), (RIGHT('abc', 1)) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_LEFT_FUNCTION, "spaced left");
    failures += expect_span_text(expression, "LEFT ('abc', 1)", "spaced left span");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(expression, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "parenthesized right");
    failures +=
        expect_node(child_at(expression, 0U), MYLITE_SQL_AST_RIGHT_FUNCTION, "wrapped right");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT LEFT();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    failures += parse_sql("SELECT LEFT('a');", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    failures += parse_sql("SELECT LEFT('a', 1, 2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    failures += parse_sql("SELECT RIGHT();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    failures += parse_sql("SELECT RIGHT('a');", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    failures += parse_sql("SELECT RIGHT('a', 1, 2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    failures += parse_sql("SELECT SUBSTRING();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    failures += parse_sql("SELECT SUBSTRING('a');", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    failures += parse_sql("SELECT MID('a', 1, 2, 3);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    failures += parse_sql("SELECT SUBSTRING ('abc', 1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);

    failures += parse_sql("SELECT SUBSTRING_INDEX();", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_SUBSTRING_INDEX_ARGUMENT_COUNT_ERROR,
        "substring_index zero argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT SUBSTRING_INDEX('a');", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_SUBSTRING_INDEX_ARGUMENT_COUNT_ERROR,
        "substring_index one argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT SUBSTRING_INDEX('a', 'b');", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_SUBSTRING_INDEX_ARGUMENT_COUNT_ERROR,
        "substring_index two argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT SUBSTRING_INDEX('a', 'b', 1, 2);", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_SUBSTRING_INDEX_ARGUMENT_COUNT_ERROR,
        "substring_index many argument error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DO LEFT('abc', 1), RIGHT('abc', 1);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "string slice do");
    failures += expect_node(child_at(expression_list, 0U), MYLITE_SQL_AST_LEFT_FUNCTION, "do left");
    failures +=
        expect_node(child_at(expression_list, 1U), MYLITE_SQL_AST_RIGHT_FUNCTION, "do right");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DO SUBSTRING('abc', 1), SUBSTR('abc' FROM 1), MID('abc', 1, 1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "substring do");
    failures += expect_node(
        child_at(expression_list, 0U),
        MYLITE_SQL_AST_SUBSTRING_FUNCTION,
        "do substring"
    );
    failures += expect_node(child_at(expression_list, 2U), MYLITE_SQL_AST_MID_FUNCTION, "do mid");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DO SUBSTRING_INDEX('abc', 'b', 1);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "substring_index do");
    failures += expect_node(
        child_at(expression_list, 0U),
        MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION,
        "do substring_index"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE substring_words (substring INT, substr INT, mid INT, substring_index INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "substring identifiers");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT l.id FROM lefts l LEFT JOIN rights r ON l.k = r.k;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    from_join = child_at(child_at(result.root, 0U), 1U);
    failures += expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "left join after left function");
    if (from_join != NULL &&
        mylite_sql_ast_node_join_kind(from_join) != MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER) {
        fprintf(stderr, "left join after left function: expected left outer join kind\n");
        ++failures;
    }
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_string_padding_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT LPAD(v, 5, '0'), RPAD('abc', +4, 'x'), REPEAT(v, 2), SPACE(3) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_LPAD_FUNCTION, "lpad function");
    failures += expect_span_text(expression, "LPAD(v, 5, '0')", "lpad span");
    failures += expect_child_count(expression, 3U, "lpad arity");
    failures += expect_node(child_at(expression, 0U), MYLITE_SQL_AST_IDENTIFIER, "lpad column");
    failures +=
        expect_literal(child_at(expression, 1U), MYLITE_SQL_AST_LITERAL_INTEGER, "lpad length");
    failures += expect_literal(child_at(expression, 2U), MYLITE_SQL_AST_LITERAL_STRING, "lpad pad");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_RPAD_FUNCTION, "rpad function");
    failures += expect_operator(
        child_at(expression, 1U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "rpad signed length"
    );
    expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_REPEAT_FUNCTION, "repeat function");
    failures += expect_child_count(expression, 2U, "repeat arity");
    expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_SPACE_FUNCTION, "space function");
    failures += expect_child_count(expression, 1U, "space arity");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "padding from table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT LPAD ('hi', 4, '?'), RPAD ('hi', 4, '?'), REPEAT ('x', 2), SPACE (2) "
        "FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_LPAD_FUNCTION, "spaced lpad");
    failures += expect_span_text(expression, "LPAD ('hi', 4, '?')", "spaced lpad span");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_RPAD_FUNCTION, "spaced rpad");
    expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_REPEAT_FUNCTION, "spaced repeat");
    expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_SPACE_FUNCTION, "spaced space");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT LPAD();", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_LPAD_ARGUMENT_COUNT_ERROR,
        "lpad zero argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT LPAD('a', 1);", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_LPAD_ARGUMENT_COUNT_ERROR,
        "lpad two argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT LPAD('a', 1, '0', 'x');", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_LPAD_ARGUMENT_COUNT_ERROR,
        "lpad many argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT RPAD('a');", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_RPAD_ARGUMENT_COUNT_ERROR,
        "rpad one argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT SPACE();", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_SPACE_ARGUMENT_COUNT_ERROR,
        "space zero argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT SPACE(1, 2);", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_SPACE_ARGUMENT_COUNT_ERROR,
        "space many argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT REPEAT('a');", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    failures += parse_sql("SELECT REPEAT('a', 1, 2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);

    failures += parse_sql(
        "DO LPAD('abc', 5, '0'), RPAD('abc', 5, '0'), REPEAT('x', 2), SPACE(2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "padding do");
    failures += expect_node(child_at(expression_list, 0U), MYLITE_SQL_AST_LPAD_FUNCTION, "do lpad");
    failures += expect_node(child_at(expression_list, 1U), MYLITE_SQL_AST_RPAD_FUNCTION, "do rpad");
    failures +=
        expect_node(child_at(expression_list, 2U), MYLITE_SQL_AST_REPEAT_FUNCTION, "do repeat");
    failures +=
        expect_node(child_at(expression_list, 3U), MYLITE_SQL_AST_SPACE_FUNCTION, "do space");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE padding_words (lpad INT, rpad INT, space INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "padding identifiers");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_string_search_functions(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT LOCATE('bar', s), LOCATE('bar', s, +2), INSTR(s, 'bar'), "
        "POSITION('bar' IN s) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_LOCATE_FUNCTION, "locate function");
    failures += expect_child_count(expression, 2U, "locate arity");
    failures += expect_span_text(expression, "LOCATE('bar', s)", "locate span");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_LOCATE_FUNCTION, "locate position");
    failures += expect_child_count(expression, 3U, "locate position arity");
    failures += expect_operator(
        child_at(expression, 2U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "locate signed position"
    );
    expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_INSTR_FUNCTION, "instr function");
    failures += expect_child_count(expression, 2U, "instr arity");
    expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_POSITION_FUNCTION, "position function");
    failures += expect_child_count(expression, 2U, "position arity");
    failures += expect_span_text(expression, "POSITION('bar' IN s)", "position span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT LOCATE ('a','abc'), INSTR ('abc','a') FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_node(
        child_at(child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_LOCATE_FUNCTION,
        "spaced locate"
    );
    failures += expect_node(
        child_at(child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_INSTR_FUNCTION,
        "spaced instr"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT LOCATE();", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_LOCATE_ARGUMENT_COUNT_ERROR,
        "locate zero argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT LOCATE('a');", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_LOCATE_ARGUMENT_COUNT_ERROR,
        "locate one argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT LOCATE('a','abc',1,2);", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_LOCATE_ARGUMENT_COUNT_ERROR,
        "locate many argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT INSTR();", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_INSTR_ARGUMENT_COUNT_ERROR,
        "instr zero argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT INSTR('abc');", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_INSTR_ARGUMENT_COUNT_ERROR,
        "instr one argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT INSTR('abc','a','x');", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_INSTR_ARGUMENT_COUNT_ERROR,
        "instr many argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT POSITION ('a' IN 'abc');", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);

    failures += parse_sql(
        "DO LOCATE('a','abc'), INSTR('abc','a'), POSITION('a' IN 'abc');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "string search do");
    failures +=
        expect_node(child_at(expression_list, 0U), MYLITE_SQL_AST_LOCATE_FUNCTION, "do locate");
    failures +=
        expect_node(child_at(expression_list, 1U), MYLITE_SQL_AST_INSTR_FUNCTION, "do instr");
    failures +=
        expect_node(child_at(expression_list, 2U), MYLITE_SQL_AST_POSITION_FUNCTION, "do position");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE string_search_words (locate INT, instr INT, position INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "search identifiers");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_find_in_set_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT FIND_IN_SET('b', 'a,b'), find_in_set(v, 'x,y') AS pos FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures +=
        expect_node(expression, MYLITE_SQL_AST_FIND_IN_SET_FUNCTION, "find_in_set function");
    failures += expect_child_count(expression, 2U, "find_in_set arity");
    failures += expect_span_text(expression, "FIND_IN_SET('b', 'a,b')", "find_in_set span");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_FIND_IN_SET_FUNCTION, "lower find_in_set");
    failures += expect_node(child_at(expression, 0U), MYLITE_SQL_AST_IDENTIFIER, "find column");
    failures += expect_node(
        child_at(child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "find alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT FIND_IN_SET();", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_FIND_IN_SET_ARGUMENT_COUNT_ERROR,
        "find_in_set zero argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT FIND_IN_SET('a');", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_FIND_IN_SET_ARGUMENT_COUNT_ERROR,
        "find_in_set one argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT FIND_IN_SET('a','a','extra');", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_FIND_IN_SET_ARGUMENT_COUNT_ERROR,
        "find_in_set many argument error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DO FIND_IN_SET('x', 'a,x');", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "find_in_set do");
    failures += expect_node(
        child_at(expression_list, 0U),
        MYLITE_SQL_AST_FIND_IN_SET_FUNCTION,
        "do find_in_set"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM t WHERE FIND_IN_SET('red', tags);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    where_clause = child_at(statement, 2U);
    predicate = child_at(where_clause, 0U);
    failures += expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "find where");
    failures += expect_node(predicate, MYLITE_SQL_AST_FIND_IN_SET_FUNCTION, "find truth predicate");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM t WHERE FIND_IN_SET('red', tags) > 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    where_clause = child_at(child_at(result.root, 0U), 2U);
    predicate = child_at(where_clause, 0U);
    failures +=
        expect_node(predicate, MYLITE_SQL_AST_COMPARISON_PREDICATE, "find comparison predicate");
    failures += expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_GREATER, "find comparison op");
    failures += expect_node(
        child_at(predicate, 0U),
        MYLITE_SQL_AST_FIND_IN_SET_FUNCTION,
        "find comparison lhs"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM t WHERE FIND_IN_SET('red', tags) IS NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    where_clause = child_at(child_at(result.root, 0U), 2U);
    predicate = child_at(where_clause, 0U);
    failures += expect_node(predicate, MYLITE_SQL_AST_IS_NULL_PREDICATE, "find is null");
    failures +=
        expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL, "find is not null op");
    failures += expect_node(
        child_at(predicate, 0U),
        MYLITE_SQL_AST_FIND_IN_SET_FUNCTION,
        "find is null lhs"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE t SET note = 'hit' WHERE FIND_IN_SET('red', tags);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(result.root, 0U),
        MYLITE_SQL_AST_UPDATE_STATEMENT,
        "find update predicate"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "DELETE FROM t WHERE FIND_IN_SET('blue', tags) > 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(result.root, 0U),
        MYLITE_SQL_AST_DELETE_STATEMENT,
        "find delete predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_regexp_like_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT REGEXP_LIKE('abc', '^a'), regexp_like(v, '^ab', 'c') AS hit FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures +=
        expect_node(expression, MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION, "regexp_like function");
    failures += expect_child_count(expression, 2U, "regexp_like two arguments");
    failures += expect_span_text(expression, "REGEXP_LIKE('abc', '^a')", "regexp_like span");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION, "lower regexp_like");
    failures += expect_child_count(expression, 3U, "regexp_like three arguments");
    failures += expect_node(child_at(expression, 0U), MYLITE_SQL_AST_IDENTIFIER, "regexp column");
    failures += expect_node(
        child_at(child_at(select_list, 1U), 1U),
        MYLITE_SQL_AST_IDENTIFIER,
        "regexp_like alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT REGEXP_LIKE();", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_REGEXP_LIKE_ARGUMENT_COUNT_ERROR,
        "regexp_like zero argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT REGEXP_LIKE('a');", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_REGEXP_LIKE_ARGUMENT_COUNT_ERROR,
        "regexp_like one argument error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT REGEXP_LIKE('a','a','i','extra');", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U),
        MYLITE_SQL_AST_REGEXP_LIKE_ARGUMENT_COUNT_ERROR,
        "regexp_like many argument error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DO REGEXP_LIKE('abc', '^a');", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "regexp_like do");
    failures += expect_node(
        child_at(expression_list, 0U),
        MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION,
        "do regexp_like"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM t WHERE REGEXP_LIKE(v, '^ab');", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    where_clause = child_at(statement, 2U);
    predicate = child_at(where_clause, 0U);
    failures += expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "regexp_like where");
    failures +=
        expect_node(predicate, MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION, "regexp_like truth predicate");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM t WHERE REGEXP_LIKE(v, '^ab') <=> TRUE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    where_clause = child_at(child_at(result.root, 0U), 2U);
    predicate = child_at(where_clause, 0U);
    failures += expect_node(
        predicate,
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "regexp_like comparison predicate"
    );
    failures +=
        expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL, "regexp_like op");
    failures += expect_node(
        child_at(predicate, 0U),
        MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION,
        "regexp_like comparison lhs"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM t WHERE REGEXP_LIKE(v, '^ab') IS NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    where_clause = child_at(child_at(result.root, 0U), 2U);
    predicate = child_at(where_clause, 0U);
    failures += expect_node(predicate, MYLITE_SQL_AST_IS_NULL_PREDICATE, "regexp_like is null");
    failures +=
        expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL, "regexp_like is null op");
    failures += expect_node(
        child_at(predicate, 0U),
        MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION,
        "regexp_like is null lhs"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE t SET note = 'hit' WHERE REGEXP_LIKE(v, '^rss_.+$');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(result.root, 0U),
        MYLITE_SQL_AST_UPDATE_STATEMENT,
        "regexp_like update predicate"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "DELETE FROM t WHERE REGEXP_LIKE(v, '^rss_.+$') = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(result.root, 0U),
        MYLITE_SQL_AST_DELETE_STATEMENT,
        "regexp_like delete predicate"
    );
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

    failures += parse_sql("SELECT PI(), Pi(), pi() FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_PI_FUNCTION, "pi function");
    failures += expect_span_text(first_expression, "PI()", "pi function span");
    failures += expect_child_count(first_expression, 0U, "pi argument count");
    failures += expect_node(second_expression, MYLITE_SQL_AST_PI_FUNCTION, "mixed pi function");
    failures += expect_span_text(second_expression, "Pi()", "mixed pi span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_PI_FUNCTION, "lower pi function");
    failures += expect_span_text(third_expression, "pi()", "lower pi span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "pi from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT PI (), (PI());", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_PI_FUNCTION, "spaced pi function");
    failures += expect_span_text(first_expression, "PI ()", "spaced pi span");
    failures +=
        expect_node(parenthesized, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "parenthesized pi");
    failures += expect_node(child_at(parenthesized, 0U), MYLITE_SQL_AST_PI_FUNCTION, "wrapped pi");
    failures += expect_span_text(parenthesized, "(PI())", "parenthesized pi span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT PI(1), PI(NULL), PI(1, 2);", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_PI_ARGUMENT_COUNT_ERROR,
        "pi integer argument error"
    );
    arguments = child_at(first_expression, 0U);
    failures += expect_node(arguments, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, "pi argument list");
    failures += expect_child_count(arguments, 1U, "pi one argument count");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_PI_ARGUMENT_COUNT_ERROR,
        "pi null argument error"
    );
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_PI_ARGUMENT_COUNT_ERROR,
        "pi multiple argument error"
    );
    arguments = child_at(third_expression, 0U);
    failures += expect_child_count(arguments, 2U, "pi multiple argument count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE pi (pi INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "pi identifier table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT PI;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare pi identifier");
    failures += expect_span_text(first_expression, "PI", "bare pi span");
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

    failures += parse_sql("SELECT RAND(), Rand(), rand() FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_RAND_FUNCTION, "rand function");
    failures += expect_span_text(first_expression, "RAND()", "rand function span");
    failures += expect_child_count(first_expression, 0U, "rand argument count");
    failures += expect_node(second_expression, MYLITE_SQL_AST_RAND_FUNCTION, "mixed rand function");
    failures += expect_span_text(second_expression, "Rand()", "mixed rand span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_RAND_FUNCTION, "lower rand function");
    failures += expect_span_text(third_expression, "rand()", "lower rand span");
    failures += expect_node(child_at(statement, 1U), MYLITE_SQL_AST_FROM_DUAL, "rand from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT RAND (), (RAND());", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_RAND_FUNCTION, "spaced rand");
    failures += expect_span_text(first_expression, "RAND ()", "spaced rand span");
    failures += expect_node(parenthesized, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "wrapped rand");
    failures +=
        expect_node(child_at(parenthesized, 0U), MYLITE_SQL_AST_RAND_FUNCTION, "rand child");
    failures += expect_span_text(parenthesized, "(RAND())", "parenthesized rand span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT RAND(1), RAND(1, 2);", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_RAND_SEED_FUNCTION, "rand seed function");
    failures += expect_child_count(first_expression, 1U, "rand seed argument count");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR,
        "rand argument count error"
    );
    failures += expect_child_count(second_expression, 1U, "rand count marker child count");
    failures += expect_child_count(child_at(second_expression, 0U), 1U, "rand extra arguments");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DO RAND(), RAND(1);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    first_expression = child_at(child_at(statement, 0U), 0U);
    second_expression = child_at(child_at(statement, 0U), 1U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_RAND_FUNCTION, "do rand function");
    failures += expect_node(second_expression, MYLITE_SQL_AST_RAND_SEED_FUNCTION, "do rand seed");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE rand (rand INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "rand identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT RAND;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare rand identifier");
    failures += expect_span_text(first_expression, "RAND", "bare rand span");
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

    failures +=
        parse_sql("SELECT SQRT(4), Sqrt(9), sqrt(16) FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_SQRT_FUNCTION, "sqrt function");
    failures += expect_span_text(first_expression, "SQRT(4)", "sqrt function span");
    failures += expect_child_count(first_expression, 1U, "sqrt argument count");
    failures +=
        expect_node(child_at(first_expression, 0U), MYLITE_SQL_AST_LITERAL, "sqrt argument");
    failures += expect_node(second_expression, MYLITE_SQL_AST_SQRT_FUNCTION, "mixed sqrt function");
    failures += expect_span_text(second_expression, "Sqrt(9)", "mixed sqrt span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_SQRT_FUNCTION, "lower sqrt function");
    failures += expect_span_text(third_expression, "sqrt(16)", "lower sqrt span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "sqrt from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SQRT (20), (SQRT(NULL));", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_SQRT_FUNCTION, "spaced sqrt function");
    failures += expect_span_text(first_expression, "SQRT (20)", "spaced sqrt span");
    failures +=
        expect_node(parenthesized, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "parenthesized sqrt");
    failures +=
        expect_node(child_at(parenthesized, 0U), MYLITE_SQL_AST_SQRT_FUNCTION, "wrapped sqrt");
    failures += expect_span_text(parenthesized, "(SQRT(NULL))", "parenthesized sqrt span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SQRT(), SQRT(1, 2);", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_SQRT_ARGUMENT_COUNT_ERROR,
        "sqrt empty argument error"
    );
    failures += expect_child_count(first_expression, 0U, "sqrt empty error child count");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_SQRT_ARGUMENT_COUNT_ERROR,
        "sqrt multiple argument error"
    );
    arguments = child_at(second_expression, 0U);
    failures += expect_child_count(arguments, 1U, "sqrt multiple argument count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE sqrt (sqrt INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "sqrt identifier table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SQRT;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare sqrt identifier");
    failures += expect_span_text(first_expression, "SQRT", "bare sqrt span");
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

    failures += parse_sql(
        "SELECT DEGREES(1), Degrees(2), radians(180) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_DEGREES_FUNCTION, "degrees function");
    failures += expect_span_text(first_expression, "DEGREES(1)", "degrees function span");
    failures += expect_child_count(first_expression, 1U, "degrees argument count");
    failures +=
        expect_node(child_at(first_expression, 0U), MYLITE_SQL_AST_LITERAL, "degrees argument");
    failures +=
        expect_node(second_expression, MYLITE_SQL_AST_DEGREES_FUNCTION, "mixed degrees function");
    failures += expect_span_text(second_expression, "Degrees(2)", "mixed degrees span");
    failures +=
        expect_node(third_expression, MYLITE_SQL_AST_RADIANS_FUNCTION, "lower radians function");
    failures += expect_span_text(third_expression, "radians(180)", "lower radians span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "angle from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DEGREES (2), (RADIANS(NULL));", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_DEGREES_FUNCTION, "spaced degrees");
    failures += expect_span_text(first_expression, "DEGREES (2)", "spaced degrees span");
    failures +=
        expect_node(parenthesized, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "wrapped radians");
    failures +=
        expect_node(child_at(parenthesized, 0U), MYLITE_SQL_AST_RADIANS_FUNCTION, "radians child");
    failures += expect_span_text(parenthesized, "(RADIANS(NULL))", "parenthesized radians span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT DEGREES(), DEGREES(1, 2), RADIANS(), RADIANS(1, 2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_DEGREES_ARGUMENT_COUNT_ERROR,
        "degrees empty argument error"
    );
    failures += expect_child_count(first_expression, 0U, "degrees empty error child count");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_DEGREES_ARGUMENT_COUNT_ERROR,
        "degrees multiple argument error"
    );
    arguments = child_at(second_expression, 0U);
    failures += expect_child_count(arguments, 1U, "degrees multiple argument count");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_RADIANS_ARGUMENT_COUNT_ERROR,
        "radians empty argument error"
    );
    failures += expect_child_count(third_expression, 0U, "radians empty error child count");
    failures += expect_node(
        child_at(child_at(select_list, 3U), 0U),
        MYLITE_SQL_AST_RADIANS_ARGUMENT_COUNT_ERROR,
        "radians multiple argument error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE degrees (radians INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "angle identifiers");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DEGREES, RADIANS;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare degrees");
    failures += expect_span_text(first_expression, "DEGREES", "bare degrees span");
    failures += expect_node(second_expression, MYLITE_SQL_AST_IDENTIFIER, "bare radians");
    failures += expect_span_text(second_expression, "RADIANS", "bare radians span");
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

    failures +=
        parse_sql("SELECT ACOS(1), Acos(0), asin(-1) FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_ACOS_FUNCTION, "acos function");
    failures += expect_span_text(first_expression, "ACOS(1)", "acos function span");
    failures += expect_child_count(first_expression, 1U, "acos argument count");
    failures += expect_node(child_at(first_expression, 0U), MYLITE_SQL_AST_LITERAL, "acos arg");
    failures += expect_node(second_expression, MYLITE_SQL_AST_ACOS_FUNCTION, "mixed acos");
    failures += expect_span_text(second_expression, "Acos(0)", "mixed acos span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_ASIN_FUNCTION, "lower asin");
    failures += expect_span_text(third_expression, "asin(-1)", "lower asin span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "inverse trig dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ACOS (0), (ASIN(NULL));", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_ACOS_FUNCTION, "spaced acos");
    failures += expect_span_text(first_expression, "ACOS (0)", "spaced acos span");
    failures += expect_node(parenthesized, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "wrapped asin");
    failures +=
        expect_node(child_at(parenthesized, 0U), MYLITE_SQL_AST_ASIN_FUNCTION, "asin child");
    failures += expect_span_text(parenthesized, "(ASIN(NULL))", "parenthesized asin span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ACOS(1) AS a, ASIN(0) s;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_ACOS_FUNCTION, "aliased acos");
    failures += expect_span_text(child_at(child_at(select_list, 0U), 1U), "a", "acos alias");
    failures += expect_node(second_expression, MYLITE_SQL_AST_ASIN_FUNCTION, "aliased asin");
    failures += expect_span_text(child_at(child_at(select_list, 1U), 1U), "s", "asin alias");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT ACOS(), ACOS(1, 2), ASIN(), ASIN(1, 2);", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_ACOS_ARGUMENT_COUNT_ERROR,
        "acos empty argument error"
    );
    failures += expect_child_count(first_expression, 0U, "acos empty error child count");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_ACOS_ARGUMENT_COUNT_ERROR,
        "acos multiple argument error"
    );
    arguments = child_at(second_expression, 0U);
    failures += expect_child_count(arguments, 1U, "acos multiple argument count");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_ASIN_ARGUMENT_COUNT_ERROR,
        "asin empty argument error"
    );
    failures += expect_child_count(third_expression, 0U, "asin empty error child count");
    failures += expect_node(
        child_at(child_at(select_list, 3U), 0U),
        MYLITE_SQL_AST_ASIN_ARGUMENT_COUNT_ERROR,
        "asin multiple argument error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE acos (asin INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "inverse trig ids");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ACOS, ASIN;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare acos");
    failures += expect_span_text(first_expression, "ACOS", "bare acos span");
    failures += expect_node(second_expression, MYLITE_SQL_AST_IDENTIFIER, "bare asin");
    failures += expect_span_text(second_expression, "ASIN", "bare asin span");
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

    failures += parse_sql(
        "SELECT SIN(1), Cos(0), tan(-1), COT(2) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    fourth_expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_SIN_FUNCTION, "sin function");
    failures += expect_span_text(first_expression, "SIN(1)", "sin function span");
    failures += expect_child_count(first_expression, 1U, "sin argument count");
    failures += expect_node(child_at(first_expression, 0U), MYLITE_SQL_AST_LITERAL, "sin arg");
    failures += expect_node(second_expression, MYLITE_SQL_AST_COS_FUNCTION, "mixed cos");
    failures += expect_span_text(second_expression, "Cos(0)", "mixed cos span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_TAN_FUNCTION, "lower tan");
    failures += expect_span_text(third_expression, "tan(-1)", "lower tan span");
    failures += expect_node(fourth_expression, MYLITE_SQL_AST_COT_FUNCTION, "cot function");
    failures += expect_span_text(fourth_expression, "COT(2)", "cot function span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "direct trig dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SIN (0), (COT(NULL));", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_SIN_FUNCTION, "spaced sin");
    failures += expect_span_text(first_expression, "SIN (0)", "spaced sin span");
    failures += expect_node(parenthesized, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "wrapped cot");
    failures += expect_node(child_at(parenthesized, 0U), MYLITE_SQL_AST_COT_FUNCTION, "cot child");
    failures += expect_span_text(parenthesized, "(COT(NULL))", "parenthesized cot span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SIN(1) AS s, COS(0) c;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_SIN_FUNCTION, "aliased sin");
    failures += expect_span_text(child_at(child_at(select_list, 0U), 1U), "s", "sin alias");
    failures += expect_node(second_expression, MYLITE_SQL_AST_COS_FUNCTION, "aliased cos");
    failures += expect_span_text(child_at(child_at(select_list, 1U), 1U), "c", "cos alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT SIN(), SIN(1, 2), COS(), TAN(1, 2), COT(), COT(1, 2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_node(
        child_at(child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_SIN_ARGUMENT_COUNT_ERROR,
        "sin empty argument error"
    );
    failures += expect_node(
        child_at(child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_SIN_ARGUMENT_COUNT_ERROR,
        "sin multiple argument error"
    );
    arguments = child_at(child_at(child_at(select_list, 1U), 0U), 0U);
    failures += expect_child_count(arguments, 1U, "sin multiple argument count");
    failures += expect_node(
        child_at(child_at(select_list, 2U), 0U),
        MYLITE_SQL_AST_COS_ARGUMENT_COUNT_ERROR,
        "cos empty argument error"
    );
    failures += expect_node(
        child_at(child_at(select_list, 3U), 0U),
        MYLITE_SQL_AST_TAN_ARGUMENT_COUNT_ERROR,
        "tan multiple argument error"
    );
    failures += expect_node(
        child_at(child_at(select_list, 4U), 0U),
        MYLITE_SQL_AST_COT_ARGUMENT_COUNT_ERROR,
        "cot empty argument error"
    );
    failures += expect_node(
        child_at(child_at(select_list, cot_multiple_argument_select_item_index), 0U),
        MYLITE_SQL_AST_COT_ARGUMENT_COUNT_ERROR,
        "cot multiple argument error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DO SIN(1), COS(NULL), TAN(TRUE), COT(2);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_DO_STATEMENT, "direct trig do");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE sin (cos INT, tan INT, cot INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "direct trig ids");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SIN, COS, TAN, COT;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    fourth_expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare sin");
    failures += expect_span_text(first_expression, "SIN", "bare sin span");
    failures += expect_node(second_expression, MYLITE_SQL_AST_IDENTIFIER, "bare cos");
    failures += expect_span_text(second_expression, "COS", "bare cos span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_IDENTIFIER, "bare tan");
    failures += expect_span_text(third_expression, "TAN", "bare tan span");
    failures += expect_node(fourth_expression, MYLITE_SQL_AST_IDENTIFIER, "bare cot");
    failures += expect_span_text(fourth_expression, "COT", "bare cot span");
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

    failures += parse_sql(
        "SELECT ATAN(1), Atan(0, -1), atan2(-1), ATAN2(1, -1) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    fourth_expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_ATAN_FUNCTION, "atan function");
    failures += expect_span_text(first_expression, "ATAN(1)", "atan function span");
    failures += expect_child_count(first_expression, 1U, "atan one-argument count");
    failures += expect_node(child_at(first_expression, 0U), MYLITE_SQL_AST_LITERAL, "atan arg");
    failures += expect_node(second_expression, MYLITE_SQL_AST_ATAN_FUNCTION, "mixed atan");
    failures += expect_span_text(second_expression, "Atan(0, -1)", "mixed atan span");
    failures += expect_child_count(second_expression, 2U, "atan two-argument count");
    failures += expect_node(third_expression, MYLITE_SQL_AST_ATAN2_FUNCTION, "lower atan2");
    failures += expect_span_text(third_expression, "atan2(-1)", "lower atan2 span");
    failures += expect_child_count(third_expression, 1U, "atan2 one-argument count");
    failures += expect_node(fourth_expression, MYLITE_SQL_AST_ATAN2_FUNCTION, "atan2 two args");
    failures += expect_child_count(fourth_expression, 2U, "atan2 two-argument count");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "atan dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ATAN (0), (ATAN2(NULL, 1));", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_ATAN_FUNCTION, "spaced atan");
    failures += expect_span_text(first_expression, "ATAN (0)", "spaced atan span");
    failures +=
        expect_node(parenthesized, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "wrapped atan2");
    failures +=
        expect_node(child_at(parenthesized, 0U), MYLITE_SQL_AST_ATAN2_FUNCTION, "atan2 child");
    failures += expect_span_text(parenthesized, "(ATAN2(NULL, 1))", "parenthesized atan2 span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ATAN(1) AS a, ATAN2(1,-1) t;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_ATAN_FUNCTION, "aliased atan");
    failures += expect_span_text(child_at(child_at(select_list, 0U), 1U), "a", "atan alias");
    failures += expect_node(second_expression, MYLITE_SQL_AST_ATAN2_FUNCTION, "aliased atan2");
    failures += expect_span_text(child_at(child_at(select_list, 1U), 1U), "t", "atan2 alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT ATAN(), ATAN(1, 2, 3), ATAN2(), ATAN2(1, 2, 3);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    fourth_expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_ATAN_ARGUMENT_COUNT_ERROR,
        "atan empty argument error"
    );
    failures += expect_child_count(first_expression, 0U, "atan empty error child count");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_ATAN_ARGUMENT_COUNT_ERROR,
        "atan multiple argument error"
    );
    arguments = child_at(second_expression, 0U);
    failures += expect_child_count(arguments, 1U, "atan multiple argument count");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_ATAN2_ARGUMENT_COUNT_ERROR,
        "atan2 empty argument error"
    );
    failures += expect_child_count(third_expression, 0U, "atan2 empty error child count");
    failures += expect_node(
        fourth_expression,
        MYLITE_SQL_AST_ATAN2_ARGUMENT_COUNT_ERROR,
        "atan2 multiple argument error"
    );
    arguments = child_at(fourth_expression, 0U);
    failures += expect_child_count(arguments, 1U, "atan2 multiple argument count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE atan (atan2 INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "atan ids");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ATAN, ATAN2;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare atan");
    failures += expect_span_text(first_expression, "ATAN", "bare atan span");
    failures += expect_node(second_expression, MYLITE_SQL_AST_IDENTIFIER, "bare atan2");
    failures += expect_span_text(second_expression, "ATAN2", "bare atan2 span");
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

    failures += parse_sql(
        "SELECT EXP(1), Ln(2), log(10, 100), LOG10(100), LOG2(8), pow(2,3), "
        "POWER(3,2) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    fourth_expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_EXP_FUNCTION, "exp function");
    failures += expect_span_text(first_expression, "EXP(1)", "exp span");
    failures += expect_child_count(first_expression, 1U, "exp child count");
    failures += expect_node(second_expression, MYLITE_SQL_AST_LN_FUNCTION, "mixed ln function");
    failures += expect_span_text(second_expression, "Ln(2)", "mixed ln span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_LOG_FUNCTION, "two-argument log");
    failures += expect_child_count(third_expression, 2U, "two-argument log child count");
    failures += expect_node(fourth_expression, MYLITE_SQL_AST_LOG10_FUNCTION, "log10 function");
    failures += expect_node(
        child_at(child_at(select_list, 4U), 0U),
        MYLITE_SQL_AST_LOG2_FUNCTION,
        "log2 function"
    );
    failures += expect_node(
        child_at(child_at(select_list, exp_log_power_pow_index), 0U),
        MYLITE_SQL_AST_POW_FUNCTION,
        "pow function"
    );
    failures += expect_node(
        child_at(child_at(select_list, exp_log_power_power_index), 0U),
        MYLITE_SQL_AST_POWER_FUNCTION,
        "power function"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "log power dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT EXP (1), (POWER(NULL, 2));", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_EXP_FUNCTION, "spaced exp");
    failures += expect_span_text(first_expression, "EXP (1)", "spaced exp span");
    failures +=
        expect_node(parenthesized, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "wrapped power");
    failures +=
        expect_node(child_at(parenthesized, 0U), MYLITE_SQL_AST_POWER_FUNCTION, "power child");
    failures += expect_span_text(parenthesized, "(POWER(NULL, 2))", "wrapped power span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT EXP(), EXP(1, 2), LN(), LOG10(1, 2), LOG2(), POW(), POW(2), "
        "POWER(2, 3, 4);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_EXP_ARGUMENT_COUNT_ERROR,
        "exp empty argument error"
    );
    failures += expect_child_count(first_expression, 0U, "exp empty child count");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_EXP_ARGUMENT_COUNT_ERROR,
        "exp multiple argument error"
    );
    arguments = child_at(second_expression, 0U);
    failures += expect_child_count(arguments, 1U, "exp extra arguments");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_LN_ARGUMENT_COUNT_ERROR,
        "ln empty argument error"
    );
    failures += expect_node(
        child_at(child_at(select_list, 3U), 0U),
        MYLITE_SQL_AST_LOG10_ARGUMENT_COUNT_ERROR,
        "log10 multiple argument error"
    );
    failures += expect_node(
        child_at(child_at(select_list, 4U), 0U),
        MYLITE_SQL_AST_LOG2_ARGUMENT_COUNT_ERROR,
        "log2 empty argument error"
    );
    failures += expect_node(
        child_at(child_at(select_list, exp_log_power_pow_index), 0U),
        MYLITE_SQL_AST_POW_ARGUMENT_COUNT_ERROR,
        "pow empty argument error"
    );
    failures += expect_node(
        child_at(child_at(select_list, exp_log_power_power_index), 0U),
        MYLITE_SQL_AST_POW_ARGUMENT_COUNT_ERROR,
        "pow missing argument error"
    );
    failures += expect_node(
        child_at(child_at(select_list, exp_log_power_wrong_arity_power_index), 0U),
        MYLITE_SQL_AST_POWER_ARGUMENT_COUNT_ERROR,
        "power multiple argument error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT LOG(), LOG(1,2,3);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE exp (log INT, pow INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "math identifiers");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT EXP, LN, LOG, LOG10, LOG2, POW, POWER;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    for (size_t index = 0U; index < exp_log_power_identifier_count; ++index) {
        failures += expect_node(
            child_at(child_at(select_list, index), 0U),
            MYLITE_SQL_AST_IDENTIFIER,
            "bare exp log power identifier"
        );
    }
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

    failures += parse_sql(
        "SELECT BIN(64), Oct(NULL), bin(~0), oct(1+2) FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    fourth_expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_BIN_FUNCTION, "bin function");
    failures += expect_span_text(first_expression, "BIN(64)", "bin function span");
    failures += expect_child_count(first_expression, 1U, "bin argument count");
    failures += expect_literal(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "bin integer"
    );
    failures += expect_node(second_expression, MYLITE_SQL_AST_OCT_FUNCTION, "mixed oct");
    failures +=
        expect_literal(child_at(second_expression, 0U), MYLITE_SQL_AST_LITERAL_NULL, "oct NULL");
    failures += expect_node(third_expression, MYLITE_SQL_AST_BIN_FUNCTION, "lower bin");
    failures += expect_operator(
        child_at(third_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_BITWISE_NOT,
        "bin bitwise argument"
    );
    failures += expect_node(fourth_expression, MYLITE_SQL_AST_OCT_FUNCTION, "lower oct");
    failures += expect_operator(
        child_at(fourth_expression, 0U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "oct arithmetic argument"
    );
    failures +=
        expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "base conversion from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT BIN (1), (OCT(1)), CONV(1010,2,10), BIN(IFNULL(NULL,7));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    fourth_expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_BIN_FUNCTION, "spaced bin");
    failures += expect_span_text(first_expression, "BIN (1)", "spaced bin span");
    failures +=
        expect_node(parenthesized, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "parenthesized oct");
    failures +=
        expect_node(child_at(parenthesized, 0U), MYLITE_SQL_AST_OCT_FUNCTION, "wrapped oct");
    failures += expect_span_text(parenthesized, "(OCT(1))", "parenthesized oct span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_CONV_FUNCTION, "conv function");
    failures += expect_child_count(third_expression, 3U, "conv argument count");
    failures += expect_literal(
        child_at(third_expression, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "conv value"
    );
    failures += expect_literal(
        child_at(third_expression, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "conv from base"
    );
    failures += expect_literal(
        child_at(third_expression, 2U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "conv to base"
    );
    failures += expect_node(fourth_expression, MYLITE_SQL_AST_BIN_FUNCTION, "nested bin value");
    failures += expect_node(
        child_at(fourth_expression, 0U),
        MYLITE_SQL_AST_IFNULL_FUNCTION,
        "ifnull bin value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT BIN();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_BIN_ARGUMENT_COUNT_ERROR,
        "empty bin argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT BIN(1,2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_BIN_ARGUMENT_COUNT_ERROR,
        "two bin argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT OCT();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_OCT_ARGUMENT_COUNT_ERROR,
        "empty oct argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT OCT(1,2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_OCT_ARGUMENT_COUNT_ERROR,
        "two oct argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CONV();", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR,
        "empty conv argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CONV(1);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR,
        "one conv argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CONV(1,10);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR,
        "two conv argument count error"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CONV(1,10,2,3);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR,
        "four conv argument count error"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE bin (oct INT, bin INT, conv INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures +=
        expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "base conversion identifiers");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_case_operator(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *searched = NULL;
    const struct mylite_sql_ast_node *simple = NULL;
    const struct mylite_sql_ast_node *when_list = NULL;
    const struct mylite_sql_ast_node *when_clause = NULL;
    const struct mylite_sql_ast_node *else_clause = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT CASE WHEN 1 THEN 2 WHEN 0 THEN 3 ELSE 4 END, "
        "CASE 1 WHEN 1 THEN 10 WHEN 2 THEN 20 ELSE 30 END FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    searched = child_at(child_at(select_list, 0U), 0U);
    simple = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(searched, MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION, "searched case");
    failures += expect_span_text(
        searched,
        "CASE WHEN 1 THEN 2 WHEN 0 THEN 3 ELSE 4 END",
        "searched case span"
    );
    failures += expect_child_count(searched, 2U, "searched case child count");
    when_list = child_at(searched, 0U);
    else_clause = child_at(searched, 1U);
    failures += expect_node(when_list, MYLITE_SQL_AST_CASE_WHEN_LIST, "searched when list");
    failures += expect_child_count(when_list, 2U, "searched when count");
    when_clause = child_at(when_list, 0U);
    failures += expect_node(when_clause, MYLITE_SQL_AST_CASE_WHEN_CLAUSE, "searched when clause");
    failures += expect_span_text(when_clause, "WHEN 1 THEN 2", "searched when span");
    failures += expect_literal(
        child_at(when_clause, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "searched condition"
    );
    failures += expect_literal(
        child_at(when_clause, 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "searched result"
    );
    failures += expect_node(else_clause, MYLITE_SQL_AST_CASE_ELSE_CLAUSE, "searched else");
    failures += expect_span_text(else_clause, "ELSE 4", "searched else span");

    failures += expect_node(simple, MYLITE_SQL_AST_SIMPLE_CASE_EXPRESSION, "simple case");
    failures += expect_child_count(simple, 3U, "simple case child count");
    failures += expect_literal(child_at(simple, 0U), MYLITE_SQL_AST_LITERAL_INTEGER, "case value");
    when_list = child_at(simple, 1U);
    failures += expect_node(when_list, MYLITE_SQL_AST_CASE_WHEN_LIST, "simple when list");
    failures += expect_child_count(when_list, 2U, "simple when count");
    failures += expect_node(child_at(simple, 2U), MYLITE_SQL_AST_CASE_ELSE_CLAUSE, "simple else");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "case from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT (CASE WHEN 1 THEN CASE WHEN 0 THEN 2 ELSE 3 END END) AS chosen;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    parenthesized = child_at(child_at(select_list, 0U), 0U);
    searched = child_at(parenthesized, 0U);
    failures += expect_node(parenthesized, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "wrapped case");
    failures += expect_node(searched, MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION, "outer nested case");
    failures += expect_child_count(searched, 1U, "no-else searched case child count");
    when_clause = child_at(child_at(searched, 0U), 0U);
    failures += expect_node(
        child_at(when_clause, 1U),
        MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION,
        "inner nested case"
    );
    failures += expect_span_text(
        parenthesized,
        "(CASE WHEN 1 THEN CASE WHEN 0 THEN 2 ELSE 3 END END)",
        "wrapped case span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT CASE WHEN 1 THEN 2 END CASE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE end (end INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_span_text(child_at(select, 0U), "end", "end table identifier");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_do_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *expression_list = NULL;
    int failures = 0;

    failures +=
        parse_sql("DO 1, NULL, IF(1,2,3), CASE WHEN 1 THEN 4 END;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "do statement");
    failures += expect_span_text(
        statement,
        "DO 1, NULL, IF(1,2,3), CASE WHEN 1 THEN 4 END",
        "do statement span"
    );
    failures +=
        expect_node(expression_list, MYLITE_SQL_AST_DO_EXPRESSION_LIST, "do expression list");
    failures += expect_child_count(expression_list, 4U, "do expression count");
    failures +=
        expect_literal(child_at(expression_list, 0U), MYLITE_SQL_AST_LITERAL_INTEGER, "do int");
    failures +=
        expect_literal(child_at(expression_list, 1U), MYLITE_SQL_AST_LITERAL_NULL, "do null");
    failures += expect_node(child_at(expression_list, 2U), MYLITE_SQL_AST_IF_FUNCTION, "do if");
    failures += expect_node(
        child_at(expression_list, 3U),
        MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION,
        "do case"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("do +1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    expression_list = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DO_STATEMENT, "lowercase do statement");
    failures += expect_node(
        child_at(expression_list, 0U),
        MYLITE_SQL_AST_UNARY_EXPRESSION,
        "signed do expression"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DO;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("DO 1 FROM DUAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("DO 1 AS x;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE do (do INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(child_at(statement, 0U), "do", "do table identifier");
    failures += expect_span_text(
        child_at(child_at(child_at(statement, 1U), 0U), 0U),
        "do",
        "do column identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_version_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT VERSION(), Version(), version() FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_VERSION_FUNCTION, "version function");
    failures += expect_span_text(first_expression, "VERSION()", "version function span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_VERSION_FUNCTION,
        "mixed-case version function"
    );
    failures += expect_span_text(second_expression, "Version()", "mixed-case version span");
    failures +=
        expect_node(third_expression, MYLITE_SQL_AST_VERSION_FUNCTION, "lower version function");
    failures += expect_span_text(third_expression, "version()", "lower version span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "version from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT VERSION (), (VERSION());", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_VERSION_FUNCTION, "spaced version function");
    failures += expect_span_text(first_expression, "VERSION ()", "spaced version span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized version function"
    );
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_VERSION_FUNCTION,
        "wrapped version function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT VERSION(1), VERSION(NULL), VERSION(1, 2);", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR,
        "version integer argument error"
    );
    failures += expect_span_text(first_expression, "VERSION(1)", "version integer argument span");
    arguments = child_at(first_expression, 0U);
    failures +=
        expect_node(arguments, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, "version argument list");
    failures += expect_child_count(arguments, 1U, "version one argument count");
    failures += expect_literal(
        child_at(arguments, 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "version integer argument"
    );
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR,
        "version null argument error"
    );
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR,
        "version multiple argument error"
    );
    arguments = child_at(third_expression, 0U);
    failures += expect_child_count(arguments, 2U, "version multiple argument count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE version (version INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures +=
        expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "version identifier table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT VERSION;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare version identifier");
    failures += expect_span_text(first_expression, "VERSION", "bare version span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT VERSION() LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_connection_id_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT CONNECTION_ID(), Connection_Id(), connection_id() FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CONNECTION_ID_FUNCTION,
        "connection id function"
    );
    failures += expect_span_text(first_expression, "CONNECTION_ID()", "connection id span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_CONNECTION_ID_FUNCTION,
        "mixed-case connection id function"
    );
    failures +=
        expect_span_text(second_expression, "Connection_Id()", "mixed-case connection id span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_CONNECTION_ID_FUNCTION,
        "lower connection id function"
    );
    failures += expect_span_text(third_expression, "connection_id()", "lower connection id span");
    failures +=
        expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "connection id from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT CONNECTION_ID (), CONNECTION_ID/**/(), CONNECTION_ID(/* inside */), "
        "(CONNECTION_ID());",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CONNECTION_ID_FUNCTION,
        "spaced connection id function"
    );
    failures += expect_span_text(first_expression, "CONNECTION_ID ()", "spaced connection id span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_CONNECTION_ID_FUNCTION,
        "comment-before-paren connection id function"
    );
    failures += expect_span_text(
        second_expression,
        "CONNECTION_ID/**/()",
        "comment-before-paren connection id span"
    );
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_CONNECTION_ID_FUNCTION,
        "commented connection id function"
    );
    failures += expect_span_text(
        third_expression,
        "CONNECTION_ID(/* inside */)",
        "commented connection id span"
    );
    failures += expect_node(
        child_at(child_at(select_list, 3U), 0U),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized connection id function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT CONNECTION_ID(1), CONNECTION_ID(NULL), CONNECTION_ID(1, 2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR,
        "connection id integer argument error"
    );
    arguments = child_at(first_expression, 0U);
    failures += expect_child_count(arguments, 1U, "connection id one argument count");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR,
        "connection id null argument error"
    );
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR,
        "connection id multiple argument error"
    );
    arguments = child_at(third_expression, 0U);
    failures += expect_child_count(arguments, 2U, "connection id multiple argument count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE connection_id (connection_id INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "connection id identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CONNECTION_ID;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare connection id identifier");
    failures += expect_span_text(first_expression, "CONNECTION_ID", "bare connection id span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT CONNECTION_ID() LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_row_count_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT ROW_COUNT(), Row_Count(), row_count() FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_ROW_COUNT_FUNCTION, "row count function");
    failures += expect_span_text(first_expression, "ROW_COUNT()", "row count function span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_ROW_COUNT_FUNCTION,
        "mixed-case row count function"
    );
    failures += expect_span_text(second_expression, "Row_Count()", "mixed-case row count span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_ROW_COUNT_FUNCTION,
        "lower row count function"
    );
    failures += expect_span_text(third_expression, "row_count()", "lower row count span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "row count from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ROW_COUNT (), (ROW_COUNT());", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_ROW_COUNT_FUNCTION,
        "spaced row count function"
    );
    failures += expect_span_text(first_expression, "ROW_COUNT ()", "spaced row count span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized row count function"
    );
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_ROW_COUNT_FUNCTION,
        "wrapped row count function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE row_count (row_count INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures +=
        expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "row count identifier table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ROW_COUNT;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare row count identifier");
    failures += expect_span_text(first_expression, "ROW_COUNT", "bare row count span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ROW_COUNT(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT ROW_COUNT(NULL);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT ROW_COUNT(1, 2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT ROW_COUNT() LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_found_rows_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT FOUND_ROWS(), Found_Rows(), found_rows() FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_FOUND_ROWS_FUNCTION, "found rows function");
    failures += expect_span_text(first_expression, "FOUND_ROWS()", "found rows function span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_FOUND_ROWS_FUNCTION,
        "mixed-case found rows function"
    );
    failures += expect_span_text(second_expression, "Found_Rows()", "mixed-case found rows span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_FOUND_ROWS_FUNCTION,
        "lower found rows function"
    );
    failures += expect_span_text(third_expression, "found_rows()", "lower found rows span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "found rows from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT FOUND_ROWS (), (FOUND_ROWS());", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_FOUND_ROWS_FUNCTION,
        "spaced found rows function"
    );
    failures += expect_span_text(first_expression, "FOUND_ROWS ()", "spaced found rows span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized found rows function"
    );
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_FOUND_ROWS_FUNCTION,
        "wrapped found rows function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE found_rows (found_rows INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures +=
        expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "found rows identifier table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT FOUND_ROWS;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare found rows identifier");
    failures += expect_span_text(first_expression, "FOUND_ROWS", "bare found rows span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT FOUND_ROWS(1);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_FOUND_ROWS_ARGUMENT_COUNT_ERROR,
        "found rows one-argument error"
    );
    failures += expect_span_text(first_expression, "FOUND_ROWS(1)", "found rows one-argument span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT FOUND_ROWS(NULL, 2);", MYLITE_SQL_PARSE_OK, &result);
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_FOUND_ROWS_ARGUMENT_COUNT_ERROR,
        "found rows two-argument error"
    );
    failures +=
        expect_span_text(first_expression, "FOUND_ROWS(NULL, 2)", "found rows two-argument span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT FOUND_ROWS() LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_last_insert_id_function(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT LAST_INSERT_ID(), Last_Insert_Id(), last_insert_id() FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION,
        "last insert id function"
    );
    failures +=
        expect_span_text(first_expression, "LAST_INSERT_ID()", "last insert id function span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION,
        "mixed-case last insert id function"
    );
    failures +=
        expect_span_text(second_expression, "Last_Insert_Id()", "mixed-case last insert id span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION,
        "lower last insert id function"
    );
    failures += expect_span_text(third_expression, "last_insert_id()", "lower last insert id span");
    failures +=
        expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "last insert id from dual");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT LAST_INSERT_ID (), (LAST_INSERT_ID());", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION,
        "spaced last insert id function"
    );
    failures +=
        expect_span_text(first_expression, "LAST_INSERT_ID ()", "spaced last insert id span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized last insert id function"
    );
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION,
        "wrapped last insert id function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE last_insert_id (last_insert_id INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    failures += expect_node(
        select,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "last insert id identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT LAST_INSERT_ID;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare last insert id identifier");
    failures += expect_span_text(first_expression, "LAST_INSERT_ID", "bare last insert id span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT LAST_INSERT_ID(1), LAST_INSERT_ID(NULL);", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_LAST_INSERT_ID_SET_FUNCTION,
        "last insert id set function"
    );
    failures += expect_span_text(first_expression, "LAST_INSERT_ID(1)", "last insert id set span");
    failures +=
        expect_node(child_at(first_expression, 0U), MYLITE_SQL_AST_LITERAL, "last insert id arg");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_LAST_INSERT_ID_SET_FUNCTION,
        "last insert id null set function"
    );
    failures +=
        expect_span_text(second_expression, "LAST_INSERT_ID(NULL)", "last insert id null set span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT LAST_INSERT_ID(1, 2);", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_LAST_INSERT_ID_ARGUMENT_COUNT_ERROR,
        "last insert id argument count error"
    );
    failures +=
        expect_span_text(first_expression, "LAST_INSERT_ID(1, 2)", "last insert id arity span");
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT LAST_INSERT_ID() LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_diagnostics_count_system_variables(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT @@warning_count, @@session.error_count, @@local.Warning_Count FROM DUAL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_SYSTEM_VARIABLE, "warning count variable");
    failures += expect_span_text(first_expression, "@@warning_count", "warning count span");
    failures +=
        expect_node(second_expression, MYLITE_SQL_AST_SYSTEM_VARIABLE, "error count variable");
    failures += expect_span_text(second_expression, "@@session.error_count", "error count span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_SYSTEM_VARIABLE,
        "local warning count variable"
    );
    failures +=
        expect_span_text(third_expression, "@@local.Warning_Count", "local warning count span");
    failures +=
        expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "system variable from dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT (@@warning_count), @@session.`warning_count`, @@`error_count`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized warning count variable"
    );
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_SYSTEM_VARIABLE,
        "wrapped warning count variable"
    );
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_SYSTEM_VARIABLE,
        "quoted warning count variable"
    );
    failures += expect_span_text(
        second_expression,
        "@@session.`warning_count`",
        "quoted warning count span"
    );
    failures += expect_node(third_expression, MYLITE_SQL_AST_SYSTEM_VARIABLE, "quoted error count");
    failures += expect_span_text(third_expression, "@@`error_count`", "quoted error count span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT @warning_count;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_USER_VARIABLE, "user variable");
    failures += expect_span_text(first_expression, "@warning_count", "user variable span");
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT ?;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_count_star_aggregate(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    int failures = 0;

    failures +=
        parse_sql("SELECT COUNT(*), count(*), Count( * ) FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_COUNT_STAR_FUNCTION, "count star function");
    failures += expect_span_text(first_expression, "COUNT(*)", "count star function span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "lower count star function"
    );
    failures += expect_span_text(second_expression, "count(*)", "lower count star span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "mixed count star function"
    );
    failures += expect_span_text(third_expression, "Count( * )", "mixed count star span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "count from dual");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT COUNT(/* inside */*) FROM t WHERE id = 1;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "commented count star function"
    );
    failures +=
        expect_span_text(first_expression, "COUNT(/* inside */*)", "commented count star span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "count from table");
    failures += expect_node(child_at(select, 2U), MYLITE_SQL_AST_WHERE_CLAUSE, "count where");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT COUNT(id), count(n), Count( nn ) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION,
        "count column function"
    );
    failures += expect_span_text(first_expression, "COUNT(id)", "count column span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION,
        "lower count column function"
    );
    failures += expect_span_text(second_expression, "count(n)", "lower count column span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION,
        "mixed count column function"
    );
    failures += expect_span_text(third_expression, "Count( nn )", "mixed count column span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "count column table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT COUNT(/* inside */n), COUNT(`weird name`) FROM t WHERE id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION,
        "commented count column function"
    );
    failures +=
        expect_span_text(first_expression, "COUNT(/* inside */n)", "commented count column span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION,
        "quoted count column function"
    );
    failures +=
        expect_span_text(second_expression, "COUNT(`weird name`)", "quoted count column span");
    failures +=
        expect_node(child_at(select, 2U), MYLITE_SQL_AST_WHERE_CLAUSE, "count column where");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT COUNT(t.n), COUNT(db.t.n), COUNT(DISTINCT t.nn) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified count argument"
    );
    failures += expect_span_text(first_expression, "COUNT(t.n)", "qualified count span");
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "schema-qualified count argument"
    );
    failures += expect_span_text(second_expression, "COUNT(db.t.n)", "schema count span");
    failures += expect_node(
        child_at(third_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified count distinct argument"
    );
    failures +=
        expect_span_text(third_expression, "COUNT(DISTINCT t.nn)", "qualified distinct span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT COUNT(1), count(-1), Count( +1 ) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "count integer literal function"
    );
    failures += expect_span_text(first_expression, "COUNT(1)", "count integer literal span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "lower count negative literal function"
    );
    failures += expect_span_text(second_expression, "count(-1)", "lower count negative span");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "mixed count positive literal function"
    );
    failures += expect_span_text(third_expression, "Count( +1 )", "mixed count positive span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "count literal table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT COUNT(TRUE), count(false), Count( TRUE ) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "count true literal function"
    );
    failures += expect_span_text(first_expression, "COUNT(TRUE)", "count true literal span");
    failures += expect_literal(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "count true literal argument"
    );
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "lower count false literal function"
    );
    failures += expect_span_text(second_expression, "count(false)", "lower count false span");
    failures += expect_literal(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "count false literal argument"
    );
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "mixed count true literal function"
    );
    failures += expect_span_text(third_expression, "Count( TRUE )", "mixed count true span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "count boolean table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT COUNT(/* inside */NULL) FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "commented count null literal function"
    );
    failures += expect_span_text(
        first_expression,
        "COUNT(/* inside */NULL)",
        "commented count null literal span"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "count literal dual");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT COUNT(/* inside */FALSE) FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION,
        "commented count false literal function"
    );
    failures += expect_span_text(
        first_expression,
        "COUNT(/* inside */FALSE)",
        "commented count false literal span"
    );
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_DUAL, "count boolean dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT COUNT(DISTINCT n), count(distinct `weird name`), Count( DISTINCT nn ) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION,
        "count distinct column function"
    );
    failures +=
        expect_span_text(first_expression, "COUNT(DISTINCT n)", "count distinct column span");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION,
        "lower count distinct quoted function"
    );
    failures += expect_span_text(
        second_expression,
        "count(distinct `weird name`)",
        "lower count distinct quoted span"
    );
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION,
        "mixed count distinct column function"
    );
    failures += expect_span_text(
        third_expression,
        "Count( DISTINCT nn )",
        "mixed count distinct column span"
    );
    failures +=
        expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "count distinct table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT COUNT(/* inside */DISTINCT n), COUNT(DISTINCT/* inside */n) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION,
        "commented count distinct keyword function"
    );
    failures += expect_span_text(
        first_expression,
        "COUNT(/* inside */DISTINCT n)",
        "commented count distinct keyword span"
    );
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION,
        "commented count distinct argument function"
    );
    failures += expect_span_text(
        second_expression,
        "COUNT(DISTINCT/* inside */n)",
        "commented count distinct argument span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT (COUNT(n));", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized count column function"
    );
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION,
        "wrapped count column function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT (COUNT(DISTINCT n));", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized count distinct column function"
    );
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION,
        "wrapped count distinct column function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT COUNT(*), COUNT(id);", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_COUNT_STAR_FUNCTION, "mixed count star");
    failures +=
        expect_node(second_expression, MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION, "mixed count column");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT (COUNT(*));", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized count star function"
    );
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "wrapped count star function"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE count (count INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures +=
        expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "count identifier table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT COUNT;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare count identifier");
    failures += expect_span_text(first_expression, "COUNT", "bare count span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT COUNT (*) FROM DUAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT/**/(*) FROM DUAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT(1.0);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT('x');", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT(+TRUE);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT(-FALSE);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT(NOT TRUE);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT(id + 1) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT(TRUE + 1) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT(id, n) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT COUNT(t.*) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT COUNT (DISTINCT id) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT COUNT/**/(DISTINCT id) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT COUNT(DISTINCT *) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT COUNT(DISTINCT id, n) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT COUNT(DISTINCT 1) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT COUNT(DISTINCT TRUE) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT COUNT(DISTINCT id + 1) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_min_max_aggregate(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    int failures = 0;

    failures += parse_sql("SELECT MIN(id), max(n), Max( n ) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION, "min aggregate");
    failures += expect_span_text(first_expression, "MIN(id)", "min aggregate span");
    failures += expect_node(child_at(first_expression, 0U), MYLITE_SQL_AST_IDENTIFIER, "min arg");
    failures += expect_span_text(child_at(first_expression, 0U), "id", "min arg span");
    failures += expect_node(second_expression, MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION, "lower max");
    failures += expect_span_text(second_expression, "max(n)", "lower max span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION, "mixed max");
    failures += expect_span_text(third_expression, "Max( n )", "mixed max span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "min max from table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT MAX(/* inside */n) FROM t WHERE id = 1;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION,
        "commented max aggregate"
    );
    failures += expect_span_text(first_expression, "MAX(/* inside */n)", "commented max span");
    failures += expect_span_text(child_at(first_expression, 0U), "n", "commented max arg span");
    failures += expect_node(child_at(select, 2U), MYLITE_SQL_AST_WHERE_CLAUSE, "max where");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT MIN(t.id), MAX(db.t.n) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified min argument"
    );
    failures += expect_span_text(first_expression, "MIN(t.id)", "qualified min span");
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "schema-qualified max argument"
    );
    failures += expect_span_text(second_expression, "MAX(db.t.n)", "schema max span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT (MIN(id));", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized min aggregate"
    );
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION,
        "wrapped min aggregate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE min (max INT, min INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures +=
        expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "min max identifier table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT MIN, MAX;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare min identifier");
    failures += expect_span_text(first_expression, "MIN", "bare min span");
    failures += expect_node(second_expression, MYLITE_SQL_AST_IDENTIFIER, "bare max identifier");
    failures += expect_span_text(second_expression, "MAX", "bare max span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT MIN (id) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT MAX/**/(id) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT MIN();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT MAX(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT MIN(NULL);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT MIN(id, n) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT MIN(*) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT MIN(DISTINCT id) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_sum_aggregate(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    int failures = 0;

    failures += parse_sql("SELECT SUM(id), sum(n), Sum( n ) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION, "sum aggregate");
    failures += expect_span_text(first_expression, "SUM(id)", "sum aggregate span");
    failures += expect_node(child_at(first_expression, 0U), MYLITE_SQL_AST_IDENTIFIER, "sum arg");
    failures += expect_span_text(child_at(first_expression, 0U), "id", "sum arg span");
    failures += expect_node(second_expression, MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION, "lower sum");
    failures += expect_span_text(second_expression, "sum(n)", "lower sum span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION, "mixed sum");
    failures += expect_span_text(third_expression, "Sum( n )", "mixed sum span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "sum from table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT SUM(/* inside */n) FROM t WHERE id = 1;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION,
        "commented sum aggregate"
    );
    failures += expect_span_text(first_expression, "SUM(/* inside */n)", "commented sum span");
    failures += expect_span_text(child_at(first_expression, 0U), "n", "commented sum arg span");
    failures += expect_node(child_at(select, 2U), MYLITE_SQL_AST_WHERE_CLAUSE, "sum where");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SUM(t.id), SUM(db.t.n) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified sum argument"
    );
    failures += expect_span_text(first_expression, "SUM(t.id)", "qualified sum span");
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "schema-qualified sum argument"
    );
    failures += expect_span_text(second_expression, "SUM(db.t.n)", "schema sum span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT (SUM(id));", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized sum aggregate"
    );
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION,
        "wrapped sum aggregate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE sum (sum INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "sum identifier table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SUM;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare sum identifier");
    failures += expect_span_text(first_expression, "SUM", "bare sum span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SUM (id) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT SUM/**/(id) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT SUM();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT SUM(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT SUM(NULL);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT SUM(id, n) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT SUM(*) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT SUM(DISTINCT id) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_avg_aggregate(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *fourth_expression = NULL;
    int failures = 0;

    failures += parse_sql("SELECT AVG(id), avg(n), Avg( n ) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION, "avg aggregate");
    failures += expect_span_text(first_expression, "AVG(id)", "avg aggregate span");
    failures += expect_node(child_at(first_expression, 0U), MYLITE_SQL_AST_IDENTIFIER, "avg arg");
    failures += expect_span_text(child_at(first_expression, 0U), "id", "avg arg span");
    failures += expect_node(second_expression, MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION, "lower avg");
    failures += expect_span_text(second_expression, "avg(n)", "lower avg span");
    failures += expect_node(third_expression, MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION, "mixed avg");
    failures += expect_span_text(third_expression, "Avg( n )", "mixed avg span");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "avg from table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT AVG (n), AVG/**/(n), AVG /*x*/ (n), AVG( /*x*/ n ) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    fourth_expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION,
        "avg whitespace aggregate"
    );
    failures += expect_span_text(first_expression, "AVG (n)", "avg whitespace span");
    failures += expect_span_text(second_expression, "AVG/**/(n)", "avg comment span");
    failures += expect_span_text(third_expression, "AVG /*x*/ (n)", "avg spaced comment span");
    failures += expect_span_text(fourth_expression, "AVG( /*x*/ n )", "avg inner comment span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT AVG(t.id), AVG(db.t.n) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified avg argument"
    );
    failures += expect_span_text(first_expression, "AVG(t.id)", "qualified avg span");
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "schema-qualified avg argument"
    );
    failures += expect_span_text(second_expression, "AVG(db.t.n)", "schema avg span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT (AVG(id));", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized avg aggregate"
    );
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION,
        "wrapped avg aggregate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE avg (avg INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "avg identifier table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT AVG;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare avg identifier");
    failures += expect_span_text(first_expression, "AVG", "bare avg span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT AVG();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT AVG(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT AVG(NULL);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT AVG(id, n) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT AVG(*) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT AVG(DISTINCT id) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_bitwise_aggregate(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *fourth_expression = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT BIT_AND(id), bit_or(n), Bit_Xor( n ) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_BIT_AND_AGGREGATE_FUNCTION,
        "bit_and aggregate"
    );
    failures += expect_span_text(first_expression, "BIT_AND(id)", "bit_and aggregate span");
    failures +=
        expect_node(child_at(first_expression, 0U), MYLITE_SQL_AST_IDENTIFIER, "bit_and arg");
    failures += expect_span_text(child_at(first_expression, 0U), "id", "bit_and arg span");
    failures +=
        expect_node(second_expression, MYLITE_SQL_AST_BIT_OR_AGGREGATE_FUNCTION, "lower bit_or");
    failures += expect_span_text(second_expression, "bit_or(n)", "lower bit_or span");
    failures +=
        expect_node(third_expression, MYLITE_SQL_AST_BIT_XOR_AGGREGATE_FUNCTION, "mixed bit_xor");
    failures += expect_span_text(third_expression, "Bit_Xor( n )", "mixed bit_xor span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT BIT_AND(/*x*/n), BIT_OR(t.n), BIT_XOR(db.t.n) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_span_text(first_expression, "BIT_AND(/*x*/n)", "commented bit_and span");
    failures += expect_node(
        child_at(second_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified bit_or argument"
    );
    failures += expect_span_text(second_expression, "BIT_OR(t.n)", "qualified bit_or span");
    failures += expect_node(
        child_at(third_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "schema-qualified bit_xor argument"
    );
    failures += expect_span_text(third_expression, "BIT_XOR(db.t.n)", "schema bit_xor span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT (BIT_AND(id));", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized bit_and aggregate"
    );
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_BIT_AND_AGGREGATE_FUNCTION,
        "wrapped bit_and aggregate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT BIT_AND (n), BIT_OR/**/(n), BIT_XOR /*x*/ (n) FROM t;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("CREATE TABLE bit_and (bit_or INT);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures +=
        expect_node(select, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "bitwise identifier table");
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT BIT_XOR;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    fourth_expression = child_at(child_at(select_list, 0U), 0U);
    failures +=
        expect_node(fourth_expression, MYLITE_SQL_AST_IDENTIFIER, "bare bit_xor identifier");
    failures += expect_span_text(fourth_expression, "BIT_XOR", "bare bit_xor span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT BIT_AND();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT BIT_AND(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT BIT_OR(NULL);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT BIT_XOR(id, n) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT BIT_AND(*) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT BIT_XOR(DISTINCT id) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_group_concat_aggregate(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_expression = NULL;
    const struct mylite_sql_ast_node *second_expression = NULL;
    const struct mylite_sql_ast_node *third_expression = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT GROUP_CONCAT(id), group_concat(name ORDER BY id), "
        "Group_Concat(name ORDER BY id DESC SEPARATOR '|') FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    second_expression = child_at(child_at(select_list, 1U), 0U);
    third_expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION,
        "group_concat aggregate"
    );
    failures += expect_span_text(first_expression, "GROUP_CONCAT(id)", "group_concat span");
    failures +=
        expect_node(child_at(first_expression, 0U), MYLITE_SQL_AST_IDENTIFIER, "group_concat arg");
    failures += expect_node(
        second_expression,
        MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION,
        "lower group_concat aggregate"
    );
    failures +=
        expect_span_text(second_expression, "group_concat(name ORDER BY id)", "ordered span");
    order_clause = child_at(second_expression, 1U);
    failures += expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "group_concat order");
    failures += expect_span_text(child_at(order_clause, 0U), "id", "group_concat order key");
    failures += expect_node(
        third_expression,
        MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION,
        "mixed group_concat aggregate"
    );
    order_clause = child_at(third_expression, 1U);
    failures += expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "desc order clause");
    failures += expect_order_direction(
        child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "desc order direction"
    );
    failures +=
        expect_literal(child_at(third_expression, 2U), MYLITE_SQL_AST_LITERAL_STRING, "separator");
    failures += expect_span_text(
        third_expression,
        "Group_Concat(name ORDER BY id DESC SEPARATOR '|')",
        "desc separator span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT GROUP_CONCAT(t.name ORDER BY t.id ASC SEPARATOR \"\") FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        child_at(first_expression, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified group_concat argument"
    );
    order_clause = child_at(first_expression, 1U);
    failures += expect_node(
        child_at(order_clause, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified group_concat order key"
    );
    failures +=
        expect_literal(child_at(first_expression, 2U), MYLITE_SQL_AST_LITERAL_STRING, "empty sep");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT GROUP_CONCAT (id) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql_with_ignore_space(
        "SELECT GROUP_CONCAT (id ORDER BY id) FROM t;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    first_expression = child_at(child_at(child_at(child_at(result.root, 0U), 0U), 0U), 0U);
    failures += expect_node(
        first_expression,
        MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION,
        "ignore_space group_concat"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT GROUP_CONCAT;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    first_expression = child_at(child_at(select_list, 0U), 0U);
    failures +=
        expect_node(first_expression, MYLITE_SQL_AST_IDENTIFIER, "bare group_concat identifier");
    failures += expect_span_text(first_expression, "GROUP_CONCAT", "bare group_concat span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT GROUP_CONCAT(DISTINCT id) FROM t;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT GROUP_CONCAT(id, n) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT GROUP_CONCAT(id + 1) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT GROUP_CONCAT(id ORDER BY 1) FROM t;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT GROUP_CONCAT(id ORDER BY id, n) FROM t;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT GROUP_CONCAT(id SEPARATOR NULL) FROM t;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT GROUP_CONCAT(id SEPARATOR 1) FROM t;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_unary_and_parenthesized_expression(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *unary = NULL;
    const struct mylite_sql_ast_node *parenthesized = NULL;
    const struct mylite_sql_ast_node *add = NULL;
    int failures = 0;

    failures += parse_sql("SELECT -(1 + 2);", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    unary = child_at(child_at(select_list, 0U), 0U);
    parenthesized = child_at(unary, 0U);
    add = child_at(parenthesized, 0U);

    failures += expect_node(unary, MYLITE_SQL_AST_UNARY_EXPRESSION, "negative expression");
    failures += expect_operator(unary, MYLITE_SQL_AST_OPERATOR_NEGATIVE, "negative expression");
    failures += expect_span_text(unary, "-(1 + 2)", "negative expression");
    failures += expect_node(
        parenthesized,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized expression"
    );
    failures += expect_span_text(parenthesized, "(1 + 2)", "parenthesized expression");
    failures += expect_node(add, MYLITE_SQL_AST_BINARY_EXPRESSION, "parenthesized add");
    failures += expect_operator(add, MYLITE_SQL_AST_OPERATOR_ADD, "parenthesized add");
    failures += expect_span_text(child_at(add, 0U), "1", "parenthesized add left");
    failures += expect_span_text(child_at(add, 1U), "2", "parenthesized add right");

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_literal_categories(void) {
    enum { expected_literal_item_count = 5 };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    int failures = 0;

    failures += parse_sql("SELECT 0xabc, b'10', .25, 1e+3, N'a';", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, expected_literal_item_count, "literal select list");
    failures += expect_literal(
        child_at(child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_LITERAL_HEX,
        "hex literal"
    );
    failures += expect_literal(
        child_at(child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_LITERAL_BIT,
        "bit literal"
    );
    failures += expect_literal(
        child_at(child_at(select_list, 2U), 0U),
        MYLITE_SQL_AST_LITERAL_DECIMAL,
        "decimal literal"
    );
    failures += expect_literal(
        child_at(child_at(select_list, 3U), 0U),
        MYLITE_SQL_AST_LITERAL_FLOAT,
        "float literal"
    );
    failures += expect_literal(
        child_at(child_at(select_list, 4U), 0U),
        MYLITE_SQL_AST_LITERAL_NATIONAL_STRING,
        "national literal"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, 1U, "wildcard select list");
    failures += expect_node(
        child_at(child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "wildcard item"
    );

    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT *;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(child_at(result.root, 0U), 1U, "bare wildcard select");
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, 1U, "bare wildcard select list");
    failures += expect_node(
        child_at(child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "bare wildcard item"
    );

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_qualified_identifier_keyword_part(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *qualified = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    int failures = 0;

    failures += parse_sql("SELECT mydb.select;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    qualified = child_at(child_at(select_list, 0U), 0U);

    failures += expect_node(qualified, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "qualified identifier");
    failures += expect_span_text(child_at(qualified, 0U), "mydb", "qualified left");
    failures += expect_span_text(child_at(qualified, 1U), "select", "qualified right");

    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT mydb. /* comment */ select;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    qualified = child_at(child_at(select_list, 0U), 0U);

    failures += expect_node(
        qualified,
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "commented qualified identifier"
    );
    failures += expect_span_text(child_at(qualified, 0U), "mydb", "commented qualified left");
    failures += expect_span_text(child_at(qualified, 1U), "select", "commented qualified right");

    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE names (names INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(child_at(statement, 0U), "names", "names table identifier");
    columns = child_at(statement, 1U);
    column = child_at(columns, 0U);
    failures += expect_span_text(child_at(column, 0U), "names", "names column identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE ifnull (ifnull INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(child_at(statement, 0U), "ifnull", "ifnull table identifier");
    columns = child_at(statement, 1U);
    column = child_at(columns, 0U);
    failures += expect_span_text(child_at(column, 0U), "ifnull", "ifnull column identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ifnull FROM ifnull;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    failures += expect_span_text(
        child_at(child_at(select_list, 0U), 0U),
        "ifnull",
        "ifnull selected identifier"
    );
    failures += expect_span_text(child_at(child_at(statement, 1U), 0U), "ifnull", "ifnull table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE coalesce (coalesce INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(child_at(statement, 0U), "coalesce", "coalesce table identifier");
    columns = child_at(statement, 1U);
    column = child_at(columns, 0U);
    failures += expect_span_text(child_at(column, 0U), "coalesce", "coalesce column identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT coalesce FROM coalesce;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    failures += expect_span_text(
        child_at(child_at(select_list, 0U), 0U),
        "coalesce",
        "coalesce selected identifier"
    );
    failures +=
        expect_span_text(child_at(child_at(statement, 1U), 0U), "coalesce", "coalesce table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE duplicate (duplicate INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(child_at(statement, 0U), "duplicate", "duplicate table");
    columns = child_at(statement, 1U);
    column = child_at(columns, 0U);
    failures += expect_span_text(child_at(column, 0U), "duplicate", "duplicate column");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT a.n FROM numbers AS a WHERE a.n IS NOT NULL ORDER BY a.id DESC LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    qualified = child_at(child_at(select_list, 0U), 0U);
    failures +=
        expect_node(qualified, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "qualified selected column");
    failures += expect_span_text(qualified, "a.n", "qualified selected span");
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_IS_NULL_PREDICATE,
        "qualified where predicate"
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 3U), 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified order key"
    );

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_table_lifecycle_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *table_names = NULL;
    const struct mylite_sql_ast_node *table_name = NULL;
    const struct mylite_sql_ast_node *rename_pairs = NULL;
    const struct mylite_sql_ast_node *rename_pair = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *table_options = NULL;
    const struct mylite_sql_ast_node *engine_option = NULL;
    const struct mylite_sql_ast_node *charset_option = NULL;
    const struct mylite_sql_ast_node *collation_option = NULL;
    const struct mylite_sql_ast_node *if_not_exists = NULL;
    const struct mylite_sql_ast_node *if_exists = NULL;
    const struct mylite_sql_ast_node *column_type = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE app.simple_lifecycle (id INT, amount BIGINT NOT NULL, "
        "flags INTEGER UNSIGNED NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_name = child_at(statement, 0U);
    columns = child_at(statement, 1U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "create table statement");
    failures +=
        expect_node(table_name, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "create qualified table");
    failures += expect_span_text(child_at(table_name, 0U), "app", "create schema name");
    failures += expect_span_text(child_at(table_name, 1U), "simple_lifecycle", "create table name");
    failures += expect_node(columns, MYLITE_SQL_AST_COLUMN_DEFINITION_LIST, "create column list");
    failures += expect_child_count(columns, 3U, "create column list");

    column = child_at(columns, 0U);
    failures += expect_span_text(child_at(column, 0U), "id", "first column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "first column type"
    );
    failures += expect_integer_display_width(child_at(column, 1U), NULL, "first column width");
    failures += expect_true(child_at(column, 2U) == NULL, "first column default nullability");

    column = child_at(columns, 1U);
    failures += expect_span_text(child_at(column, 0U), "amount", "second column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        0,
        "second column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "second column nullability"
    );

    column = child_at(columns, 2U);
    failures += expect_span_text(child_at(column, 0U), "flags", "third column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        1,
        "third column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "third column nullability"
    );

    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE small_integer_types (ti TINYINT, tiu TINYINT UNSIGNED, "
        "si SMALLINT, siu SMALLINT UNSIGNED, mi MEDIUMINT, miu MEDIUMINT UNSIGNED);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures +=
        expect_child_count(columns, small_integer_column_count, "small integer column list");
    column = child_at(columns, 0U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint column type"
    );
    column = child_at(columns, 1U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        1,
        "tinyint unsigned column type"
    );
    column = child_at(columns, 2U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
        0,
        "smallint column type"
    );
    column = child_at(columns, 3U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
        1,
        "smallint unsigned column type"
    );
    column = child_at(columns, 4U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        0,
        "mediumint column type"
    );
    column = child_at(columns, mediumint_unsigned_column_index);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        1,
        "mediumint unsigned column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE signed_integer_types (ti TINYINT SIGNED, si SMALLINT SIGNED, "
        "mi MEDIUMINT SIGNED, i INT SIGNED, ii INTEGER SIGNED, b BIGINT SIGNED);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures +=
        expect_child_count(columns, signed_integer_column_count, "signed integer column list");
    column = child_at(columns, 0U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint signed column type"
    );
    failures += expect_span_text(child_at(column, 1U), "TINYINT SIGNED", "tinyint signed span");
    column = child_at(columns, 1U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
        0,
        "smallint signed column type"
    );
    column = child_at(columns, 2U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        0,
        "mediumint signed column type"
    );
    column = child_at(columns, 3U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "int signed column type"
    );
    column = child_at(columns, signed_integer_column_index);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "integer signed column type"
    );
    failures += expect_span_text(child_at(column, 1U), "INTEGER SIGNED", "integer signed span");
    column = child_at(columns, signed_bigint_column_index);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        0,
        "bigint signed column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE integer_aliases (i1 INT1, i2 INT2, i3 INT3, i4 INT4, i8 INT8, "
        "i1u INT1 UNSIGNED, i2s INT2 SIGNED, i3u INT3 UNSIGNED, "
        "i4u INT4 UNSIGNED, i8u INT8 UNSIGNED);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures +=
        expect_child_count(columns, alias_integer_column_count, "alias integer column list");
    column = child_at(columns, 0U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "int1 alias column type"
    );
    failures += expect_span_text(child_at(column, 1U), "INT1", "int1 alias span");
    column = child_at(columns, 1U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
        0,
        "int2 alias column type"
    );
    column = child_at(columns, 2U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        0,
        "int3 alias column type"
    );
    column = child_at(columns, 3U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "int4 alias column type"
    );
    column = child_at(columns, 4U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        0,
        "int8 alias column type"
    );
    column = child_at(columns, alias_int1_unsigned_column_index);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        1,
        "int1 unsigned alias column type"
    );
    failures += expect_span_text(child_at(column, 1U), "INT1 UNSIGNED", "int1 unsigned span");
    column = child_at(columns, alias_int2_signed_column_index);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
        0,
        "int2 signed alias column type"
    );
    failures += expect_span_text(child_at(column, 1U), "INT2 SIGNED", "int2 signed span");
    column = child_at(columns, alias_int3_unsigned_column_index);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        1,
        "int3 unsigned alias column type"
    );
    column = child_at(columns, alias_int4_unsigned_column_index);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        1,
        "int4 unsigned alias column type"
    );
    column = child_at(columns, alias_int8_unsigned_column_index);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        1,
        "int8 unsigned alias column type"
    );
    failures += expect_span_text(child_at(column, 1U), "INT8 UNSIGNED", "int8 unsigned span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE display_widths (ti0 TINYINT(0), ti1 TINYINT(1), "
        "ti2 TINYINT(2), si SMALLINT(5), mi MEDIUMINT(9), i INT(11), "
        "ii INTEGER(10), bi BIGINT(20), iu INT(10) UNSIGNED, "
        "tis TINYINT(1) SIGNED, tiu TINYINT(1) UNSIGNED, i1 INT1(1), "
        "i2 INT2(5), i3 INT3(7), i4 INT4(9), i8 INT8(20));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures +=
        expect_child_count(columns, display_width_column_count, "display width column list");

    column = child_at(columns, 0U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint zero display width type"
    );
    failures += expect_integer_display_width(column_type, "0", "tinyint zero display width");
    failures += expect_span_text(column_type, "TINYINT(0)", "tinyint zero display width span");

    column = child_at(columns, 1U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint one display width type"
    );
    failures += expect_integer_display_width(column_type, "1", "tinyint one display width");

    column = child_at(columns, display_width_int_unsigned_column_index);
    column_type = child_at(column, 1U);
    failures +=
        expect_integer_type(column_type, MYLITE_SQL_AST_INTEGER_TYPE_INT, 1, "int width unsigned");
    failures += expect_integer_display_width(column_type, "10", "int unsigned display width");
    failures += expect_span_text(column_type, "INT(10) UNSIGNED", "int unsigned width span");

    column = child_at(columns, display_width_tinyint_signed_column_index);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint width signed"
    );
    failures += expect_integer_display_width(column_type, "1", "tinyint signed display width");
    failures += expect_span_text(column_type, "TINYINT(1) SIGNED", "tinyint signed width span");

    column = child_at(columns, display_width_tinyint_unsigned_column_index);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        1,
        "tinyint width unsigned"
    );
    failures += expect_integer_display_width(column_type, "1", "tinyint unsigned display width");
    failures += expect_span_text(column_type, "TINYINT(1) UNSIGNED", "tinyint unsigned width span");

    column = child_at(columns, display_width_int1_column_index);
    column_type = child_at(column, 1U);
    failures +=
        expect_integer_type(column_type, MYLITE_SQL_AST_INTEGER_TYPE_TINYINT, 0, "int1 width");
    failures += expect_integer_display_width(column_type, "1", "int1 display width");

    column = child_at(columns, display_width_int8_column_index);
    column_type = child_at(column, 1U);
    failures +=
        expect_integer_type(column_type, MYLITE_SQL_AST_INTEGER_TYPE_BIGINT, 0, "int8 width");
    failures += expect_integer_display_width(column_type, "20", "int8 display width");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE bool_aliases (b BOOL, c BOOLEAN, nn BOOL NOT NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, bool_alias_column_count, "bool alias column list");
    column = child_at(columns, 0U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "bool alias column type"
    );
    failures += expect_integer_display_width(column_type, NULL, "bool alias display width");
    failures += expect_integer_bool_alias(column_type, "bool alias marker");
    failures += expect_span_text(column_type, "BOOL", "bool alias span");
    column = child_at(columns, 1U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "boolean alias column type"
    );
    failures += expect_integer_bool_alias(column_type, "boolean alias marker");
    failures += expect_span_text(column_type, "BOOLEAN", "boolean alias span");
    column = child_at(columns, 2U);
    column_type = child_at(column, 1U);
    failures += expect_integer_bool_alias(column_type, "bool not null alias marker");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "bool alias not null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE explicit_default_nulls (a INT DEFAULT NULL, "
        "b BIGINT NULL DEFAULT NULL, c BOOL DEFAULT NULL, nn INT NOT NULL DEFAULT NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, 4U, "explicit default null column list");
    column = child_at(columns, 0U);
    failures += expect_true(
        first_child_kind(column, MYLITE_SQL_AST_NULLABILITY) == NULL,
        "default null omitted nullability"
    );
    failures += expect_node(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "default null marker"
    );
    failures += expect_span_text(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        "DEFAULT NULL",
        "default null span"
    );
    column = child_at(columns, 1U);
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "default null explicit nullability"
    );
    failures += expect_node(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "explicit null default marker"
    );
    column = child_at(columns, 2U);
    failures += expect_integer_bool_alias(child_at(column, 1U), "default null bool marker");
    failures += expect_node(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "bool default null marker"
    );
    column = child_at(columns, 3U);
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "not null default null parser marker"
    );
    failures += expect_node(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "not null default null marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE integer_defaults (a INT DEFAULT 5, b INT DEFAULT +9, "
        "c INT DEFAULT -7, d BOOL DEFAULT TRUE, e BOOLEAN DEFAULT FALSE);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures +=
        expect_child_count(columns, integer_default_column_count, "integer default column list");
    column = child_at(columns, 0U);
    failures += expect_node(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
        MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE,
        "integer default marker"
    );
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "integer default literal"
    );
    failures += expect_span_text(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
        "DEFAULT 5",
        "integer default span"
    );
    column = child_at(columns, 1U);
    failures += expect_operator(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "positive default operator"
    );
    column = child_at(columns, 2U);
    failures += expect_operator(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "negative default operator"
    );
    column = child_at(columns, 3U);
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "true default literal"
    );
    column = child_at(columns, 4U);
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "false default literal"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE bool_identifiers (BOOL INT, BOOLEAN TINYINT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    column = child_at(columns, 0U);
    failures += expect_span_text(child_at(column, 0U), "BOOL", "bool identifier column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "bool identifier column type"
    );
    column = child_at(columns, 1U);
    failures += expect_span_text(child_at(column, 0U), "BOOLEAN", "boolean identifier column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "boolean identifier column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE IF NOT EXISTS app.if_missing (id INT) ENGINE=InnoDB;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_name = child_at(statement, 0U);
    columns = child_at(statement, 1U);
    if_not_exists = child_at(statement, 2U);
    table_options = child_at(statement, 3U);
    failures += expect_child_count(statement, 4U, "create if not exists child count");
    failures += expect_node(
        if_not_exists,
        MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE,
        "create if not exists clause"
    );
    failures += expect_span_text(child_at(table_name, 1U), "if_missing", "if not exists table");
    failures += expect_child_count(columns, 1U, "if not exists column list");
    failures +=
        expect_node(table_options, MYLITE_SQL_AST_TABLE_OPTION_LIST, "if not exists options");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP TABLE IF EXISTS app.if_missing;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    table_names = child_at(statement, 0U);
    table_name = child_at(table_names, 0U);
    if_exists = child_at(statement, 1U);
    failures += expect_child_count(statement, 2U, "drop if exists child count");
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_TABLE_STATEMENT, "drop table statement");
    failures += expect_node(table_names, MYLITE_SQL_AST_TABLE_NAME_LIST, "drop table name list");
    failures += expect_child_count(table_names, 1U, "drop if exists table name count");
    failures +=
        expect_node(table_name, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "drop qualified table");
    failures +=
        expect_node(if_exists, MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE, "drop if exists clause");
    failures += expect_span_text(child_at(table_name, 0U), "app", "drop if exists schema");
    failures += expect_span_text(child_at(table_name, 1U), "if_missing", "drop if exists table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE engine_forms (id INT) ENGINE=InnoDB;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_options = child_at(statement, 2U);
    engine_option = child_at(table_options, 0U);
    failures += expect_child_count(statement, 3U, "engine create child count");
    failures +=
        expect_node(table_options, MYLITE_SQL_AST_TABLE_OPTION_LIST, "create table options");
    failures += expect_child_count(table_options, 1U, "engine option list child count");
    failures += expect_node(
        engine_option,
        MYLITE_SQL_AST_TABLE_ENGINE_OPTION,
        "create table engine option"
    );
    failures += expect_span_text(child_at(engine_option, 0U), "InnoDB", "engine option name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE engine_space (id INT) ENGINE InnoDB;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE engine_string (id INT) ENGINE='InnoDB';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_options = child_at(statement, 2U);
    engine_option = child_at(table_options, 0U);
    failures +=
        expect_literal(child_at(engine_option, 0U), MYLITE_SQL_AST_LITERAL_STRING, "string engine");
    failures += expect_span_text(child_at(engine_option, 0U), "'InnoDB'", "string engine name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE engine_quoted (id INT) ENGINE=`InnoDB`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE charset_options (id INT) DEFAULT CHARSET=utf8mb4 "
        "COLLATE='utf8mb4_0900_ai_ci';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_options = child_at(statement, 2U);
    charset_option = child_at(table_options, 0U);
    collation_option = child_at(table_options, 1U);
    failures += expect_child_count(table_options, 2U, "charset option list child count");
    failures += expect_node(
        charset_option,
        MYLITE_SQL_AST_TABLE_CHARSET_OPTION,
        "create table charset option"
    );
    failures += expect_span_text(child_at(charset_option, 0U), "utf8mb4", "charset option name");
    failures += expect_node(
        collation_option,
        MYLITE_SQL_AST_TABLE_COLLATION_OPTION,
        "create table collation option"
    );
    failures += expect_literal(
        child_at(collation_option, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "string collation"
    );
    failures += expect_span_text(
        child_at(collation_option, 0U),
        "'utf8mb4_0900_ai_ci'",
        "collation option name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE character_set_space (id INT) DEFAULT CHARACTER SET utf8mb4;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE charset_space (id INT) CHARSET utf8mb4;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE default_collate (id INT) DEFAULT COLLATE utf8mb4_0900_ai_ci;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE status (status INT);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE collation (collation INT);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP TABLE app.simple_lifecycle;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    table_names = child_at(statement, 0U);
    table_name = child_at(table_names, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_TABLE_STATEMENT, "drop table statement");
    failures +=
        expect_node(table_names, MYLITE_SQL_AST_TABLE_NAME_LIST, "single drop table name list");
    failures += expect_child_count(table_names, 1U, "single drop table name count");
    failures += expect_span_text(table_name, "app.simple_lifecycle", "drop target");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("DROP TABLE first_table, app.second_table;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    table_names = child_at(statement, 0U);
    table_name = child_at(table_names, 0U);
    failures += expect_child_count(statement, 1U, "multi drop child count");
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_TABLE_STATEMENT, "multi drop statement");
    failures +=
        expect_node(table_names, MYLITE_SQL_AST_TABLE_NAME_LIST, "multi drop table name list");
    failures += expect_child_count(table_names, 2U, "multi drop table name count");
    failures += expect_span_text(table_name, "first_table", "first multi drop target");
    failures +=
        expect_span_text(child_at(table_names, 1U), "app.second_table", "second multi drop target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DROP TABLE IF EXISTS first_table, app.second_table;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_names = child_at(statement, 0U);
    if_exists = child_at(statement, 1U);
    failures += expect_child_count(statement, 2U, "multi drop if exists child count");
    failures += expect_child_count(table_names, 2U, "multi drop if exists table name count");
    failures +=
        expect_node(if_exists, MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE, "multi drop if exists clause");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("TRUNCATE TABLE app.simple_lifecycle;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT, "truncate table statement");
    failures +=
        expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "truncate target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("TRUNCATE simple_lifecycle;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT, "bare truncate statement");
    failures +=
        expect_span_text(child_at(statement, 0U), "simple_lifecycle", "bare truncate target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLES FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_TABLES_STATEMENT, "show tables statement");
    failures += expect_span_text(child_at(statement, 0U), "app", "show tables schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLES IN app;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(result.root, 0U),
        MYLITE_SQL_AST_SHOW_TABLES_STATEMENT,
        "show tables in statement"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLES;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_TABLES_STATEMENT, "bare show tables");
    failures += expect_true(child_at(statement, 0U) == NULL, "bare show has no schema child");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLES LIKE 'a%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_TABLES_STATEMENT, "show tables like");
    failures +=
        expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING, "tables like");
    failures += expect_span_text(child_at(statement, 0U), "'a%'", "show tables like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLES FROM app LIKE 'a\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_TABLES_STATEMENT, "show tables schema like");
    failures += expect_true(
        !mylite_sql_ast_node_show_tables_is_full(statement),
        "show tables schema like is not full"
    );
    failures += expect_span_text(child_at(statement, 0U), "app", "show tables like schema");
    failures +=
        expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING, "schema like");
    failures += expect_span_text(child_at(statement, 1U), "'a\\_%'", "schema like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLE STATUS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT,
        "bare show table status"
    );
    failures +=
        expect_true(child_at(statement, 0U) == NULL, "bare table status has no schema child");
    failures += expect_true(child_at(statement, 1U) == NULL, "bare table status has no like child");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLE STATUS FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT,
        "show table status from"
    );
    failures += expect_span_text(child_at(statement, 0U), "app", "show table status schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLE STATUS IN app LIKE 'a\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT,
        "show table status in like"
    );
    failures += expect_span_text(child_at(statement, 0U), "app", "show table status like schema");
    failures +=
        expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING, "status like");
    failures += expect_span_text(child_at(statement, 1U), "'a\\_%'", "status like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLE STATUS LIKE 'a%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT,
        "show table status like"
    );
    failures +=
        expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING, "status bare like");
    failures += expect_span_text(child_at(statement, 0U), "'a%'", "status bare like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CHARACTER SET;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT, "show character set");
    failures += expect_child_count(statement, 0U, "show character set child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CHARSET LIKE 'utf8%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT, "show charset like");
    failures +=
        expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING, "charset like");
    failures += expect_span_text(child_at(statement, 0U), "'utf8%'", "charset like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLLATION;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT, "show collation");
    failures += expect_child_count(statement, 0U, "show collation child count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW COLLATION LIKE 'utf8mb4\\_0900\\_ai\\_ci';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT, "show collation like");
    failures +=
        expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING, "collation like");
    failures += expect_span_text(
        child_at(statement, 0U),
        "'utf8mb4\\_0900\\_ai\\_ci'",
        "collation like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ENGINES;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT, "show engines");
    failures += expect_child_count(statement, 0U, "show engines child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW STORAGE ENGINES;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT, "show storage engines");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "RENAME TABLE app.simple_lifecycle TO archive.renamed_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_RENAME_TABLE_STATEMENT, "rename table statement");
    rename_pairs = child_at(statement, 0U);
    rename_pair = child_at(rename_pairs, 0U);
    failures += expect_child_count(statement, 1U, "rename table statement");
    failures +=
        expect_node(rename_pairs, MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST, "rename pair list");
    failures += expect_child_count(rename_pairs, 1U, "rename pair list child count");
    failures += expect_node(rename_pair, MYLITE_SQL_AST_RENAME_TABLE_PAIR, "rename pair");
    failures +=
        expect_span_text(child_at(rename_pair, 0U), "app.simple_lifecycle", "rename source");
    failures +=
        expect_span_text(child_at(rename_pair, 1U), "archive.renamed_lifecycle", "rename target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "RENAME TABLE first_old TO first_new, app.second_old TO archive.second_new;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    rename_pairs = child_at(statement, 0U);
    rename_pair = child_at(rename_pairs, 0U);
    failures += expect_child_count(statement, 1U, "multi rename statement");
    failures += expect_child_count(rename_pairs, 2U, "multi rename pair count");
    failures += expect_node(rename_pair, MYLITE_SQL_AST_RENAME_TABLE_PAIR, "first rename pair");
    failures += expect_span_text(child_at(rename_pair, 0U), "first_old", "first rename source");
    failures += expect_span_text(child_at(rename_pair, 1U), "first_new", "first rename target");
    rename_pair = child_at(rename_pairs, 1U);
    failures +=
        expect_span_text(child_at(rename_pair, 0U), "app.second_old", "second rename source");
    failures +=
        expect_span_text(child_at(rename_pair, 1U), "archive.second_new", "second rename target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle RENAME TO archive.renamed_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT,
        "alter table rename to statement"
    );
    failures += expect_child_count(statement, 2U, "alter table rename statement");
    failures +=
        expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "alter rename source");
    failures += expect_span_text(
        child_at(statement, 1U),
        "archive.renamed_lifecycle",
        "alter rename target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle RENAME renamed_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT,
        "alter table bare rename statement"
    );
    failures +=
        expect_span_text(child_at(statement, 1U), "renamed_lifecycle", "bare rename target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle RENAME AS renamed_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT,
        "alter table rename as statement"
    );
    failures += expect_span_text(child_at(statement, 1U), "renamed_lifecycle", "rename as target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle ADD COLUMN added INT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT,
        "alter table add column statement"
    );
    failures += expect_child_count(statement, 2U, "alter add column child count");
    failures +=
        expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "alter add target");
    column = child_at(statement, 1U);
    failures += expect_node(column, MYLITE_SQL_AST_COLUMN_DEFINITION, "alter add column");
    failures += expect_span_text(child_at(column, 0U), "added", "alter added column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "alter added column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "alter added column nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle DROP COLUMN old_col;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT,
        "alter table drop column statement"
    );
    failures += expect_child_count(statement, 2U, "alter drop column child count");
    failures +=
        expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "alter drop target");
    failures += expect_span_text(child_at(statement, 1U), "old_col", "alter dropped column name");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE simple_lifecycle DROP old_col;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT,
        "alter table bare drop column statement"
    );
    failures +=
        expect_span_text(child_at(statement, 0U), "simple_lifecycle", "bare alter drop target");
    failures += expect_span_text(child_at(statement, 1U), "old_col", "bare dropped column name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle RENAME COLUMN old_col TO new_col;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_RENAME_COLUMN_STATEMENT,
        "alter table rename column statement"
    );
    failures += expect_child_count(statement, 3U, "alter rename column child count");
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter rename column target"
    );
    failures +=
        expect_span_text(child_at(statement, 1U), "old_col", "alter rename old column name");
    failures +=
        expect_span_text(child_at(statement, 2U), "new_col", "alter rename new column name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle MODIFY COLUMN old_col BIGINT NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT,
        "alter table modify column statement"
    );
    failures += expect_child_count(statement, 2U, "alter modify column child count");
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter modify column target"
    );
    column = child_at(statement, 1U);
    failures += expect_node(column, MYLITE_SQL_AST_COLUMN_DEFINITION, "alter modify column");
    failures += expect_span_text(child_at(column, 0U), "old_col", "alter modified column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        0,
        "alter modified column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "alter modified column nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle CHANGE COLUMN old_col new_col BIGINT NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT,
        "alter table change column statement"
    );
    failures += expect_child_count(statement, 3U, "alter change column child count");
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter change column target"
    );
    failures += expect_span_text(child_at(statement, 1U), "old_col", "alter change old column");
    column = child_at(statement, 2U);
    failures += expect_node(column, MYLITE_SQL_AST_COLUMN_DEFINITION, "alter changed column");
    failures += expect_span_text(child_at(column, 0U), "new_col", "alter change new column");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        0,
        "alter changed column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "alter changed column nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ADD added BIGINT UNSIGNED NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT,
        "bare alter table add statement"
    );
    failures +=
        expect_span_text(child_at(statement, 0U), "simple_lifecycle", "bare alter add target");
    column = child_at(statement, 1U);
    failures += expect_span_text(child_at(column, 0U), "added", "bare alter add column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        1,
        "bare alter add column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "bare alter add column nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ADD added_small SMALLINT UNSIGNED NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
        1,
        "smallint unsigned alter add column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ADD added_signed TINYINT SIGNED NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint signed alter add column type"
    );
    failures += expect_span_text(child_at(column, 1U), "TINYINT SIGNED", "signed alter add span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ADD added_alias INT1 UNSIGNED NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        1,
        "int1 unsigned alter add column type"
    );
    failures += expect_span_text(child_at(column, 1U), "INT1 UNSIGNED", "alias alter add span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ADD added_width TINYINT(1) NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "display width alter add column type"
    );
    failures += expect_integer_display_width(column_type, "1", "display width alter add");
    failures += expect_span_text(column_type, "TINYINT(1)", "display width alter add span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ADD added_bool BOOL NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "bool alias alter add column type"
    );
    failures += expect_integer_bool_alias(column_type, "bool alias alter add marker");
    failures += expect_span_text(column_type, "BOOL", "bool alias alter add span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ADD added_default INT NULL DEFAULT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_node(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "alter add default null marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col INT UNSIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT,
        "bare alter table modify statement"
    );
    failures +=
        expect_span_text(child_at(statement, 0U), "simple_lifecycle", "bare alter modify target");
    column = child_at(statement, 1U);
    failures += expect_span_text(child_at(column, 0U), "old_col", "bare alter modify column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        1,
        "bare alter modify column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "bare alter modify column nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col TINYINT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint alter modify column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col INT SIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "int signed alter modify column type"
    );
    failures += expect_span_text(child_at(column, 1U), "INT SIGNED", "signed alter modify span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col INT4 SIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "int4 signed alter modify column type"
    );
    failures += expect_span_text(child_at(column, 1U), "INT4 SIGNED", "alias alter modify span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col INT(11) UNSIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        1,
        "display width alter modify column type"
    );
    failures += expect_integer_display_width(column_type, "11", "display width alter modify");
    failures +=
        expect_span_text(column_type, "INT(11) UNSIGNED", "display width alter modify span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col BOOLEAN NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "boolean alias alter modify column type"
    );
    failures += expect_integer_bool_alias(column_type, "boolean alias alter modify marker");
    failures += expect_span_text(column_type, "BOOLEAN", "boolean alias alter modify span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col BIGINT DEFAULT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_node(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "alter modify default null marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_col INT UNSIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT,
        "bare alter table change statement"
    );
    failures +=
        expect_span_text(child_at(statement, 0U), "simple_lifecycle", "bare alter change target");
    failures += expect_span_text(child_at(statement, 1U), "old_col", "bare alter change old name");
    column = child_at(statement, 2U);
    failures += expect_span_text(child_at(column, 0U), "new_col", "bare alter change new name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        1,
        "bare alter change column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "bare alter change column nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_col MEDIUMINT UNSIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 2U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        1,
        "mediumint unsigned alter change column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_col MEDIUMINT SIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 2U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        0,
        "mediumint signed alter change column type"
    );
    failures +=
        expect_span_text(child_at(column, 1U), "MEDIUMINT SIGNED", "signed alter change span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_col INT3 NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 2U);
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        0,
        "int3 alias alter change column type"
    );
    failures += expect_span_text(child_at(column, 1U), "INT3", "alias alter change span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_width INT1(1) NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 2U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "display width alter change column type"
    );
    failures += expect_integer_display_width(column_type, "1", "display width alter change");
    failures += expect_span_text(column_type, "INT1(1)", "display width alter change span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_bool BOOL NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 2U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "bool alias alter change column type"
    );
    failures += expect_integer_bool_alias(column_type, "bool alias alter change marker");
    failures += expect_span_text(column_type, "BOOL", "bool alias alter change span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_default BIGINT DEFAULT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 2U);
    failures += expect_node(
        first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "alter change default null marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle ALTER COLUMN old_col SET DEFAULT +8;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT,
        "alter table set default statement"
    );
    failures += expect_child_count(statement, 3U, "alter set default child count");
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter set default target"
    );
    failures += expect_span_text(child_at(statement, 1U), "old_col", "alter set default column");
    failures += expect_node(
        child_at(statement, 2U),
        MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE,
        "alter set default value node"
    );
    failures += expect_operator(
        child_at(child_at(statement, 2U), 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "alter set default positive value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ALTER old_col SET DEFAULT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT,
        "alter table bare set default statement"
    );
    failures += expect_node(
        child_at(statement, 2U),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "alter set default null node"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle ALTER COLUMN old_col DROP DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT,
        "alter table drop default statement"
    );
    failures += expect_child_count(statement, 2U, "alter drop default child count");
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter drop default target"
    );
    failures += expect_span_text(child_at(statement, 1U), "old_col", "alter drop default column");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ALTER old_col DROP DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT,
        "alter table bare drop default statement"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle ALTER COLUMN old_col SET INVISIBLE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT,
        "alter table column invisible statement"
    );
    failures += expect_child_count(statement, 2U, "alter column invisible child count");
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter column invisible target"
    );
    failures +=
        expect_span_text(child_at(statement, 1U), "old_col", "alter column invisible column");
    failures += expect_column_visibility(
        statement,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_INVISIBLE,
        "alter column invisible payload"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ALTER old_col SET VISIBLE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT,
        "alter table column visible statement"
    );
    failures += expect_column_visibility(
        statement,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE,
        "alter column visible payload"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE visibility_names (visible INT, invisible INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT INTO app.simple_lifecycle (amount, id) VALUES (+1, -2), (NULL, 3);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "insert statement");
    failures += expect_child_count(statement, 3U, "insert statement");
    failures += expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "insert target");
    failures +=
        expect_node(child_at(statement, 1U), MYLITE_SQL_AST_IDENTIFIER_LIST, "insert columns");
    failures += expect_child_count(child_at(statement, 1U), 2U, "insert columns");
    failures += expect_span_text(child_at(child_at(statement, 1U), 0U), "amount", "insert col 1");
    failures += expect_span_text(child_at(child_at(statement, 1U), 1U), "id", "insert col 2");
    failures += expect_node(child_at(statement, 2U), MYLITE_SQL_AST_INSERT_ROW_LIST, "insert rows");
    failures += expect_child_count(child_at(statement, 2U), 2U, "insert rows");
    failures += expect_node(
        child_at(child_at(statement, 2U), 0U),
        MYLITE_SQL_AST_INSERT_ROW,
        "first insert row"
    );
    failures += expect_operator(
        child_at(child_at(child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "positive insert value"
    );
    failures += expect_operator(
        child_at(child_at(child_at(statement, 2U), 0U), 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "negative insert value"
    );
    failures += expect_literal(
        child_at(child_at(child_at(statement, 2U), 1U), 0U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "null insert value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO simple_lifecycle VALUES (1);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "insert without columns");
    failures += expect_node(
        child_at(statement, 1U),
        MYLITE_SQL_AST_IDENTIFIER_LIST,
        "empty insert columns"
    );
    failures += expect_child_count(child_at(statement, 1U), 0U, "insert has no column list");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT INTO simple_lifecycle VALUES (TRUE, false);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_literal(
        child_at(child_at(child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "true insert value"
    );
    failures += expect_literal(
        child_at(child_at(child_at(statement, 2U), 0U), 1U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "false insert value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT INTO app.simple_lifecycle SET id = +1, amount = NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_SET_STATEMENT, "insert set statement");
    failures += expect_child_count(statement, 2U, "insert set statement");
    failures +=
        expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "insert set target");
    failures += expect_node(
        child_at(statement, 1U),
        MYLITE_SQL_AST_INSERT_ASSIGNMENT_LIST,
        "insert set assignments"
    );
    failures += expect_child_count(child_at(statement, 1U), 2U, "insert set assignment count");
    failures += expect_node(
        child_at(child_at(statement, 1U), 0U),
        MYLITE_SQL_AST_INSERT_ASSIGNMENT,
        "insert set first assignment"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(statement, 1U), 0U), 0U),
        "id",
        "insert set first target"
    );
    failures += expect_operator(
        child_at(child_at(child_at(statement, 1U), 0U), 1U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "insert set first value"
    );
    failures += expect_literal(
        child_at(child_at(child_at(statement, 1U), 1U), 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "insert set second value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT INTO app.simple_lifecycle SET id = TRUE, amount = false;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_literal(
        child_at(child_at(child_at(statement, 1U), 0U), 1U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "insert set true value"
    );
    failures += expect_literal(
        child_at(child_at(child_at(statement, 1U), 1U), 1U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "insert set false value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT simple_lifecycle SET id = -1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_SET_STATEMENT, "insert set no into");
    failures += expect_span_text(child_at(statement, 0U), "simple_lifecycle", "no into target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT INTO simple_lifecycle SET simple_lifecycle.id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        child_at(child_at(child_at(statement, 1U), 0U), 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "insert set qualified target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "REPLACE INTO app.simple_lifecycle (amount, id) VALUES (+1, -2), (NULL, TRUE);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT, "replace statement");
    failures += expect_child_count(statement, 3U, "replace statement");
    failures += expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "replace target");
    failures +=
        expect_node(child_at(statement, 1U), MYLITE_SQL_AST_IDENTIFIER_LIST, "replace columns");
    failures += expect_child_count(child_at(statement, 1U), 2U, "replace columns");
    failures += expect_span_text(child_at(child_at(statement, 1U), 0U), "amount", "replace col 1");
    failures += expect_span_text(child_at(child_at(statement, 1U), 1U), "id", "replace col 2");
    failures +=
        expect_node(child_at(statement, 2U), MYLITE_SQL_AST_INSERT_ROW_LIST, "replace rows");
    failures += expect_child_count(child_at(statement, 2U), 2U, "replace rows");
    failures += expect_operator(
        child_at(child_at(child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "positive replace value"
    );
    failures += expect_operator(
        child_at(child_at(child_at(statement, 2U), 0U), 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "negative replace value"
    );
    failures += expect_literal(
        child_at(child_at(child_at(statement, 2U), 1U), 0U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "null replace value"
    );
    failures += expect_literal(
        child_at(child_at(child_at(statement, 2U), 1U), 1U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "true replace value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE simple_lifecycle VALUES (FALSE);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT, "replace without into");
    failures += expect_span_text(child_at(statement, 0U), "simple_lifecycle", "no into replace");
    failures += expect_child_count(child_at(statement, 1U), 0U, "replace has no column list");
    failures += expect_literal(
        child_at(child_at(child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "false replace value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "REPLACE INTO app.simple_lifecycle SET id = +1, amount = NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_REPLACE_SET_STATEMENT, "replace set statement");
    failures += expect_child_count(statement, 2U, "replace set statement");
    failures +=
        expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "replace set target");
    failures += expect_node(
        child_at(statement, 1U),
        MYLITE_SQL_AST_INSERT_ASSIGNMENT_LIST,
        "replace set assignments"
    );
    failures += expect_child_count(child_at(statement, 1U), 2U, "replace set assignment count");
    failures += expect_node(
        child_at(child_at(statement, 1U), 0U),
        MYLITE_SQL_AST_INSERT_ASSIGNMENT,
        "replace set first assignment"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(statement, 1U), 0U), 0U),
        "id",
        "replace set first target"
    );
    failures += expect_operator(
        child_at(child_at(child_at(statement, 1U), 0U), 1U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "replace set first value"
    );
    failures += expect_literal(
        child_at(child_at(child_at(statement, 1U), 1U), 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "replace set second value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "REPLACE simple_lifecycle SET id = TRUE, amount = false;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_REPLACE_SET_STATEMENT, "replace set no into");
    failures += expect_span_text(child_at(statement, 0U), "simple_lifecycle", "replace set target");
    failures += expect_literal(
        child_at(child_at(child_at(statement, 1U), 0U), 1U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "replace set true value"
    );
    failures += expect_literal(
        child_at(child_at(child_at(statement, 1U), 1U), 1U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "replace set false value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "REPLACE INTO simple_lifecycle SET simple_lifecycle.id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        child_at(child_at(child_at(statement, 1U), 0U), 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "replace set qualified target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM app.simple_lifecycle;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SELECT_STATEMENT, "table wildcard select");
    failures +=
        expect_node(child_at(statement, 1U), MYLITE_SQL_AST_FROM_TABLE, "table select from");
    failures += expect_span_text(
        child_at(child_at(statement, 1U), 0U),
        "app.simple_lifecycle",
        "table select target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id, amount FROM simple_lifecycle;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SELECT_STATEMENT, "table projection select");
    failures += expect_node(child_at(statement, 1U), MYLITE_SQL_AST_FROM_TABLE, "projection from");
    failures += expect_span_text(
        child_at(child_at(child_at(statement, 0U), 0U), 0U),
        "id",
        "first projection"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(statement, 0U), 1U), 0U),
        "amount",
        "second projection"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_create_table_comment_option_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *table_options = NULL;
    const struct mylite_sql_ast_node *comment_option = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE combined_options (id INT) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 "
        "COLLATE=utf8mb4_0900_ai_ci COMMENT='metadata';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_options = child_at(statement, 2U);
    failures += expect_child_count(table_options, 4U, "combined option list child count");
    failures += expect_node(
        child_at(table_options, 0U),
        MYLITE_SQL_AST_TABLE_ENGINE_OPTION,
        "combined engine option"
    );
    failures += expect_node(
        child_at(table_options, 1U),
        MYLITE_SQL_AST_TABLE_CHARSET_OPTION,
        "combined charset option"
    );
    failures += expect_node(
        child_at(table_options, 2U),
        MYLITE_SQL_AST_TABLE_COLLATION_OPTION,
        "combined collation option"
    );
    comment_option = child_at(table_options, 3U);
    failures +=
        expect_node(comment_option, MYLITE_SQL_AST_TABLE_COMMENT_OPTION, "combined comment option");
    failures += expect_literal(
        child_at(comment_option, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "combined comment literal"
    );
    failures += expect_span_text(child_at(comment_option, 0U), "'metadata'", "comment literal");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE reverse_options (id INT) COMMENT \"first\" COLLATE=utf8mb4_0900_ai_ci "
        "DEFAULT CHARSET=`utf8mb4` COMMENT='second' ENGINE=InnoDB;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_options = child_at(statement, 2U);
    failures +=
        expect_child_count(table_options, reverse_table_option_count, "reverse option child count");
    failures += expect_node(
        child_at(table_options, 0U),
        MYLITE_SQL_AST_TABLE_COMMENT_OPTION,
        "first reverse comment option"
    );
    failures += expect_span_text(
        child_at(child_at(table_options, 0U), 0U),
        "\"first\"",
        "double quoted comment literal"
    );
    failures += expect_node(
        child_at(table_options, 3U),
        MYLITE_SQL_AST_TABLE_COMMENT_OPTION,
        "second reverse comment option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE comment_identifier (COMMENT INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(
        child_at(child_at(child_at(statement, 1U), 0U), 0U),
        "COMMENT",
        "comment identifier column"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_comment_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *comment_option = NULL;
    int failures = 0;

    failures += parse_sql("ALTER TABLE commented COMMENT='new';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_COMMENT_STATEMENT,
        "alter table comment statement"
    );
    failures += expect_child_count(statement, 2U, "alter table comment child count");
    failures += expect_span_text(child_at(statement, 0U), "commented", "comment target table");
    comment_option = child_at(statement, 1U);
    failures += expect_node(
        comment_option,
        MYLITE_SQL_AST_TABLE_COMMENT_OPTION,
        "alter table comment option"
    );
    failures += expect_literal(
        child_at(comment_option, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "alter table comment literal"
    );
    failures += expect_span_text(child_at(comment_option, 0U), "'new'", "alter comment text");
    failures += expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) ==
            MYLITE_SQL_AST_ALTER_ALGORITHM_UNSPECIFIED,
        "alter comment default algorithm"
    );
    failures += expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_UNSPECIFIED,
        "alter comment default lock"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.commented COMMENT \"new\", ALGORITHM=INPLACE, LOCK=NONE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_COMMENT_STATEMENT,
        "qualified alter table comment statement"
    );
    failures += expect_span_text(child_at(statement, 0U), "app.commented", "qualified target");
    failures += expect_span_text(
        child_at(child_at(statement, 1U), 0U),
        "\"new\"",
        "double quoted alter comment literal"
    );
    failures += expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE,
        "alter comment inplace algorithm"
    );
    failures += expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_NONE,
        "alter comment none lock"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE commented COMMENT=123;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("ALTER TABLE commented COMMENT=NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("ALTER TABLE commented COMMENT=abc;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "ALTER TABLE commented COMMENT='first', COMMENT='second';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_full_tables_statements(void) {
    int failures = 0;
    struct mylite_sql_parse_result result = {0};
    const struct mylite_sql_ast_node *statement = NULL;

    failures += parse_sql("SHOW FULL TABLES;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_TABLES_STATEMENT, "show full tables statement");
    failures += expect_true(
        mylite_sql_ast_node_show_tables_is_full(statement),
        "show full tables has full flag"
    );
    failures += expect_child_count(statement, 0U, "show full tables child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL TABLES FROM app LIKE 'a\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLES_STATEMENT,
        "show full tables schema like"
    );
    failures += expect_true(
        mylite_sql_ast_node_show_tables_is_full(statement),
        "show full tables schema like full flag"
    );
    failures += expect_span_text(child_at(statement, 0U), "app", "show full tables schema");
    failures += expect_literal(
        child_at(statement, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "show full tables like"
    );
    failures += expect_span_text(child_at(statement, 1U), "'a\\_%'", "show full tables like");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL TABLES IN app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_show_tables_is_full(statement),
        "show full tables in full flag"
    );
    failures += expect_span_text(child_at(statement, 0U), "app", "show full tables in schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EXTENDED FULL TABLES;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL EXTENDED TABLES;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW FULL TABLES WHERE Tables_in_app = 'simple_lifecycle';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_table_status_where_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures +=
        parse_sql("SHOW TABLE STATUS WHERE Name = 'numbers';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT,
        "show table status where"
    );
    failures +=
        expect_node(child_at(statement, 0U), MYLITE_SQL_AST_WHERE_CLAUSE, "status bare where");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW TABLE STATUS FROM app WHERE `Rows` = '3';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT,
        "show table status schema where"
    );
    failures += expect_span_text(child_at(statement, 0U), "app", "show table status where schema");
    failures +=
        expect_node(child_at(statement, 1U), MYLITE_SQL_AST_WHERE_CLAUSE, "status schema where");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_table_binary_charset_options(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *table_options = NULL;
    const struct mylite_sql_ast_node *charset_option = NULL;
    const struct mylite_sql_ast_node *collation_option = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE binary_charset_options (id INT) DEFAULT CHARSET=binary COLLATE=binary;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_options = child_at(statement, 2U);
    charset_option = child_at(table_options, 0U);
    collation_option = child_at(table_options, 1U);
    failures += expect_node(
        charset_option,
        MYLITE_SQL_AST_TABLE_CHARSET_OPTION,
        "create table binary charset option"
    );
    failures +=
        expect_span_text(child_at(charset_option, 0U), "binary", "binary charset option name");
    failures += expect_node(
        collation_option,
        MYLITE_SQL_AST_TABLE_COLLATION_OPTION,
        "create table binary collation option"
    );
    failures +=
        expect_span_text(child_at(collation_option, 0U), "binary", "binary collation option name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE binary_collate_only (id INT) COLLATE=binary;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_options = child_at(statement, 2U);
    collation_option = child_at(table_options, 0U);
    failures += expect_node(
        collation_option,
        MYLITE_SQL_AST_TABLE_COLLATION_OPTION,
        "create table binary collate-only option"
    );
    failures += expect_span_text(
        child_at(collation_option, 0U),
        "binary",
        "binary collate-only option name"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_column_position_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *position = NULL;
    int failures = 0;

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle ADD COLUMN added BIGINT FIRST;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT,
        "alter table add column first statement"
    );
    failures += expect_child_count(statement, 3U, "alter add first child count");
    position = child_at(statement, 2U);
    failures +=
        expect_node(position, MYLITE_SQL_AST_COLUMN_POSITION_FIRST, "alter add first position");
    failures += expect_child_count(position, 0U, "alter add first position child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle ADD COLUMN added BIGINT AFTER other_col;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT,
        "alter table add column after statement"
    );
    failures += expect_child_count(statement, 3U, "alter add after child count");
    position = child_at(statement, 2U);
    failures +=
        expect_node(position, MYLITE_SQL_AST_COLUMN_POSITION_AFTER, "alter add after position");
    failures += expect_child_count(position, 1U, "alter add after position child count");
    failures +=
        expect_span_text(child_at(position, 0U), "other_col", "alter add after column name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle MODIFY old_col BIGINT FIRST;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT,
        "alter table modify column first statement"
    );
    failures += expect_child_count(statement, 3U, "alter modify first child count");
    position = child_at(statement, 2U);
    failures +=
        expect_node(position, MYLITE_SQL_AST_COLUMN_POSITION_FIRST, "alter modify first position");
    failures += expect_child_count(position, 0U, "alter modify first position child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle MODIFY old_col BIGINT AFTER other_col;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT,
        "alter table modify column after statement"
    );
    failures += expect_child_count(statement, 3U, "alter modify after child count");
    position = child_at(statement, 2U);
    failures +=
        expect_node(position, MYLITE_SQL_AST_COLUMN_POSITION_AFTER, "alter modify after position");
    failures += expect_child_count(position, 1U, "alter modify after position child count");
    failures +=
        expect_span_text(child_at(position, 0U), "other_col", "alter modify after column name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle CHANGE old_col new_col BIGINT FIRST;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT,
        "alter table change column first statement"
    );
    failures += expect_child_count(statement, 4U, "alter change first child count");
    position = child_at(statement, 3U);
    failures +=
        expect_node(position, MYLITE_SQL_AST_COLUMN_POSITION_FIRST, "alter change first position");
    failures += expect_child_count(position, 0U, "alter change first position child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle CHANGE old_col new_col BIGINT AFTER other_col;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT,
        "alter table change column after statement"
    );
    failures += expect_child_count(statement, 4U, "alter change after child count");
    position = child_at(statement, 3U);
    failures +=
        expect_node(position, MYLITE_SQL_AST_COLUMN_POSITION_AFTER, "alter change after position");
    failures += expect_child_count(position, 1U, "alter change after position child count");
    failures +=
        expect_span_text(child_at(position, 0U), "other_col", "alter change after column name");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_column_position_nonreserved_identifier_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *position = NULL;
    int failures = 0;

    failures +=
        parse_sql("CREATE TABLE first (after INT, first INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_span_text(child_at(statement, 0U), "first", "first table identifier");
    failures +=
        expect_span_text(child_at(child_at(columns, 0U), 0U), "after", "after column identifier");
    failures +=
        expect_span_text(child_at(child_at(columns, 1U), 0U), "first", "first column identifier");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE first ADD first BIGINT AFTER after;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    position = child_at(statement, 2U);
    failures += expect_span_text(
        child_at(child_at(statement, 1U), 0U),
        "first",
        "add first target identifier"
    );
    failures += expect_span_text(child_at(position, 0U), "after", "add after target identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE first MODIFY first BIGINT AFTER after;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    position = child_at(statement, 2U);
    failures += expect_span_text(
        child_at(child_at(statement, 1U), 0U),
        "first",
        "modify first target identifier"
    );
    failures += expect_span_text(child_at(position, 0U), "after", "modify after target identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE first CHANGE after first BIGINT AFTER after;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    position = child_at(statement, 3U);
    failures += expect_span_text(child_at(statement, 1U), "after", "change after old identifier");
    failures += expect_span_text(
        child_at(child_at(statement, 2U), 0U),
        "first",
        "change first new identifier"
    );
    failures += expect_span_text(child_at(position, 0U), "after", "change after target identifier");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_column_charset_collation_attribute_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *charset_option = NULL;
    const struct mylite_sql_ast_node *collation_option = NULL;
    const struct mylite_sql_ast_node *comment_attribute = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE column_charset ("
        "v VARCHAR(10) CHARACTER SET utf8mb4 COLLATE 'utf8mb4_bin', "
        "t TEXT CHARSET `utf8mb4`, "
        "c CHAR(2) COLLATE utf8mb4_unicode_ci NOT NULL"
        ");",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    column = child_at(columns, 0U);
    charset_option = child_at(column, 2U);
    collation_option = child_at(column, 3U);
    failures += expect_node(
        charset_option,
        MYLITE_SQL_AST_COLUMN_CHARSET_ATTRIBUTE,
        "column charset attribute"
    );
    failures += expect_span_text(child_at(charset_option, 0U), "utf8mb4", "column charset name");
    failures += expect_node(
        collation_option,
        MYLITE_SQL_AST_COLUMN_COLLATION_ATTRIBUTE,
        "column collation attribute"
    );
    failures += expect_literal(
        child_at(collation_option, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "column string collation"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE column_binary_charset (v VARCHAR(10) CHARACTER SET binary, "
        "c CHAR(3) COLLATE binary);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    column = child_at(columns, 0U);
    charset_option = child_at(column, 2U);
    failures += expect_span_text(child_at(charset_option, 0U), "binary", "binary charset name");
    column = child_at(columns, 1U);
    collation_option = child_at(column, 2U);
    failures += expect_span_text(child_at(collation_option, 0U), "binary", "binary collation name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE column_comment ("
        "a INT COMMENT 'alpha', "
        "b VARCHAR(5) CHARACTER SET ascii COMMENT 'bee' NOT NULL, "
        "c INT COMMENT 'first' COMMENT 'second'"
        ");",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    column = child_at(columns, 0U);
    comment_attribute = child_at(column, 2U);
    failures += expect_node(
        comment_attribute,
        MYLITE_SQL_AST_COLUMN_COMMENT_ATTRIBUTE,
        "column comment attribute"
    );
    failures += expect_literal(
        child_at(comment_attribute, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "column comment literal"
    );
    failures +=
        expect_span_text(child_at(comment_attribute, 0U), "'alpha'", "column comment literal text");
    column = child_at(columns, 2U);
    failures += expect_node(
        child_at(column, 2U),
        MYLITE_SQL_AST_COLUMN_COMMENT_ATTRIBUTE,
        "first duplicate column comment"
    );
    failures += expect_node(
        child_at(column, 3U),
        MYLITE_SQL_AST_COLUMN_COMMENT_ATTRIBUTE,
        "second duplicate column comment"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE column_comment ADD COLUMN d INT COMMENT 'dee' FIRST;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE column_comment MODIFY COLUMN a BIGINT COMMENT 'modified';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE column_comment CHANGE COLUMN b bb VARCHAR(7) COMMENT 'changed';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE column_comment_equal (a INT COMMENT='x');",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE column_comment_number (a INT COMMENT 123);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE column_comment_null (a INT COMMENT NULL);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE column_comment_before_charset ("
        "v VARCHAR(10) COMMENT 'x' CHARACTER SET utf8mb4);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE column_charset_equal (v VARCHAR(10) CHARACTER SET=utf8mb4);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE column_collation_equal (v VARCHAR(10) COLLATE=utf8mb4_bin);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE column_collation_before_charset ("
        "v VARCHAR(10) COLLATE utf8mb4_bin CHARACTER SET utf8mb4);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE column_charset_after_nullability ("
        "v VARCHAR(10) NOT NULL CHARACTER SET utf8mb4);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE column_charset_after_unique ("
        "v VARCHAR(10) UNIQUE CHARACTER SET utf8mb4);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE column_collation_after_unique_key ("
        "v VARCHAR(10) UNIQUE KEY COLLATE utf8mb4_bin);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_empty_insert_values_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("INSERT INTO simple_lifecycle VALUES ();", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "insert empty row");
    failures += expect_child_count(child_at(statement, 1U), 0U, "empty row omitted columns");
    failures += expect_child_count(child_at(statement, 2U), 1U, "empty row list count");
    failures += expect_child_count(
        child_at(child_at(statement, 2U), 0U),
        0U,
        "empty insert row value count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT INTO simple_lifecycle () VALUES ();", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "insert explicit empty");
    failures += expect_span_text(child_at(statement, 1U), "()", "explicit empty insert columns");
    failures += expect_child_count(child_at(statement, 1U), 0U, "explicit empty column count");
    failures += expect_child_count(
        child_at(child_at(statement, 2U), 0U),
        0U,
        "explicit empty insert row value count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO simple_lifecycle VALUE ();", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "insert value empty");
    failures += expect_child_count(child_at(statement, 2U), 1U, "value empty row list count");
    failures += expect_child_count(
        child_at(child_at(statement, 2U), 0U),
        0U,
        "value empty insert row value count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT INTO simple_lifecycle VALUE (1), (2);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "insert value rows");
    failures += expect_child_count(child_at(statement, 2U), 2U, "value row list count");
    failures +=
        expect_child_count(child_at(child_at(statement, 2U), 0U), 1U, "first value row count");
    failures +=
        expect_child_count(child_at(child_at(statement, 2U), 1U), 1U, "second value row count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT INTO simple_lifecycle VALUES ROW();", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "insert row empty");
    failures += expect_child_count(child_at(statement, 2U), 1U, "row empty row list count");
    failures += expect_child_count(
        child_at(child_at(statement, 2U), 0U),
        0U,
        "row empty insert row value count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT INTO simple_lifecycle VALUES ROW(1), ROW(2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "insert row constructors");
    failures += expect_child_count(child_at(statement, 2U), 2U, "row constructor list count");
    failures += expect_child_count(
        child_at(child_at(statement, 2U), 0U),
        1U,
        "first row constructor count"
    );
    failures += expect_child_count(
        child_at(child_at(statement, 2U), 1U),
        1U,
        "second row constructor count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("REPLACE INTO simple_lifecycle () VALUES (), ();", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT, "replace explicit empty");
    failures += expect_span_text(child_at(statement, 1U), "()", "explicit empty replace columns");
    failures += expect_child_count(child_at(statement, 2U), 2U, "empty replace rows");
    failures +=
        expect_child_count(child_at(child_at(statement, 2U), 0U), 0U, "first empty replace row");
    failures +=
        expect_child_count(child_at(child_at(statement, 2U), 1U), 0U, "second empty replace row");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("REPLACE INTO simple_lifecycle VALUE (1), (2);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT, "replace value rows");
    failures += expect_child_count(child_at(statement, 2U), 2U, "replace value row list count");
    failures += expect_child_count(
        child_at(child_at(statement, 2U), 0U),
        1U,
        "replace first value row count"
    );
    failures += expect_child_count(
        child_at(child_at(statement, 2U), 1U),
        1U,
        "replace second value row count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "REPLACE INTO simple_lifecycle VALUES ROW(), ROW();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT, "replace row constructors");
    failures += expect_child_count(child_at(statement, 2U), 2U, "replace row constructor count");
    failures += expect_child_count(
        child_at(child_at(statement, 2U), 0U),
        0U,
        "replace first empty row constructor"
    );
    failures += expect_child_count(
        child_at(child_at(statement, 2U), 1U),
        0U,
        "replace second empty row constructor"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE value (value INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "value keyword identifier table"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_table_maintenance_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *table_names = NULL;
    int failures = 0;

    failures += parse_sql("ANALYZE TABLE t;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    table_names = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_ANALYZE_TABLE_STATEMENT, "analyze statement");
    failures += expect_child_count(statement, 1U, "analyze child count");
    failures += expect_node(table_names, MYLITE_SQL_AST_TABLE_NAME_LIST, "analyze target list");
    failures += expect_child_count(table_names, 1U, "analyze target count");
    failures += expect_span_text(child_at(table_names, 0U), "t", "analyze target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ANALYZE LOCAL TABLE app.t, other;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    table_names = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_ANALYZE_TABLE_STATEMENT, "analyze local");
    failures += expect_child_count(table_names, 2U, "analyze local target count");
    failures += expect_span_text(child_at(table_names, 0U), "app.t", "analyze qualified target");
    failures += expect_span_text(child_at(table_names, 1U), "other", "analyze second target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ANALYZE NO_WRITE_TO_BINLOG TABLE t;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ANALYZE_TABLE_STATEMENT,
        "analyze no_write_to_binlog"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CHECK TABLE t QUICK FAST MEDIUM EXTENDED CHANGED FOR UPGRADE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_names = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CHECK_TABLE_STATEMENT, "check statement");
    failures += expect_child_count(statement, 1U, "check child count");
    failures += expect_child_count(table_names, 1U, "check target count");
    failures += expect_span_text(child_at(table_names, 0U), "t", "check target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("OPTIMIZE LOCAL TABLE t;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    table_names = child_at(statement, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_OPTIMIZE_TABLE_STATEMENT, "optimize statement");
    failures += expect_span_text(child_at(table_names, 0U), "t", "optimize target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "REPAIR NO_WRITE_TO_BINLOG TABLE app.t QUICK EXTENDED USE_FRM;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_names = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_REPAIR_TABLE_STATEMENT, "repair statement");
    failures += expect_span_text(child_at(table_names, 0U), "app.t", "repair target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ANALYZE TABLE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ANALYZE TABLE t UPDATE HISTOGRAM ON c;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CHECKSUM TABLE t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPAIR TABLE t FOR UPGRADE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_temporary_table_lifecycle_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *table_names = NULL;
    const struct mylite_sql_ast_node *table_name = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *if_not_exists = NULL;
    const struct mylite_sql_ast_node *if_exists = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TEMPORARY TABLE IF NOT EXISTS app.temp_lifecycle ("
        "id INT, note VARCHAR(10), KEY note_idx (note(3)));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_name = child_at(statement, 0U);
    columns = child_at(statement, 1U);
    if_not_exists = child_at(statement, 2U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_STATEMENT,
        "create temporary table statement"
    );
    failures +=
        expect_node(table_name, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "create temporary target");
    failures += expect_span_text(child_at(table_name, 0U), "app", "create temporary schema");
    failures += expect_span_text(child_at(table_name, 1U), "temp_lifecycle", "temporary table");
    failures += expect_child_count(columns, 3U, "temporary create item list");
    failures += expect_node(
        if_not_exists,
        MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE,
        "temporary create if not exists clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DROP TEMPORARY TABLE IF EXISTS first_temp, app.second_temp;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_names = child_at(statement, 0U);
    if_exists = child_at(statement, 1U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_DROP_TEMPORARY_TABLE_STATEMENT,
        "drop temporary table statement"
    );
    failures += expect_child_count(statement, 2U, "drop temporary if exists child count");
    failures += expect_child_count(table_names, 2U, "drop temporary table name count");
    failures += expect_span_text(child_at(table_names, 0U), "first_temp", "first temp drop");
    failures += expect_span_text(child_at(table_names, 1U), "app.second_temp", "second temp drop");
    failures += expect_node(
        if_exists,
        MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE,
        "drop temporary if exists clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE temporary (temporary INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    table_name = child_at(statement, 0U);
    columns = child_at(statement, 1U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "temporary nonreserved create table statement"
    );
    failures += expect_span_text(table_name, "temporary", "temporary nonreserved table name");
    failures += expect_span_text(
        child_at(child_at(columns, 0U), 0U),
        "temporary",
        "temporary nonreserved column name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT temporary FROM temporary;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SELECT_STATEMENT, "temporary nonreserved select");
    failures += expect_span_text(
        child_at(child_at(child_at(statement, 0U), 0U), 0U),
        "temporary",
        "temporary nonreserved projection"
    );
    failures += expect_span_text(
        child_at(child_at(statement, 1U), 0U),
        "temporary",
        "temporary nonreserved source"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_serial_alias_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *column_type = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE serial_aliases (id SERIAL, nn SERIAL NOT NULL, nullable SERIAL NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, 3U, "serial alias column list");
    column = child_at(columns, 0U);
    column_type = child_at(column, 1U);
    failures += expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        1,
        "serial alias column type"
    );
    failures += expect_integer_display_width(column_type, NULL, "serial alias display width");
    failures += expect_integer_serial_alias(column_type, "serial alias marker");
    failures += expect_span_text(column_type, "SERIAL", "serial alias span");
    column = child_at(columns, 1U);
    column_type = child_at(column, 1U);
    failures += expect_integer_serial_alias(column_type, "serial not null alias marker");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "serial alias not null"
    );
    column = child_at(columns, 2U);
    column_type = child_at(column, 1U);
    failures += expect_integer_serial_alias(column_type, "serial null alias marker");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "serial alias null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE serial_identifiers (SERIAL INT, serial BIGINT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    column = child_at(columns, 0U);
    failures += expect_span_text(child_at(column, 0U), "SERIAL", "serial identifier column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "serial identifier column type"
    );
    column = child_at(columns, 1U);
    failures += expect_span_text(child_at(column, 0U), "serial", "lowercase serial identifier");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        0,
        "lowercase serial identifier column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_serial_default_value (c SERIAL DEFAULT VALUE);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_varchar_type_statements(void) {
    enum {
        national_varchar_column_count = 6U,
        national_varchar_last_column_index = 5U,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *column_type = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE string_types (v0 VARCHAR(0), label VARCHAR(255) NOT NULL DEFAULT 'tag', "
        "alias CHARACTER VARYING(3), short_alias CHAR VARYING(4));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, 4U, "varchar column list");
    column = child_at(columns, 0U);
    column_type = child_at(column, 1U);
    failures += expect_span_text(child_at(column, 0U), "v0", "varchar zero column name");
    failures += expect_varchar_type(column_type, "0", "varchar zero column type");
    failures += expect_span_text(column_type, "VARCHAR(0)", "varchar zero span");
    column = child_at(columns, 1U);
    column_type = child_at(column, 1U);
    failures += expect_span_text(child_at(column, 0U), "label", "varchar max column name");
    failures += expect_varchar_type(column_type, "255", "varchar max column type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "varchar max not null"
    );
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "varchar string default"
    );
    column = child_at(columns, 2U);
    column_type = child_at(column, 1U);
    failures += expect_varchar_type(column_type, "3", "character varying column type");
    failures += expect_span_text(column_type, "CHARACTER VARYING(3)", "character varying span");
    column = child_at(columns, 3U);
    column_type = child_at(column, 1U);
    failures += expect_varchar_type(column_type, "4", "char varying column type");
    failures += expect_span_text(column_type, "CHAR VARYING(4)", "char varying span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE national_varchar_types ("
        "nv NVARCHAR(5), "
        "national_v NATIONAL VARCHAR(6), "
        "nchar_v NCHAR VARCHAR(7), "
        "nchar_varying NCHAR VARYING(8), "
        "national_char_varying NATIONAL CHAR VARYING(9), "
        "national_character_varying NATIONAL CHARACTER VARYING(10));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures +=
        expect_child_count(columns, national_varchar_column_count, "national varchar column list");
    column = child_at(columns, 0U);
    column_type = child_at(column, 1U);
    failures += expect_national_varchar_type(column_type, "5", "nvarchar column type");
    failures += expect_span_text(column_type, "NVARCHAR(5)", "nvarchar span");
    column = child_at(columns, 1U);
    column_type = child_at(column, 1U);
    failures += expect_national_varchar_type(column_type, "6", "national varchar column type");
    failures += expect_span_text(column_type, "NATIONAL VARCHAR(6)", "national varchar span");
    column = child_at(columns, 2U);
    column_type = child_at(column, 1U);
    failures += expect_national_varchar_type(column_type, "7", "nchar varchar column type");
    failures += expect_span_text(column_type, "NCHAR VARCHAR(7)", "nchar varchar span");
    column = child_at(columns, 3U);
    column_type = child_at(column, 1U);
    failures += expect_national_varchar_type(column_type, "8", "nchar varying column type");
    failures += expect_span_text(column_type, "NCHAR VARYING(8)", "nchar varying span");
    column = child_at(columns, 4U);
    column_type = child_at(column, 1U);
    failures += expect_national_varchar_type(column_type, "9", "national char varying column type");
    failures +=
        expect_span_text(column_type, "NATIONAL CHAR VARYING(9)", "national char varying span");
    column = child_at(columns, national_varchar_last_column_index);
    column_type = child_at(column, 1U);
    failures +=
        expect_national_varchar_type(column_type, "10", "national character varying column type");
    failures += expect_span_text(
        column_type,
        "NATIONAL CHARACTER VARYING(10)",
        "national character varying span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ADD COLUMN label VARCHAR(12) NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    column_type = child_at(column, 1U);
    failures += expect_varchar_type(column_type, "12", "alter add varchar column type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "alter add varchar not null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col VARCHAR(15) NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    column_type = child_at(column, 1U);
    failures += expect_varchar_type(column_type, "15", "varchar alter modify column type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "varchar alter modify nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_text VARCHAR(20) NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 2U);
    column_type = child_at(column, 1U);
    failures += expect_varchar_type(column_type, "20", "varchar alter change column type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "varchar alter change nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT INTO simple_lifecycle VALUES ('text');", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_literal(
        child_at(child_at(child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "string insert value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE bad_nvarchar (c NVARCHAR);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "CREATE TABLE bad_national_varchar (c NATIONAL VARCHAR);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_char_type_statements(void) {
    enum {
        char_column_count = 5U,
        national_char_column_count = 6U,
        national_char_last_column_index = 5U,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *column_type = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE char_types (c CHAR, c0 CHAR(0), c255 CHAR(255) NOT NULL DEFAULT 'z', "
        "alias CHARACTER, alias2 CHARACTER(2));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, char_column_count, "char column list");
    column = child_at(columns, 0U);
    column_type = child_at(column, 1U);
    failures += expect_span_text(child_at(column, 0U), "c", "bare char column name");
    failures += expect_char_type(column_type, NULL, 0, "bare char column type");
    failures += expect_span_text(column_type, "CHAR", "bare char span");
    column = child_at(columns, 1U);
    column_type = child_at(column, 1U);
    failures += expect_span_text(child_at(column, 0U), "c0", "char zero column name");
    failures += expect_char_type(column_type, "0", 1, "char zero column type");
    failures += expect_span_text(column_type, "CHAR(0)", "char zero span");
    column = child_at(columns, 2U);
    column_type = child_at(column, 1U);
    failures += expect_span_text(child_at(column, 0U), "c255", "char max column name");
    failures += expect_char_type(column_type, "255", 1, "char max column type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "char max not null"
    );
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "char string default"
    );
    column = child_at(columns, 3U);
    column_type = child_at(column, 1U);
    failures += expect_char_type(column_type, NULL, 0, "bare character alias type");
    failures += expect_span_text(column_type, "CHARACTER", "bare character alias span");
    column = child_at(columns, 4U);
    column_type = child_at(column, 1U);
    failures += expect_char_type(column_type, "2", 1, "character alias length type");
    failures += expect_span_text(column_type, "CHARACTER(2)", "character alias length span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE national_char_types ("
        "n NCHAR, "
        "n2 NCHAR(2), "
        "national_char NATIONAL CHAR, "
        "national_char3 NATIONAL CHAR(3), "
        "national_character NATIONAL CHARACTER, "
        "national_character4 NATIONAL CHARACTER(4));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures +=
        expect_child_count(columns, national_char_column_count, "national char column list");
    column = child_at(columns, 0U);
    column_type = child_at(column, 1U);
    failures += expect_national_char_type(column_type, NULL, 0, "bare nchar column type");
    failures += expect_span_text(column_type, "NCHAR", "bare nchar span");
    column = child_at(columns, 1U);
    column_type = child_at(column, 1U);
    failures += expect_national_char_type(column_type, "2", 1, "nchar length column type");
    failures += expect_span_text(column_type, "NCHAR(2)", "nchar length span");
    column = child_at(columns, 2U);
    column_type = child_at(column, 1U);
    failures += expect_national_char_type(column_type, NULL, 0, "national char column type");
    failures += expect_span_text(column_type, "NATIONAL CHAR", "national char span");
    column = child_at(columns, 3U);
    column_type = child_at(column, 1U);
    failures += expect_national_char_type(column_type, "3", 1, "national char length column type");
    failures += expect_span_text(column_type, "NATIONAL CHAR(3)", "national char length span");
    column = child_at(columns, 4U);
    column_type = child_at(column, 1U);
    failures += expect_national_char_type(column_type, NULL, 0, "national character column type");
    failures += expect_span_text(column_type, "NATIONAL CHARACTER", "national character span");
    column = child_at(columns, national_char_last_column_index);
    column_type = child_at(column, 1U);
    failures +=
        expect_national_char_type(column_type, "4", 1, "national character length column type");
    failures +=
        expect_span_text(column_type, "NATIONAL CHARACTER(4)", "national character length span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ADD COLUMN code CHAR(2) NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    column_type = child_at(column, 1U);
    failures += expect_char_type(column_type, "2", 1, "alter add char column type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "alter add char not null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col CHAR(3) NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    column_type = child_at(column, 1U);
    failures += expect_char_type(column_type, "3", 1, "char alter modify column type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "char alter modify nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_code CHAR(4) NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 2U);
    column_type = child_at(column, 1U);
    failures += expect_char_type(column_type, "4", 1, "char alter change column type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "char alter change nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE bad_char (c CHAR());", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("CREATE TABLE bad_char (c CHAR(-1));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "CREATE TABLE bad_char (c CHARACTER VARYING);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "CREATE TABLE bad_char (c CHAR VARYING);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("CREATE TABLE bad_char (c CHARACTER());", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("CREATE TABLE bad_char (c NCHAR());", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("CREATE TABLE bad_char (c NCHAR(-1));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_text_type_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *column_type = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE text_types (tt TINYTEXT, t TEXT, mt MEDIUMTEXT NOT NULL, lt LONGTEXT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, 4U, "text family column list");

    column = child_at(columns, 0U);
    column_type = child_at(column, 1U);
    failures += expect_span_text(child_at(column, 0U), "tt", "tinytext column name");
    failures +=
        expect_text_type(column_type, MYLITE_SQL_AST_TEXT_TYPE_TINYTEXT, "tinytext column type");
    failures += expect_span_text(column_type, "TINYTEXT", "tinytext span");

    column = child_at(columns, 1U);
    column_type = child_at(column, 1U);
    failures += expect_span_text(child_at(column, 0U), "t", "text column name");
    failures += expect_text_type(column_type, MYLITE_SQL_AST_TEXT_TYPE_TEXT, "text column type");
    failures += expect_span_text(column_type, "TEXT", "text span");

    column = child_at(columns, 2U);
    column_type = child_at(column, 1U);
    failures += expect_span_text(child_at(column, 0U), "mt", "mediumtext column name");
    failures += expect_text_type(
        column_type,
        MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT,
        "mediumtext column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "mediumtext not null"
    );

    column = child_at(columns, 3U);
    column_type = child_at(column, 1U);
    failures += expect_span_text(child_at(column, 0U), "lt", "longtext column name");
    failures +=
        expect_text_type(column_type, MYLITE_SQL_AST_TEXT_TYPE_LONGTEXT, "longtext column type");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE long_aliases (a LONG, b LONG VARCHAR NOT NULL, c LONG VARBINARY);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, 3U, "long alias column list");
    column = child_at(columns, 0U);
    column_type = child_at(column, 1U);
    failures += expect_span_text(child_at(column, 0U), "a", "long alias column name");
    failures += expect_text_type(
        column_type,
        MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT,
        "long alias mediumtext type"
    );
    failures += expect_span_text(column_type, "LONG", "long alias span");
    column = child_at(columns, 1U);
    column_type = child_at(column, 1U);
    failures += expect_span_text(child_at(column, 0U), "b", "long varchar alias column name");
    failures += expect_text_type(
        column_type,
        MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT,
        "long varchar alias mediumtext type"
    );
    failures += expect_span_text(column_type, "LONG VARCHAR", "long varchar alias span");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "long varchar alias not null"
    );
    column = child_at(columns, 2U);
    column_type = child_at(column, 1U);
    failures += expect_span_text(child_at(column, 0U), "c", "long varbinary alias column name");
    failures += expect_binary_string_type(
        column_type,
        MYLITE_SQL_AST_BINARY_STRING_TYPE_MEDIUMBLOB,
        "long varbinary alias mediumblob type"
    );
    failures += expect_span_text(column_type, "LONG VARBINARY", "long varbinary alias span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE text (text INT, body TEXT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(child_at(statement, 0U), "text", "nonreserved text table name");
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, 2U, "nonreserved text identifier columns");
    column = child_at(columns, 0U);
    failures += expect_span_text(child_at(column, 0U), "text", "nonreserved text column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "nonreserved text column integer type"
    );
    column = child_at(columns, 1U);
    failures += expect_span_text(child_at(column, 0U), "body", "nonreserved text body column name");
    failures += expect_text_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_TEXT_TYPE_TEXT,
        "nonreserved text body type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE long (long INT, body LONG);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(child_at(statement, 0U), "long", "nonreserved long table name");
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, 2U, "nonreserved long identifier columns");
    column = child_at(columns, 0U);
    failures += expect_span_text(child_at(column, 0U), "long", "nonreserved long column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "nonreserved long column integer type"
    );
    column = child_at(columns, 1U);
    failures += expect_span_text(child_at(column, 0U), "body", "nonreserved long body name");
    failures += expect_text_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT,
        "nonreserved long body alias type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ADD COLUMN body TEXT NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    column_type = child_at(column, 1U);
    failures +=
        expect_text_type(column_type, MYLITE_SQL_AST_TEXT_TYPE_TEXT, "alter add text column type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "alter add text not null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY body MEDIUMTEXT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    column_type = child_at(column, 1U);
    failures += expect_text_type(
        column_type,
        MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT,
        "alter modify mediumtext column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "alter modify mediumtext nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_body new_body LONGTEXT NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 2U);
    column_type = child_at(column, 1U);
    failures += expect_text_type(
        column_type,
        MYLITE_SQL_AST_TEXT_TYPE_LONGTEXT,
        "alter change longtext column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "alter change longtext nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE invalid_text_length (body TEXT(10));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "CREATE TABLE invalid_long_varchar_length (body LONG VARCHAR(10));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "CREATE TABLE invalid_long_varbinary_length (body LONG VARBINARY(10));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "CREATE TABLE invalid_long_text (body LONG TEXT);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "CREATE TABLE invalid_long_binary (body LONG BINARY);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_json_type_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE json_types (id INT, payload JSON, required JSON NOT NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, 3U, "json column list");

    column = child_at(columns, 1U);
    failures += expect_span_text(child_at(column, 0U), "payload", "json column name");
    failures += expect_node(child_at(column, 1U), MYLITE_SQL_AST_JSON_TYPE, "json column type");
    failures += expect_span_text(child_at(column, 1U), "JSON", "json type span");

    column = child_at(columns, 2U);
    failures += expect_node(child_at(column, 1U), MYLITE_SQL_AST_JSON_TYPE, "json not null type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "json not null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE json (json INT, payload JSON);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(child_at(statement, 0U), "json", "nonreserved json table name");
    columns = child_at(statement, 1U);
    column = child_at(columns, 0U);
    failures += expect_span_text(child_at(column, 0U), "json", "nonreserved json column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "nonreserved json column integer type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE json_types ADD COLUMN metadata JSON NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_node(child_at(column, 1U), MYLITE_SQL_AST_JSON_TYPE, "alter add json type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "alter add json null"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_enum_type_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *column_type = NULL;
    const struct mylite_sql_ast_node *labels = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE enum_types (status ENUM('draft','published') NOT NULL DEFAULT 'draft', "
        "enum ENUM('', 'A''B'));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, 2U, "enum column list");
    column = child_at(columns, 0U);
    column_type = child_at(column, 1U);
    labels = child_at(column_type, 0U);
    failures += expect_node(column_type, MYLITE_SQL_AST_ENUM_TYPE, "enum status column type");
    failures += expect_span_text(column_type, "ENUM('draft','published')", "enum status span");
    failures += expect_child_count(labels, 2U, "enum status label count");
    failures +=
        expect_literal(child_at(labels, 0U), MYLITE_SQL_AST_LITERAL_STRING, "enum draft label");
    failures += expect_span_text(child_at(labels, 0U), "'draft'", "enum draft label span");
    failures +=
        expect_literal(child_at(labels, 1U), MYLITE_SQL_AST_LITERAL_STRING, "enum published label");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "enum not null"
    );
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "enum string default"
    );
    column = child_at(columns, 1U);
    column_type = child_at(column, 1U);
    labels = child_at(column_type, 0U);
    failures += expect_span_text(child_at(column, 0U), "enum", "enum keyword identifier");
    failures += expect_node(column_type, MYLITE_SQL_AST_ENUM_TYPE, "enum keyword column type");
    failures += expect_child_count(labels, 2U, "enum escaped label count");
    failures += expect_span_text(child_at(labels, 0U), "''", "enum empty label span");
    failures += expect_span_text(child_at(labels, 1U), "'A''B'", "enum escaped label span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE enum_types ADD COLUMN next_status ENUM('queued','done') NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    column_type = child_at(column, 1U);
    failures += expect_node(column_type, MYLITE_SQL_AST_ENUM_TYPE, "alter add enum column type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "alter add enum nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE bad_enum (v ENUM());", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_set_type_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *column_type = NULL;
    const struct mylite_sql_ast_node *members = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE set_types (flags SET('active','featured') NOT NULL DEFAULT '', "
        "set SET('', 'A''B'));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, 2U, "set column list");
    column = child_at(columns, 0U);
    column_type = child_at(column, 1U);
    members = child_at(column_type, 0U);
    failures += expect_node(column_type, MYLITE_SQL_AST_SET_TYPE, "set flags column type");
    failures += expect_span_text(column_type, "SET('active','featured')", "set flags span");
    failures += expect_child_count(members, 2U, "set flags member count");
    failures +=
        expect_literal(child_at(members, 0U), MYLITE_SQL_AST_LITERAL_STRING, "set active member");
    failures += expect_span_text(child_at(members, 0U), "'active'", "set active member span");
    failures +=
        expect_literal(child_at(members, 1U), MYLITE_SQL_AST_LITERAL_STRING, "set featured member");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "set not null"
    );
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "set string default"
    );
    column = child_at(columns, 1U);
    column_type = child_at(column, 1U);
    members = child_at(column_type, 0U);
    failures += expect_span_text(child_at(column, 0U), "set", "set keyword identifier");
    failures += expect_node(column_type, MYLITE_SQL_AST_SET_TYPE, "set keyword column type");
    failures += expect_child_count(members, 2U, "set escaped member count");
    failures += expect_span_text(child_at(members, 0U), "''", "set empty member span");
    failures += expect_span_text(child_at(members, 1U), "'A''B'", "set escaped member span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE set_types ADD COLUMN next_flags SET('queued','done') NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    column_type = child_at(column, 1U);
    failures += expect_node(column_type, MYLITE_SQL_AST_SET_TYPE, "alter add set column type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "alter add set nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE bad_set (v SET());", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_bit_type_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *column_type = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE bit (bit INT, b BIT, b6 BIT(6) NOT NULL DEFAULT b'101');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_span_text(child_at(statement, 0U), "bit", "nonreserved bit table name");
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, 3U, "bit column list");
    column = child_at(columns, 0U);
    failures += expect_span_text(child_at(column, 0U), "bit", "nonreserved bit column name");
    failures += expect_integer_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "nonreserved bit column integer type"
    );
    column = child_at(columns, 1U);
    column_type = child_at(column, 1U);
    failures += expect_bit_type(column_type, NULL, 0, "bare bit column type");
    failures += expect_span_text(column_type, "BIT", "bare bit span");
    column = child_at(columns, 2U);
    column_type = child_at(column, 1U);
    failures += expect_bit_type(column_type, "6", 1, "bit width column type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "bit width not null"
    );
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_BIT,
        "bit literal default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ADD COLUMN flags BIT(9) DEFAULT b'101';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    column_type = child_at(column, 1U);
    failures += expect_bit_type(column_type, "9", 1, "alter add bit column type");
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_BIT,
        "alter add bit literal default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col BIT(8) NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_bit_type(child_at(column, 1U), "8", 1, "bit alter modify column type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "bit alter modify nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_bits BIT NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 2U);
    failures += expect_bit_type(child_at(column, 1U), NULL, 0, "bit alter change column type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "bit alter change nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE bad_bit (b BIT());", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("CREATE TABLE bad_bit (b BIT(-1));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("CREATE TABLE bad_bit (b BIT(1.0));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_year_type_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *column_type = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE year_types (y YEAR, y4 YEAR(4) NOT NULL DEFAULT '70', year INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, year_column_count, "year column list");
    column = child_at(columns, 0U);
    column_type = child_at(column, 1U);
    failures += expect_year_type(column_type, NULL, 0, "bare year column type");
    failures += expect_span_text(column_type, "YEAR", "bare year span");
    column = child_at(columns, 1U);
    column_type = child_at(column, 1U);
    failures += expect_year_type(column_type, "4", 1, "year width column type");
    failures += expect_span_text(column_type, "YEAR(4)", "year width span");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "year width not null"
    );
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "year string default"
    );
    column = child_at(columns, 2U);
    failures += expect_span_text(child_at(column, 0U), "year", "year keyword identifier");
    failures +=
        expect_node(child_at(column, 1U), MYLITE_SQL_AST_INTEGER_TYPE, "year identifier type");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE year_types ADD COLUMN created YEAR DEFAULT 70;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_year_type(child_at(column, 1U), NULL, 0, "alter add year");
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "alter year default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM year_types WHERE y IN ('70', 2000, NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    where_clause = child_at(statement, 2U);
    predicate = child_at(where_clause, 0U);
    failures += expect_node(predicate, MYLITE_SQL_AST_IN_PREDICATE, "year in predicate");
    failures += expect_literal(
        child_at(child_at(predicate, 1U), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "year in string"
    );
    failures += expect_literal(
        child_at(child_at(predicate, 1U), 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "year in integer"
    );
    failures += expect_literal(
        child_at(child_at(predicate, 1U), 2U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "year in null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE year_types SET y = '69';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 1U), 0U);
    failures += expect_literal(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "year update string value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE bad_year (y YEAR());", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("CREATE TABLE bad_year (y YEAR(-1));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("CREATE TABLE bad_year (y YEAR(1.0));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_decimal_type_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *column_type = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE decimal_types (a DECIMAL, b DECIMAL(5), c DECIMAL(5,2) UNSIGNED, "
        "d NUMERIC(4,1), e DEC(6,0), f FIXED(7,3));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, decimal_column_count, "decimal column list");

    column = child_at(columns, 0U);
    column_type = child_at(column, 1U);
    failures += expect_decimal_type(
        column_type,
        MYLITE_SQL_AST_DECIMAL_TYPE_DECIMAL,
        NULL,
        NULL,
        0,
        "bare decimal column type"
    );
    failures += expect_span_text(column_type, "DECIMAL", "bare decimal span");

    column = child_at(columns, 1U);
    column_type = child_at(column, 1U);
    failures += expect_decimal_type(
        column_type,
        MYLITE_SQL_AST_DECIMAL_TYPE_DECIMAL,
        "5",
        NULL,
        0,
        "precision decimal column type"
    );

    column = child_at(columns, 2U);
    column_type = child_at(column, 1U);
    failures += expect_decimal_type(
        column_type,
        MYLITE_SQL_AST_DECIMAL_TYPE_DECIMAL,
        "5",
        "2",
        1,
        "unsigned decimal column type"
    );
    failures += expect_span_text(column_type, "DECIMAL(5,2) UNSIGNED", "unsigned decimal span");

    column = child_at(columns, 3U);
    failures += expect_decimal_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_DECIMAL_TYPE_NUMERIC,
        "4",
        "1",
        0,
        "numeric column type"
    );

    column = child_at(columns, 4U);
    failures += expect_decimal_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_DECIMAL_TYPE_DEC,
        "6",
        "0",
        0,
        "dec column type"
    );

    column = child_at(columns, decimal_fixed_column_index);
    failures += expect_decimal_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_DECIMAL_TYPE_FIXED,
        "7",
        "3",
        0,
        "fixed column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE decimal_defaults (a DECIMAL(5,2) DEFAULT 1.23, "
        "b NUMERIC(5,2) DEFAULT -0.50, c DECIMAL(5,2) DEFAULT +1.20);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    column = child_at(columns, 0U);
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_DECIMAL,
        "decimal default literal"
    );
    column = child_at(columns, 1U);
    failures += expect_operator(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "negative decimal default"
    );
    column = child_at(columns, 2U);
    failures += expect_operator(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "positive decimal default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle ADD COLUMN amount DECIMAL(8,2) NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_decimal_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_DECIMAL_TYPE_DECIMAL,
        "8",
        "2",
        0,
        "alter add decimal column type"
    );
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "alter add decimal not null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY amount NUMERIC(9,3) NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_decimal_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_DECIMAL_TYPE_NUMERIC,
        "9",
        "3",
        0,
        "alter modify numeric column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE amount total DEC(10,4) NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 2U);
    failures += expect_decimal_type(
        child_at(column, 1U),
        MYLITE_SQL_AST_DECIMAL_TYPE_DEC,
        "10",
        "4",
        0,
        "alter change dec column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT INTO decimal_types VALUES (1.20, -2.30, +3.40, NULL, TRUE, FALSE);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_literal(
        child_at(child_at(child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_LITERAL_DECIMAL,
        "decimal insert value"
    );
    failures += expect_operator(
        child_at(child_at(child_at(statement, 2U), 0U), 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "negative decimal insert value"
    );
    failures += expect_operator(
        child_at(child_at(child_at(statement, 2U), 0U), 2U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "positive decimal insert value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE decimal_types SET a = 9.99;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_literal(
        child_at(child_at(child_at(statement, 1U), 0U), 1U),
        MYLITE_SQL_AST_LITERAL_DECIMAL,
        "decimal update value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE bad_decimal (c DECIMAL SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_approximate_type_statements(void) {
    enum {
        approximate_double_column = 5,
        approximate_double_precision_column = 6,
        approximate_real_column = 7,
        approximate_unsigned_float_column = 8,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE approximate_types (a FLOAT, b FLOAT(24), c FLOAT(25), "
        "d FLOAT4, e FLOAT8, f DOUBLE, g DOUBLE PRECISION, h REAL, i FLOAT UNSIGNED);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, approximate_column_count, "approximate column list");
    failures += expect_approximate_type(
        child_at(child_at(columns, 0U), 1U),
        MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT,
        NULL,
        0,
        "bare float column type"
    );
    failures += expect_approximate_type(
        child_at(child_at(columns, 1U), 1U),
        MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT,
        "24",
        0,
        "precision float column type"
    );
    failures += expect_approximate_type(
        child_at(child_at(columns, 2U), 1U),
        MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT,
        "25",
        0,
        "double precision float column type"
    );
    failures += expect_approximate_type(
        child_at(child_at(columns, 3U), 1U),
        MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT4,
        NULL,
        0,
        "float4 column type"
    );
    failures += expect_approximate_type(
        child_at(child_at(columns, 4U), 1U),
        MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT8,
        NULL,
        0,
        "float8 column type"
    );
    failures += expect_approximate_type(
        child_at(child_at(columns, approximate_double_column), 1U),
        MYLITE_SQL_AST_APPROXIMATE_TYPE_DOUBLE,
        NULL,
        0,
        "double column type"
    );
    failures += expect_span_text(
        child_at(child_at(columns, approximate_double_precision_column), 1U),
        "DOUBLE PRECISION",
        "double precision span"
    );
    failures += expect_approximate_type(
        child_at(child_at(columns, approximate_real_column), 1U),
        MYLITE_SQL_AST_APPROXIMATE_TYPE_REAL,
        NULL,
        0,
        "real column type"
    );
    failures += expect_approximate_type(
        child_at(child_at(columns, approximate_unsigned_float_column), 1U),
        MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT,
        NULL,
        1,
        "unsigned float column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE approximate_defaults (a FLOAT DEFAULT 1.25, "
        "b DOUBLE DEFAULT -2.5e1, c FLOAT DEFAULT +3.5E-1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    column = child_at(columns, 0U);
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_DECIMAL,
        "approximate decimal default literal"
    );
    column = child_at(columns, 1U);
    failures += expect_operator(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "negative approximate default"
    );
    column = child_at(columns, 2U);
    failures += expect_operator(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "positive approximate default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT INTO approximate_types VALUES (1.25e1, -2.5E0, +3.5e-1, NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_literal(
        child_at(child_at(child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_LITERAL_FLOAT,
        "approximate insert float literal"
    );
    failures += expect_operator(
        child_at(child_at(child_at(statement, 2U), 0U), 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "negative approximate insert value"
    );
    failures += expect_operator(
        child_at(child_at(child_at(statement, 2U), 0U), 2U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "positive approximate insert value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE approximate_types SET a = 9.75e0;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_literal(
        child_at(child_at(child_at(statement, 1U), 0U), 1U),
        MYLITE_SQL_AST_LITERAL_FLOAT,
        "approximate update value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE bad_approximate (c FLOAT SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "CREATE TABLE bad_approximate_scale (c DOUBLE(7,4));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_date_type_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE date_types (d DATE, nn DATE NOT NULL DEFAULT '2024-02-29', date INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, date_column_count, "date column list");
    column = child_at(columns, 0U);
    failures += expect_node(child_at(column, 1U), MYLITE_SQL_AST_DATE_TYPE, "date column type");
    failures += expect_span_text(child_at(column, 1U), "DATE", "date column span");
    column = child_at(columns, 1U);
    failures += expect_node(child_at(column, 1U), MYLITE_SQL_AST_DATE_TYPE, "not null date type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "not null date"
    );
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "date string default"
    );
    column = child_at(columns, 2U);
    failures += expect_span_text(child_at(column, 0U), "date", "date keyword identifier");
    failures +=
        expect_node(child_at(column, 1U), MYLITE_SQL_AST_INTEGER_TYPE, "date identifier type");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE date_types ADD COLUMN created DATE DEFAULT '1000-01-01';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_node(child_at(column, 1U), MYLITE_SQL_AST_DATE_TYPE, "alter add date");
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "alter date default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM date_types WHERE d BETWEEN '2024-01-01' AND '2024-12-31';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    where_clause = child_at(statement, 2U);
    predicate = child_at(where_clause, 0U);
    failures += expect_node(predicate, MYLITE_SQL_AST_BETWEEN_PREDICATE, "date between");
    failures += expect_literal(
        child_at(predicate, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "date between lower"
    );
    failures += expect_literal(
        child_at(predicate, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "date between upper"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM date_types WHERE d IN ('2024-02-29', NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    predicate = child_at(child_at(statement, 2U), 0U);
    failures += expect_node(predicate, MYLITE_SQL_AST_IN_PREDICATE, "date in predicate");
    failures += expect_literal(
        child_at(child_at(predicate, 1U), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "date in string"
    );
    failures += expect_literal(
        child_at(child_at(predicate, 1U), 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "date in null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE date_types SET d = '2025-01-02';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 1U), 0U);
    failures += expect_literal(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "date update string value"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_datetime_type_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE datetime_types (d DATETIME, nn DATETIME NOT NULL DEFAULT "
        "'2024-05-06 07:08:09', datetime INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, datetime_column_count, "datetime column list");
    column = child_at(columns, 0U);
    failures +=
        expect_node(child_at(column, 1U), MYLITE_SQL_AST_DATETIME_TYPE, "datetime column type");
    failures += expect_span_text(child_at(column, 1U), "DATETIME", "datetime column span");
    column = child_at(columns, 1U);
    failures +=
        expect_node(child_at(column, 1U), MYLITE_SQL_AST_DATETIME_TYPE, "not null datetime type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "not null datetime"
    );
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "datetime string default"
    );
    column = child_at(columns, 2U);
    failures += expect_span_text(child_at(column, 0U), "datetime", "datetime keyword identifier");
    failures +=
        expect_node(child_at(column, 1U), MYLITE_SQL_AST_INTEGER_TYPE, "datetime identifier type");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE datetime_types ADD COLUMN created DATETIME DEFAULT "
        "'1000-01-01 00:00:00';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures +=
        expect_node(child_at(column, 1U), MYLITE_SQL_AST_DATETIME_TYPE, "alter add datetime");
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "alter datetime default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM datetime_types WHERE d BETWEEN '2024-01-01 00:00:00' "
        "AND '2024-12-31 23:59:59';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    where_clause = child_at(statement, 2U);
    predicate = child_at(where_clause, 0U);
    failures += expect_node(predicate, MYLITE_SQL_AST_BETWEEN_PREDICATE, "datetime between");
    failures += expect_literal(
        child_at(predicate, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "datetime between lower"
    );
    failures += expect_literal(
        child_at(predicate, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "datetime between upper"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM datetime_types WHERE d IN ('2024-05-06 07:08:09', NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    predicate = child_at(child_at(statement, 2U), 0U);
    failures += expect_node(predicate, MYLITE_SQL_AST_IN_PREDICATE, "datetime in predicate");
    failures += expect_literal(
        child_at(child_at(predicate, 1U), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "datetime in string"
    );
    failures += expect_literal(
        child_at(child_at(predicate, 1U), 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "datetime in null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE datetime_types SET d = '2025-01-02 03:04:05';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 1U), 0U);
    failures += expect_literal(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "datetime update string value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE datetime_fractional (d DATETIME(3));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_time_type_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE time_types (t TIME, nn TIME NOT NULL DEFAULT '01:02:03', time INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, time_column_count, "time column list");
    column = child_at(columns, 0U);
    failures += expect_node(child_at(column, 1U), MYLITE_SQL_AST_TIME_TYPE, "time column type");
    failures += expect_span_text(child_at(column, 1U), "TIME", "time column span");
    column = child_at(columns, 1U);
    failures += expect_node(child_at(column, 1U), MYLITE_SQL_AST_TIME_TYPE, "not null time type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "not null time"
    );
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "time string default"
    );
    column = child_at(columns, 2U);
    failures += expect_span_text(child_at(column, 0U), "time", "time keyword identifier");
    failures +=
        expect_node(child_at(column, 1U), MYLITE_SQL_AST_INTEGER_TYPE, "time identifier type");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE time_types ADD COLUMN elapsed TIME DEFAULT '-00:00:01';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures += expect_node(child_at(column, 1U), MYLITE_SQL_AST_TIME_TYPE, "alter add time");
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "alter time default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM time_types WHERE t BETWEEN '-00:00:01' AND '24:00:00';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    where_clause = child_at(statement, 2U);
    predicate = child_at(where_clause, 0U);
    failures += expect_node(predicate, MYLITE_SQL_AST_BETWEEN_PREDICATE, "time between");
    failures += expect_literal(
        child_at(predicate, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "time between lower"
    );
    failures += expect_literal(
        child_at(predicate, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "time between upper"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM time_types WHERE t IN ('838:59:59', NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    predicate = child_at(child_at(statement, 2U), 0U);
    failures += expect_node(predicate, MYLITE_SQL_AST_IN_PREDICATE, "time in predicate");
    failures += expect_literal(
        child_at(child_at(predicate, 1U), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "time in string"
    );
    failures += expect_literal(
        child_at(child_at(predicate, 1U), 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "time in null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE time_types SET t = '02:03:04';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 1U), 0U);
    failures += expect_literal(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "time update string value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE time_fractional (t TIME(3));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_timestamp_type_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE timestamp_types (ts TIMESTAMP, nn TIMESTAMP NOT NULL DEFAULT "
        "'2024-05-06 07:08:09', timestamp INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    failures += expect_child_count(columns, timestamp_column_count, "timestamp column list");
    column = child_at(columns, 0U);
    failures +=
        expect_node(child_at(column, 1U), MYLITE_SQL_AST_TIMESTAMP_TYPE, "timestamp column type");
    failures += expect_span_text(child_at(column, 1U), "TIMESTAMP", "timestamp column span");
    column = child_at(columns, 1U);
    failures +=
        expect_node(child_at(column, 1U), MYLITE_SQL_AST_TIMESTAMP_TYPE, "not null timestamp type");
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "not null timestamp"
    );
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "timestamp string default"
    );
    column = child_at(columns, 2U);
    failures += expect_span_text(child_at(column, 0U), "timestamp", "timestamp keyword identifier");
    failures +=
        expect_node(child_at(column, 1U), MYLITE_SQL_AST_INTEGER_TYPE, "timestamp identifier type");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE timestamp_types ADD COLUMN created TIMESTAMP DEFAULT "
        "'1970-01-01 00:00:01';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(statement, 1U);
    failures +=
        expect_node(child_at(column, 1U), MYLITE_SQL_AST_TIMESTAMP_TYPE, "alter add timestamp");
    failures += expect_literal(
        child_at(first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "alter timestamp default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM timestamp_types WHERE ts BETWEEN '1970-01-01 00:00:01' "
        "AND '2038-01-19 03:14:07';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    where_clause = child_at(statement, 2U);
    predicate = child_at(where_clause, 0U);
    failures += expect_node(predicate, MYLITE_SQL_AST_BETWEEN_PREDICATE, "timestamp between");
    failures += expect_literal(
        child_at(predicate, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "timestamp between lower"
    );
    failures += expect_literal(
        child_at(predicate, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "timestamp between upper"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM timestamp_types WHERE ts IN ('2024-05-06 07:08:09', NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    predicate = child_at(child_at(statement, 2U), 0U);
    failures += expect_node(predicate, MYLITE_SQL_AST_IN_PREDICATE, "timestamp in predicate");
    failures += expect_literal(
        child_at(child_at(predicate, 1U), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "timestamp in string"
    );
    failures += expect_literal(
        child_at(child_at(predicate, 1U), 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "timestamp in null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE timestamp_types SET ts = '2025-01-02 03:04:05';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 1U), 0U);
    failures += expect_literal(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "timestamp update string value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE timestamp_fractional (ts TIMESTAMP(3));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_current_date_time_function_statements(void) {
    enum {
        curdate_item_index = 0U,
        current_date_item_index = 1U,
        current_date_call_item_index = 2U,
        curtime_item_index = 3U,
        current_time_item_index = 4U,
        current_time_call_item_index = 5U,
        ignore_space_curtime_item_index = 1U,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT CURDATE(), CURRENT_DATE, CURRENT_DATE(), CURTIME(), CURRENT_TIME, CURRENT_TIME();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    failures += expect_node(
        child_at(child_at(select_list, curdate_item_index), 0U),
        MYLITE_SQL_AST_CURRENT_DATE_VALUE,
        "CURDATE select item"
    );
    failures += expect_node(
        child_at(child_at(select_list, current_date_item_index), 0U),
        MYLITE_SQL_AST_CURRENT_DATE_VALUE,
        "CURRENT_DATE select item"
    );
    failures += expect_node(
        child_at(child_at(select_list, current_date_call_item_index), 0U),
        MYLITE_SQL_AST_CURRENT_DATE_VALUE,
        "CURRENT_DATE() select item"
    );
    failures += expect_node(
        child_at(child_at(select_list, curtime_item_index), 0U),
        MYLITE_SQL_AST_CURRENT_TIME_VALUE,
        "CURTIME select item"
    );
    failures += expect_node(
        child_at(child_at(select_list, current_time_item_index), 0U),
        MYLITE_SQL_AST_CURRENT_TIME_VALUE,
        "CURRENT_TIME select item"
    );
    failures += expect_node(
        child_at(child_at(select_list, current_time_call_item_index), 0U),
        MYLITE_SQL_AST_CURRENT_TIME_VALUE,
        "CURRENT_TIME() select item"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT INTO t VALUES (CURDATE(), CURRENT_TIME);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "INSERT INTO t SET d = CURDATE(), tm = CURRENT_TIME;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("REPLACE INTO t VALUES (CURRENT_DATE, CURTIME());", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "REPLACE INTO t SET d = CURRENT_DATE, tm = CURTIME();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE t SET d = CURRENT_DATE, tm = CURTIME();", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 1U), 0U);
    failures += expect_node(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_CURRENT_DATE_VALUE,
        "current date update assignment"
    );
    assignment = child_at(child_at(statement, 1U), 1U);
    failures += expect_node(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_CURRENT_TIME_VALUE,
        "current time update assignment"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CURDATE(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CURRENT_DATE(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CURTIME(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CURRENT_TIME(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CURDATE ();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CURTIME ();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql_with_ignore_space("SELECT CURDATE (), CURTIME ();", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_node(
        child_at(child_at(select_list, curdate_item_index), 0U),
        MYLITE_SQL_AST_CURRENT_DATE_VALUE,
        "ignore_space CURDATE select item"
    );
    failures += expect_node(
        child_at(child_at(select_list, ignore_space_curtime_item_index), 0U),
        MYLITE_SQL_AST_CURRENT_TIME_VALUE,
        "ignore_space CURTIME select item"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_utc_date_time_function_statements(void) {
    enum {
        utc_date_item_index = 0U,
        utc_date_call_item_index = 1U,
        utc_time_item_index = 2U,
        utc_time_call_item_index = 3U,
        utc_timestamp_item_index = 4U,
        utc_timestamp_call_item_index = 5U,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT UTC_DATE, UTC_DATE(), UTC_TIME, UTC_TIME(), UTC_TIMESTAMP, UTC_TIMESTAMP();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    failures += expect_node(
        child_at(child_at(select_list, utc_date_item_index), 0U),
        MYLITE_SQL_AST_UTC_DATE_VALUE,
        "UTC_DATE select item"
    );
    failures += expect_node(
        child_at(child_at(select_list, utc_date_call_item_index), 0U),
        MYLITE_SQL_AST_UTC_DATE_VALUE,
        "UTC_DATE() select item"
    );
    failures += expect_node(
        child_at(child_at(select_list, utc_time_item_index), 0U),
        MYLITE_SQL_AST_UTC_TIME_VALUE,
        "UTC_TIME select item"
    );
    failures += expect_node(
        child_at(child_at(select_list, utc_time_call_item_index), 0U),
        MYLITE_SQL_AST_UTC_TIME_VALUE,
        "UTC_TIME() select item"
    );
    failures += expect_node(
        child_at(child_at(select_list, utc_timestamp_item_index), 0U),
        MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE,
        "UTC_TIMESTAMP select item"
    );
    failures += expect_node(
        child_at(child_at(select_list, utc_timestamp_call_item_index), 0U),
        MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE,
        "UTC_TIMESTAMP() select item"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT UTC_DATE (), UTC_TIME (), UTC_TIMESTAMP ();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_node(
        child_at(child_at(select_list, utc_date_item_index), 0U),
        MYLITE_SQL_AST_UTC_DATE_VALUE,
        "UTC_DATE whitespace select item"
    );
    failures += expect_node(
        child_at(child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_UTC_TIME_VALUE,
        "UTC_TIME whitespace select item"
    );
    failures += expect_node(
        child_at(child_at(select_list, 2U), 0U),
        MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE,
        "UTC_TIMESTAMP whitespace select item"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT INTO t VALUES (UTC_DATE(), UTC_TIME(), UTC_TIMESTAMP());",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "INSERT INTO t SET d = UTC_DATE, tm = UTC_TIME, dt = UTC_TIMESTAMP;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "REPLACE INTO t VALUES (UTC_DATE(), UTC_TIME(), UTC_TIMESTAMP());",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "REPLACE INTO t SET d = UTC_DATE, tm = UTC_TIME, dt = UTC_TIMESTAMP;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE t SET d = UTC_DATE, tm = UTC_TIME, dt = UTC_TIMESTAMP;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 1U), 0U);
    failures += expect_node(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_UTC_DATE_VALUE,
        "UTC_DATE update assignment"
    );
    assignment = child_at(child_at(statement, 1U), 1U);
    failures += expect_node(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_UTC_TIME_VALUE,
        "UTC_TIME update assignment"
    );
    assignment = child_at(child_at(statement, 1U), 2U);
    failures += expect_node(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE,
        "UTC_TIMESTAMP update assignment"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT UTC_DATE(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT UTC_TIME(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT UTC_TIMESTAMP(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_create_table_primary_key_statements(void) {
    enum {
        fulltext_item_count = 7U,
        fulltext_key_item_index = 3U,
        fulltext_index_item_index = 4U,
        fulltext_named_item_index = 5U,
        fulltext_unnamed_item_index = 6U,
    };

    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *items = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *primary_key = NULL;
    const struct mylite_sql_ast_node *key_parts = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE inline_pk (id INT PRIMARY KEY, amount BIGINT NOT NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    items = child_at(statement, 1U);
    column = child_at(items, 0U);
    primary_key = child_at(column, 2U);
    failures += expect_node(items, MYLITE_SQL_AST_COLUMN_DEFINITION_LIST, "inline pk item list");
    failures += expect_child_count(items, 2U, "inline pk item count");
    failures += expect_node(column, MYLITE_SQL_AST_COLUMN_DEFINITION, "inline pk column");
    failures += expect_span_text(child_at(column, 0U), "id", "inline pk column name");
    failures += expect_node(primary_key, MYLITE_SQL_AST_INLINE_PRIMARY_KEY, "inline pk marker");
    failures += expect_span_text(primary_key, "PRIMARY KEY", "inline pk marker span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE inline_pk_full (id INT NOT NULL DEFAULT +1 PRIMARY KEY);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    column = child_at(child_at(statement, 1U), 0U);
    failures += expect_nullability(
        child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "inline pk not null"
    );
    failures +=
        expect_node(child_at(column, 3U), MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE, "inline pk default");
    failures +=
        expect_node(child_at(column, 4U), MYLITE_SQL_AST_INLINE_PRIMARY_KEY, "inline pk final");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE auto_inline (id INT AUTO_INCREMENT PRIMARY KEY, amount INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    column = child_at(child_at(child_at(result.root, 0U), 1U), 0U);
    failures += expect_node(
        child_at(column, 2U),
        MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT,
        "inline auto increment marker"
    );
    failures += expect_node(
        child_at(column, 3U),
        MYLITE_SQL_AST_INLINE_PRIMARY_KEY,
        "inline auto increment primary key"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE auto_table_pk (id INT NOT NULL AUTO_INCREMENT, PRIMARY KEY (id));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    column = child_at(child_at(child_at(result.root, 0U), 1U), 0U);
    failures += expect_node(
        child_at(column, 3U),
        MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT,
        "table pk auto increment marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE auto_secondary_key (id INT AUTO_INCREMENT, KEY(id), v INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    items = child_at(statement, 1U);
    column = child_at(items, 0U);
    failures += expect_node(
        child_at(column, 2U),
        MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT,
        "secondary key auto increment marker"
    );
    failures += expect_node(
        child_at(items, 1U),
        MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION,
        "secondary key auto increment index"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE auto_secondary_unique (id INT AUTO_INCREMENT UNIQUE, v INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    column = child_at(child_at(child_at(result.root, 0U), 1U), 0U);
    failures += expect_node(
        child_at(column, 2U),
        MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT,
        "secondary unique auto increment marker"
    );
    failures += expect_node(
        child_at(column, 3U),
        MYLITE_SQL_AST_INLINE_UNIQUE_KEY,
        "secondary unique auto increment key"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE auto_option (id INT PRIMARY KEY AUTO_INCREMENT) AUTO_INCREMENT=7;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    items = child_at(statement, 1U);
    column = child_at(items, 0U);
    failures += expect_node(
        child_at(column, 2U),
        MYLITE_SQL_AST_INLINE_PRIMARY_KEY,
        "auto option primary key before auto increment"
    );
    failures += expect_node(
        child_at(column, 3U),
        MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT,
        "auto option column marker"
    );
    failures += expect_node(
        child_at(statement, 2U),
        MYLITE_SQL_AST_TABLE_OPTION_LIST,
        "auto increment table option list"
    );
    failures += expect_node(
        child_at(child_at(statement, 2U), 0U),
        MYLITE_SQL_AST_TABLE_AUTO_INCREMENT_OPTION,
        "auto increment table option"
    );
    failures += expect_literal(
        child_at(child_at(child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "auto increment table option value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE table_pk (id INT, PRIMARY KEY (id));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    items = child_at(statement, 1U);
    primary_key = child_at(items, 1U);
    key_parts = child_at(primary_key, 0U);
    failures += expect_child_count(items, 2U, "table pk item count");
    failures += expect_node(primary_key, MYLITE_SQL_AST_PRIMARY_KEY_DEFINITION, "table pk");
    failures += expect_span_text(primary_key, "PRIMARY KEY (id)", "table pk span");
    failures += expect_node(key_parts, MYLITE_SQL_AST_PRIMARY_KEY_PART_LIST, "table pk parts");
    failures += expect_child_count(key_parts, 1U, "table pk part count");
    failures += expect_span_text(child_at(key_parts, 0U), "id", "table pk part");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE parser_composite_pk (a INT, b INT, PRIMARY KEY (a, `b`));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    primary_key = child_at(child_at(child_at(result.root, 0U), 1U), 2U);
    key_parts = child_at(primary_key, 0U);
    failures += expect_child_count(key_parts, 2U, "composite pk parser part count");
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 0U), "a", "composite pk first part");
    failures +=
        expect_span_text(child_at(child_at(key_parts, 1U), 0U), "`b`", "composite pk second part");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE parser_qualified_pk (id INT, PRIMARY KEY (parser_qualified_pk.id));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE parser_qualified_pk_desc (id INT, PRIMARY KEY (parser_qualified_pk_desc.id "
        "DESC));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE secondary_keys (id INT PRIMARY KEY, v INT, KEY k_v (v), INDEX (id));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    items = child_at(statement, 1U);
    failures += expect_child_count(items, 4U, "secondary key item count");
    failures += expect_node(
        child_at(items, 2U),
        MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION,
        "named secondary key"
    );
    failures += expect_span_text(child_at(child_at(items, 2U), 0U), "k_v", "secondary key name");
    key_parts = child_at(child_at(items, 2U), 1U);
    failures +=
        expect_node(key_parts, MYLITE_SQL_AST_SECONDARY_INDEX_PART_LIST, "secondary key parts");
    failures += expect_child_count(key_parts, 1U, "secondary key part count");
    failures += expect_node(
        child_at(key_parts, 0U),
        MYLITE_SQL_AST_SECONDARY_INDEX_PART,
        "secondary key part"
    );
    failures += expect_span_text(child_at(child_at(key_parts, 0U), 0U), "v", "secondary key part");
    failures += expect_node(
        child_at(items, 3U),
        MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION,
        "unnamed secondary index"
    );
    key_parts = child_at(child_at(items, 3U), 0U);
    failures += expect_child_count(key_parts, 1U, "unnamed secondary key part count");
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 0U), "id", "unnamed secondary key part");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE parser_composite_secondary (a INT, b INT, KEY k_ab (a, b));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    key_parts = child_at(child_at(child_at(result.root, 0U), 1U), 2U);
    key_parts = child_at(key_parts, 1U);
    failures += expect_child_count(key_parts, 2U, "composite secondary parser part count");
    failures += expect_span_text(
        child_at(child_at(key_parts, 0U), 0U),
        "a",
        "composite secondary first part"
    );
    failures += expect_span_text(
        child_at(child_at(key_parts, 1U), 0U),
        "b",
        "composite secondary second part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_named_pk (id INT, CONSTRAINT pk PRIMARY KEY (id));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unique_keys (id INT UNIQUE, n INT UNIQUE KEY, UNIQUE KEY u_id (id), "
        "UNIQUE INDEX (n));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    items = child_at(statement, 1U);
    failures += expect_child_count(items, 4U, "unique key item count");
    failures += expect_node(
        child_at(child_at(items, 0U), 2U),
        MYLITE_SQL_AST_INLINE_UNIQUE_KEY,
        "inline unique attribute"
    );
    failures += expect_node(
        child_at(child_at(items, 1U), 2U),
        MYLITE_SQL_AST_INLINE_UNIQUE_KEY,
        "inline unique key attribute"
    );
    failures += expect_node(
        child_at(items, 2U),
        MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION,
        "named unique key"
    );
    failures += expect_span_text(child_at(child_at(items, 2U), 0U), "u_id", "unique key name");
    key_parts = child_at(child_at(items, 2U), 1U);
    failures +=
        expect_node(key_parts, MYLITE_SQL_AST_SECONDARY_INDEX_PART_LIST, "unique key parts");
    failures += expect_child_count(key_parts, 1U, "unique key part count");
    failures += expect_span_text(child_at(child_at(key_parts, 0U), 0U), "id", "unique key part");
    failures += expect_node(
        child_at(items, 3U),
        MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION,
        "unnamed unique index"
    );
    key_parts = child_at(child_at(items, 3U), 0U);
    failures += expect_child_count(key_parts, 1U, "unnamed unique key part count");
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 0U), "n", "unnamed unique key part");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE named_unique_constraints ("
        "a INT, b INT, c VARCHAR(20), body TEXT, "
        "CONSTRAINT uq_a UNIQUE (a), "
        "CONSTRAINT uq_b UNIQUE KEY (b DESC), "
        "CONSTRAINT UNIQUE INDEX uq_c (c(3), body(2)));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    items = child_at(statement, 1U);

    enum {
        named_unique_constraint_item_count = 7U,
        named_unique_key_item_index = 5U,
        named_unique_explicit_item_index = 6U,
    };

    failures += expect_child_count(
        items,
        named_unique_constraint_item_count,
        "named unique constraint item count"
    );
    failures += expect_node(
        child_at(items, 4U),
        MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION,
        "named unique constraint definition"
    );
    failures += expect_span_text(child_at(child_at(items, 4U), 0U), "uq_a", "named unique name");
    key_parts = child_at(child_at(items, 4U), 1U);
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 0U), "a", "named unique key part");
    failures += expect_span_text(
        child_at(child_at(items, named_unique_key_item_index), 0U),
        "uq_b",
        "named unique key name"
    );
    key_parts = child_at(child_at(items, named_unique_key_item_index), 1U);
    failures += expect_order_direction(
        child_at(child_at(key_parts, 0U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "named unique desc key part"
    );
    failures += expect_span_text(
        child_at(child_at(items, named_unique_explicit_item_index), 0U),
        "uq_c",
        "constraint unique index explicit name"
    );
    key_parts = child_at(child_at(items, named_unique_explicit_item_index), 1U);
    failures += expect_child_count(key_parts, 2U, "constraint unique prefix part count");
    failures += expect_span_text(
        child_at(child_at(key_parts, 0U), 1U),
        "3",
        "constraint unique first prefix"
    );
    failures += expect_span_text(
        child_at(child_at(key_parts, 1U), 1U),
        "2",
        "constraint unique second prefix"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE explicit_named_unique (a INT, CONSTRAINT ignored UNIQUE KEY visible (a));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    items = child_at(child_at(result.root, 0U), 1U);
    failures += expect_span_text(
        child_at(child_at(items, 1U), 0U),
        "visible",
        "constraint unique explicit visible index name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE explicit_no_keyword_name (a INT, CONSTRAINT c UNIQUE visible (a));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    items = child_at(child_at(result.root, 0U), 1U);
    failures += expect_span_text(
        child_at(child_at(items, 1U), 0U),
        "visible",
        "constraint unique no-keyword explicit visible index name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE secondary_prefix (id INT, name VARCHAR(20), KEY k_name (name(4)));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    key_parts = child_at(child_at(child_at(result.root, 0U), 1U), 2U);
    key_parts = child_at(key_parts, 1U);
    failures += expect_child_count(key_parts, 1U, "secondary prefix parser part count");
    failures += expect_node(
        child_at(key_parts, 0U),
        MYLITE_SQL_AST_SECONDARY_INDEX_PART,
        "secondary prefix key part"
    );
    failures += expect_span_text(
        child_at(child_at(key_parts, 0U), 0U),
        "name",
        "secondary prefix key part column"
    );
    failures += expect_span_text(
        child_at(child_at(key_parts, 0U), 1U),
        "4",
        "secondary prefix key part length"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE secondary_desc (id INT, v VARCHAR(20), KEY k (id DESC), "
        "KEY k_v (v(4) ASC));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    key_parts = child_at(child_at(child_at(result.root, 0U), 1U), 2U);
    key_parts = child_at(key_parts, 1U);
    failures += expect_order_direction(
        child_at(child_at(key_parts, 0U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "secondary desc key part"
    );
    key_parts = child_at(child_at(child_at(result.root, 0U), 1U), 3U);
    key_parts = child_at(key_parts, 1U);
    failures += expect_order_direction(
        child_at(child_at(key_parts, 0U), 2U),
        MYLITE_SQL_AST_ORDER_DIRECTION_ASC,
        "secondary prefix asc key part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE fulltext_keys (id INT, body TEXT, title VARCHAR(20), "
        "FULLTEXT KEY ft_body (body), FULLTEXT INDEX ft_title (title), "
        "FULLTEXT ft_named (body(10)), FULLTEXT (title DESC));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    items = child_at(child_at(result.root, 0U), 1U);
    failures += expect_child_count(items, fulltext_item_count, "fulltext item count");
    failures += expect_node(
        child_at(items, fulltext_key_item_index),
        MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION,
        "fulltext key definition"
    );
    failures += expect_span_text(
        child_at(child_at(items, fulltext_key_item_index), 0U),
        "ft_body",
        "fulltext key name"
    );
    key_parts = child_at(child_at(items, fulltext_key_item_index), 1U);
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 0U), "body", "fulltext key part");
    failures += expect_node(
        child_at(items, fulltext_index_item_index),
        MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION,
        "fulltext index definition"
    );
    failures += expect_span_text(
        child_at(child_at(items, fulltext_index_item_index), 0U),
        "ft_title",
        "fulltext index name"
    );
    key_parts = child_at(child_at(items, fulltext_index_item_index), 1U);
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 0U), "title", "fulltext index part");
    failures += expect_node(
        child_at(items, fulltext_named_item_index),
        MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION,
        "named fulltext definition"
    );
    failures += expect_span_text(
        child_at(child_at(items, fulltext_named_item_index), 0U),
        "ft_named",
        "named fulltext name"
    );
    key_parts = child_at(child_at(items, fulltext_named_item_index), 1U);
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 1U), "10", "fulltext ignored prefix");
    failures += expect_node(
        child_at(items, fulltext_unnamed_item_index),
        MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION,
        "unnamed fulltext definition"
    );
    key_parts = child_at(child_at(items, fulltext_unnamed_item_index), 0U);
    failures += expect_order_direction(
        child_at(child_at(key_parts, 0U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "fulltext explicit order parser part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE fulltext_no_semicolon (body TEXT, FULLTEXT KEY ft_body (body))",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_constraint_fulltext (body TEXT, "
        "CONSTRAINT c FULLTEXT KEY ft_body (body));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_secondary_using (id INT, KEY k USING BTREE (id));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    items = child_at(child_at(result.root, 0U), 1U);
    failures += expect_node(
        child_at(child_at(items, 1U), 1U),
        MYLITE_SQL_AST_INDEX_TYPE_OPTION,
        "secondary index type option"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(items, 1U), 1U), 0U),
        "BTREE",
        "secondary index type option name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_pk_prefix (id INT, PRIMARY KEY (id(4)));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE pk_order (id INT, PRIMARY KEY (id DESC));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    primary_key = child_at(child_at(child_at(result.root, 0U), 1U), 1U);
    key_parts = child_at(primary_key, 0U);
    failures += expect_order_direction(
        child_at(child_at(key_parts, 0U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "primary key desc parser part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_pk_using (id INT, PRIMARY KEY USING BTREE (id));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_create_table_foreign_key_statements(void) {
    enum {
        named_indexed_fk_child_count = 6,
        named_indexed_fk_action_index = 5,
    };

    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *items = NULL;
    const struct mylite_sql_ast_node *foreign_key = NULL;
    const struct mylite_sql_ast_node *actions = NULL;
    const struct mylite_sql_ast_node *child_parts = NULL;
    const struct mylite_sql_ast_node *parent_parts = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE child (id INT, parent_id INT, CONSTRAINT fk_child_parent "
        "FOREIGN KEY (parent_id) REFERENCES parent (id));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    items = child_at(statement, 1U);
    foreign_key = child_at(items, 2U);
    failures += expect_node(
        foreign_key,
        MYLITE_SQL_AST_FOREIGN_KEY_DEFINITION,
        "named create foreign key definition"
    );
    failures += expect_child_count(foreign_key, 4U, "named create foreign key child count");
    failures += expect_span_text(child_at(foreign_key, 0U), "fk_child_parent", "fk name");
    child_parts = child_at(foreign_key, 1U);
    parent_parts = child_at(foreign_key, 3U);
    failures +=
        expect_node(child_parts, MYLITE_SQL_AST_FOREIGN_KEY_PART_LIST, "fk child part list");
    failures += expect_span_text(child_at(child_parts, 0U), "parent_id", "fk child part");
    failures += expect_span_text(child_at(foreign_key, 2U), "parent", "fk parent table");
    failures +=
        expect_node(parent_parts, MYLITE_SQL_AST_FOREIGN_KEY_PART_LIST, "fk parent part list");
    failures += expect_span_text(child_at(parent_parts, 0U), "id", "fk parent part");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE child (parent_id INT, FOREIGN KEY (`parent_id`) REFERENCES app.parent "
        "(`id`));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    foreign_key = child_at(child_at(child_at(result.root, 0U), 1U), 1U);
    failures += expect_child_count(foreign_key, 3U, "unnamed create foreign key child count");
    child_parts = child_at(foreign_key, 0U);
    parent_parts = child_at(foreign_key, 2U);
    failures += expect_span_text(child_at(child_parts, 0U), "`parent_id`", "quoted fk child part");
    failures += expect_span_text(child_at(foreign_key, 1U), "app.parent", "qualified fk parent");
    failures += expect_span_text(child_at(parent_parts, 0U), "`id`", "quoted fk parent part");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE child (parent_id INT, FOREIGN KEY parent_idx (`parent_id`) "
        "REFERENCES app.parent (`id`));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    foreign_key = child_at(child_at(child_at(result.root, 0U), 1U), 1U);
    failures += expect_child_count(foreign_key, 4U, "indexed create foreign key child count");
    failures += expect_node(
        child_at(foreign_key, 0U),
        MYLITE_SQL_AST_FOREIGN_KEY_INDEX_NAME,
        "fk index name wrapper"
    );
    failures +=
        expect_span_text(child_at(child_at(foreign_key, 0U), 0U), "parent_idx", "fk index name");
    failures += expect_span_text(child_at(foreign_key, 2U), "app.parent", "indexed fk parent");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE child (a INT, b INT, FOREIGN KEY (a, b) REFERENCES parent (a, b));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    foreign_key = child_at(child_at(child_at(result.root, 0U), 1U), 2U);
    child_parts = child_at(foreign_key, 0U);
    parent_parts = child_at(foreign_key, 2U);
    failures += expect_child_count(child_parts, 2U, "composite fk child parser parts");
    failures += expect_child_count(parent_parts, 2U, "composite fk parent parser parts");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE child (parent_id INT, FOREIGN KEY (parent_id) REFERENCES parent);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE child (parent_id INT, FOREIGN KEY (parent_id) REFERENCES parent (id) "
        "ON DELETE CASCADE);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    foreign_key = child_at(child_at(child_at(result.root, 0U), 1U), 1U);
    failures += expect_child_count(foreign_key, 4U, "create foreign key action child count");
    actions = child_at(foreign_key, 3U);
    failures += expect_node(actions, MYLITE_SQL_AST_FOREIGN_KEY_ACTION_LIST, "fk action list");
    failures += expect_child_count(actions, 1U, "fk action count");
    failures += expect_node(
        child_at(actions, 0U),
        MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_CASCADE,
        "delete cascade action"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE child (parent_id INT, CONSTRAINT fk FOREIGN KEY idx_parent (parent_id) "
        "REFERENCES parent (id) ON UPDATE RESTRICT ON DELETE NO ACTION);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    foreign_key = child_at(child_at(child_at(result.root, 0U), 1U), 1U);
    failures += expect_child_count(
        foreign_key,
        named_indexed_fk_child_count,
        "named indexed create fk action child count"
    );
    failures += expect_span_text(child_at(foreign_key, 0U), "fk", "named indexed fk name");
    failures += expect_node(
        child_at(foreign_key, 1U),
        MYLITE_SQL_AST_FOREIGN_KEY_INDEX_NAME,
        "named indexed fk index"
    );
    actions = child_at(foreign_key, named_indexed_fk_action_index);
    failures += expect_child_count(actions, 2U, "named indexed fk action count");
    failures += expect_node(
        child_at(actions, 0U),
        MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_RESTRICT,
        "update restrict action"
    );
    failures += expect_node(
        child_at(actions, 1U),
        MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_NO_ACTION,
        "delete no action action"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE child (parent_id INT, FOREIGN KEY (parent_id) REFERENCES parent (id) "
        "ON DELETE SET NULL ON UPDATE SET NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    foreign_key = child_at(child_at(child_at(result.root, 0U), 1U), 1U);
    actions = child_at(foreign_key, 3U);
    failures += expect_child_count(actions, 2U, "set null fk action count");
    failures += expect_node(
        child_at(actions, 0U),
        MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_SET_NULL,
        "delete set null action"
    );
    failures += expect_node(
        child_at(actions, 1U),
        MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_SET_NULL,
        "update set null action"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE child (parent_id INT, FOREIGN KEY (parent_id) REFERENCES parent (id) "
        "ON DELETE SET DEFAULT);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_add_primary_key_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *primary_key = NULL;
    const struct mylite_sql_ast_node *key_parts = NULL;
    int failures = 0;

    failures += parse_sql("ALTER TABLE add_pk ADD PRIMARY KEY (id);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT,
        "alter add primary key statement"
    );
    failures += expect_child_count(statement, 2U, "alter add primary key child count");
    failures += expect_span_text(child_at(statement, 0U), "add_pk", "alter add primary key table");
    primary_key = child_at(statement, 1U);
    key_parts = child_at(primary_key, 0U);
    failures += expect_node(
        primary_key,
        MYLITE_SQL_AST_PRIMARY_KEY_DEFINITION,
        "alter add primary key definition"
    );
    failures += expect_span_text(primary_key, "PRIMARY KEY (id)", "alter add primary key span");
    failures += expect_child_count(key_parts, 1U, "alter add primary key part count");
    failures += expect_span_text(child_at(key_parts, 0U), "id", "alter add primary key part");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE add_pk ADD PRIMARY KEY (id), ALGORITHM=COPY, LOCK=EXCLUSIVE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY,
        "alter add primary key algorithm option"
    );
    failures += expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_EXCLUSIVE,
        "alter add primary key lock option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE app.add_pk ADD PRIMARY KEY (`id`);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.add_pk",
        "schema-qualified alter add primary key table"
    );
    primary_key = child_at(statement, 1U);
    key_parts = child_at(primary_key, 0U);
    failures += expect_span_text(
        child_at(child_at(key_parts, 0U), 0U),
        "`id`",
        "quoted alter add primary key part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE add_pk ADD PRIMARY KEY (id, other);", MYLITE_SQL_PARSE_OK, &result);
    key_parts = child_at(child_at(child_at(result.root, 0U), 1U), 0U);
    failures += expect_child_count(key_parts, 2U, "alter add composite pk parser part count");
    failures += expect_span_text(
        child_at(child_at(key_parts, 1U), 0U),
        "other",
        "alter add composite pk part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE add_pk ADD PRIMARY KEY (add_pk.id);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE add_pk ADD PRIMARY KEY (add_pk.id DESC);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE add_pk ADD PRIMARY KEY (id DESC, other ASC);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    key_parts = child_at(child_at(child_at(result.root, 0U), 1U), 0U);
    failures += expect_order_direction(
        child_at(child_at(key_parts, 0U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "alter add primary key desc part"
    );
    failures += expect_order_direction(
        child_at(child_at(key_parts, 1U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_ASC,
        "alter add primary key asc part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE add_pk ADD CONSTRAINT pk PRIMARY KEY (id);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE add_pk ADD PRIMARY KEY USING BTREE (id);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE add_pk ADD PRIMARY KEY (id), ADD KEY k_v (v);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_create_index_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *key_parts = NULL;
    int failures = 0;

    failures += parse_sql("CREATE INDEX k_v ON create_idx (v);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_INDEX_STATEMENT, "create index");
    failures += expect_child_count(statement, 3U, "create index child count");
    failures += expect_span_text(child_at(statement, 0U), "k_v", "create index name");
    failures += expect_span_text(child_at(statement, 1U), "create_idx", "create index table");
    key_parts = child_at(statement, 2U);
    failures += expect_child_count(key_parts, 1U, "create index part count");
    failures += expect_span_text(child_at(child_at(key_parts, 0U), 0U), "v", "create index part");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE UNIQUE INDEX `u_v` ON app.create_idx (`v`);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_CREATE_UNIQUE_INDEX_STATEMENT, "create unique index");
    failures += expect_span_text(child_at(statement, 0U), "`u_v`", "create unique index name");
    failures +=
        expect_span_text(child_at(statement, 1U), "app.create_idx", "create unique index table");
    key_parts = child_at(statement, 2U);
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 0U), "`v`", "create unique index part");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE INDEX k_multi ON create_idx (v, id);", MYLITE_SQL_PARSE_OK, &result);
    key_parts = child_at(child_at(result.root, 0U), 2U);
    failures += expect_child_count(key_parts, 2U, "create composite index parser part count");
    failures += expect_span_text(
        child_at(child_at(key_parts, 1U), 0U),
        "id",
        "create composite index part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE INDEX ON create_idx (v);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE INDEX IF NOT EXISTS k_v ON create_idx (v);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE INDEX k_v ON create_idx (create_idx.v);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE INDEX k_v ON create_idx (v(4));", MYLITE_SQL_PARSE_OK, &result);
    key_parts = child_at(child_at(result.root, 0U), 2U);
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 0U), "v", "create index prefix column");
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 1U), "4", "create index prefix length");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE INDEX k_v ON create_idx (v DESC);", MYLITE_SQL_PARSE_OK, &result);
    key_parts = child_at(child_at(result.root, 0U), 2U);
    failures += expect_order_direction(
        child_at(child_at(key_parts, 0U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "create index desc part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE FULLTEXT INDEX ft_v ON create_idx (v(4));", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_FULLTEXT_INDEX_STATEMENT,
        "create fulltext index"
    );
    failures += expect_span_text(child_at(statement, 0U), "ft_v", "create fulltext index name");
    failures +=
        expect_span_text(child_at(statement, 1U), "create_idx", "create fulltext index table");
    key_parts = child_at(statement, 2U);
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 0U), "v", "create fulltext part");
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 1U), "4", "create fulltext prefix");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE FULLTEXT KEY k_v ON create_idx (v);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE INDEX k_v USING BTREE ON create_idx (v);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_child_count(statement, 4U, "create index type child count");
    failures += expect_node(
        child_at(statement, 1U),
        MYLITE_SQL_AST_INDEX_TYPE_OPTION,
        "create index type option"
    );
    failures += expect_span_text(
        child_at(child_at(statement, 1U), 0U),
        "BTREE",
        "create index type option name"
    );
    key_parts = child_at(statement, 3U);
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 0U), "v", "create index type part");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_drop_index_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("DROP INDEX k_v ON drop_idx;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_INDEX_STATEMENT, "drop index");
    failures += expect_child_count(statement, 2U, "drop index child count");
    failures += expect_span_text(child_at(statement, 0U), "k_v", "drop index name");
    failures += expect_span_text(child_at(statement, 1U), "drop_idx", "drop index table");
    failures += expect_span_text(statement, "DROP INDEX k_v ON drop_idx", "drop index span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP INDEX `k_v` ON app.drop_idx;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(child_at(statement, 0U), "`k_v`", "quoted drop index name");
    failures +=
        expect_span_text(child_at(statement, 1U), "app.drop_idx", "qualified drop index table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP INDEX `PRIMARY` ON drop_idx;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(child_at(statement, 0U), "`PRIMARY`", "quoted drop primary name");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("DROP INDEX PRIMARY ON drop_idx;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP KEY k_v ON drop_idx;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("DROP INDEX IF EXISTS k_v ON drop_idx;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DROP INDEX k_v ON drop_idx ALGORITHM=INPLACE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP INDEX k_v;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_add_index_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *secondary_index = NULL;
    const struct mylite_sql_ast_node *key_parts = NULL;
    int failures = 0;

    failures += parse_sql("ALTER TABLE add_idx ADD INDEX k_v (v);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT,
        "alter add index statement"
    );
    failures += expect_child_count(statement, 2U, "alter add index child count");
    failures += expect_span_text(child_at(statement, 0U), "add_idx", "alter add index table");
    secondary_index = child_at(statement, 1U);
    failures += expect_node(
        secondary_index,
        MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION,
        "alter add index definition"
    );
    failures += expect_span_text(secondary_index, "INDEX k_v (v)", "alter add index span");
    failures += expect_span_text(child_at(secondary_index, 0U), "k_v", "alter add index name");
    key_parts = child_at(secondary_index, 1U);
    failures += expect_child_count(key_parts, 1U, "alter add index part count");
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 0U), "v", "alter add index part");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE add_idx ADD INDEX k_v (v), ALGORITHM=INPLACE, LOCK=NONE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE,
        "alter add index algorithm option"
    );
    failures += expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_NONE,
        "alter add index lock option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE app.add_idx ADD KEY (`v`);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.add_idx",
        "schema-qualified alter add index table"
    );
    secondary_index = child_at(statement, 1U);
    key_parts = child_at(secondary_index, 0U);
    failures +=
        expect_child_count(secondary_index, 1U, "unnamed alter add index definition child count");
    failures += expect_span_text(
        child_at(child_at(key_parts, 0U), 0U),
        "`v`",
        "quoted alter add index part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE add_idx ADD KEY k_multi (v, id);", MYLITE_SQL_PARSE_OK, &result);
    key_parts = child_at(child_at(child_at(result.root, 0U), 1U), 1U);
    failures += expect_child_count(key_parts, 2U, "alter add composite index parser part count");
    failures += expect_span_text(
        child_at(child_at(key_parts, 1U), 0U),
        "id",
        "alter add composite index part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE add_idx ADD KEY k_qualified (add_idx.v);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE add_idx ADD UNIQUE KEY u_v (v);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT,
        "alter add unique statement"
    );
    secondary_index = child_at(statement, 1U);
    failures += expect_node(
        secondary_index,
        MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION,
        "alter add unique definition"
    );
    failures += expect_span_text(secondary_index, "UNIQUE KEY u_v (v)", "alter add unique span");
    failures += expect_span_text(child_at(secondary_index, 0U), "u_v", "alter add unique name");
    key_parts = child_at(secondary_index, 1U);
    failures += expect_child_count(key_parts, 1U, "alter add unique part count");
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 0U), "v", "alter add unique part");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE add_idx ADD UNIQUE (`v`);", MYLITE_SQL_PARSE_OK, &result);
    secondary_index = child_at(child_at(result.root, 0U), 1U);
    failures += expect_node(
        secondary_index,
        MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION,
        "unnamed alter add unique definition"
    );
    failures += expect_child_count(secondary_index, 1U, "unnamed alter add unique child count");
    key_parts = child_at(secondary_index, 0U);
    failures += expect_span_text(
        child_at(child_at(key_parts, 0U), 0U),
        "`v`",
        "unnamed alter add unique part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE add_idx ADD UNIQUE INDEX u_v (v);", MYLITE_SQL_PARSE_OK, &result);
    secondary_index = child_at(child_at(result.root, 0U), 1U);
    failures += expect_span_text(
        secondary_index,
        "UNIQUE INDEX u_v (v)",
        "alter add unique index keyword span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE add_idx ADD FULLTEXT KEY ft_v (v(4));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT,
        "alter add fulltext statement"
    );
    secondary_index = child_at(statement, 1U);
    failures += expect_node(
        secondary_index,
        MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION,
        "alter add fulltext definition"
    );
    failures +=
        expect_span_text(secondary_index, "FULLTEXT KEY ft_v (v(4))", "alter add fulltext span");
    key_parts = child_at(secondary_index, 1U);
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 0U), "v", "alter add fulltext part");
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 1U), "4", "alter add fulltext prefix");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE add_idx ADD CONSTRAINT uq_v UNIQUE (v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT,
        "alter add constraint unique statement"
    );
    secondary_index = child_at(statement, 1U);
    failures += expect_node(
        secondary_index,
        MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION,
        "alter add constraint unique definition"
    );
    failures += expect_span_text(secondary_index, "UNIQUE (v)", "alter add constraint unique span");
    failures +=
        expect_span_text(child_at(secondary_index, 0U), "uq_v", "alter add constraint unique name");
    key_parts = child_at(secondary_index, 1U);
    failures += expect_child_count(key_parts, 1U, "alter add constraint unique part count");
    failures += expect_span_text(
        child_at(child_at(key_parts, 0U), 0U),
        "v",
        "alter add constraint unique part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE add_idx ADD CONSTRAINT uq_key UNIQUE KEY visible (v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    secondary_index = child_at(child_at(result.root, 0U), 1U);
    failures += expect_span_text(
        child_at(secondary_index, 0U),
        "visible",
        "alter add constraint unique explicit index name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE add_idx ADD CONSTRAINT UNIQUE (v);", MYLITE_SQL_PARSE_OK, &result);
    secondary_index = child_at(child_at(result.root, 0U), 1U);
    failures +=
        expect_child_count(secondary_index, 1U, "alter add constraint unnamed unique child count");
    key_parts = child_at(secondary_index, 0U);
    failures += expect_span_text(
        child_at(child_at(key_parts, 0U), 0U),
        "v",
        "alter add constraint unnamed unique part"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE add_idx ADD CONSTRAINT uq_key UNIQUE KEY (v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    secondary_index = child_at(child_at(result.root, 0U), 1U);
    failures += expect_span_text(
        child_at(secondary_index, 0U),
        "uq_key",
        "alter add constraint unique key name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE add_idx ADD CONSTRAINT uq_key UNIQUE visible (v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    secondary_index = child_at(child_at(result.root, 0U), 1U);
    failures += expect_span_text(
        child_at(secondary_index, 0U),
        "visible",
        "alter add constraint unique no-keyword explicit index name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE add_idx ADD CONSTRAINT c FULLTEXT KEY ft_v (v);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE add_idx ADD INDEX k_v (v), ADD INDEX k_id (id);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE add_idx ADD INDEX k_v USING BTREE (v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    secondary_index = child_at(child_at(result.root, 0U), 1U);
    failures += expect_node(
        child_at(secondary_index, 1U),
        MYLITE_SQL_AST_INDEX_TYPE_OPTION,
        "alter add index type option"
    );
    failures += expect_span_text(
        child_at(child_at(secondary_index, 1U), 0U),
        "BTREE",
        "alter add index type option name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE add_idx ADD INDEX k_v (v(4));", MYLITE_SQL_PARSE_OK, &result);
    secondary_index = child_at(child_at(result.root, 0U), 1U);
    key_parts = child_at(secondary_index, 1U);
    failures += expect_span_text(
        child_at(child_at(key_parts, 0U), 0U),
        "v",
        "alter add index prefix column"
    );
    failures += expect_span_text(
        child_at(child_at(key_parts, 0U), 1U),
        "4",
        "alter add index prefix length"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE add_idx ADD INDEX k_v (v DESC);", MYLITE_SQL_PARSE_OK, &result);
    secondary_index = child_at(child_at(result.root, 0U), 1U);
    key_parts = child_at(secondary_index, 1U);
    failures += expect_order_direction(
        child_at(child_at(key_parts, 0U), 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "alter add index desc part"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_add_foreign_key_statements(void) {
    enum {
        alter_fk_action_child_count = 5,
        alter_fk_action_list_index = 4,
    };

    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *foreign_key = NULL;
    const struct mylite_sql_ast_node *actions = NULL;
    const struct mylite_sql_ast_node *child_parts = NULL;
    const struct mylite_sql_ast_node *parent_parts = NULL;
    int failures = 0;

    failures += parse_sql(
        "ALTER TABLE child ADD CONSTRAINT fk_child_parent FOREIGN KEY (parent_id) "
        "REFERENCES parent (id);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_FOREIGN_KEY_STATEMENT,
        "alter add foreign key statement"
    );
    failures += expect_child_count(statement, 2U, "alter add foreign key child count");
    failures += expect_span_text(child_at(statement, 0U), "child", "alter fk child table");
    foreign_key = child_at(statement, 1U);
    failures +=
        expect_node(foreign_key, MYLITE_SQL_AST_FOREIGN_KEY_DEFINITION, "alter fk definition");
    failures += expect_child_count(foreign_key, 4U, "alter fk definition child count");
    failures += expect_span_text(child_at(foreign_key, 0U), "fk_child_parent", "alter fk name");
    child_parts = child_at(foreign_key, 1U);
    parent_parts = child_at(foreign_key, 3U);
    failures += expect_span_text(child_at(child_parts, 0U), "parent_id", "alter fk child part");
    failures += expect_span_text(child_at(foreign_key, 2U), "parent", "alter fk parent table");
    failures += expect_span_text(child_at(parent_parts, 0U), "id", "alter fk parent part");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE child ADD FOREIGN KEY (parent_id) REFERENCES parent (id), "
        "ALGORITHM=INPLACE, LOCK=NONE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE,
        "alter add foreign key algorithm option"
    );
    failures += expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_NONE,
        "alter add foreign key lock option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.child ADD CONSTRAINT `fk` FOREIGN KEY (`parent_id`) "
        "REFERENCES app.parent (`id`);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    foreign_key = child_at(statement, 1U);
    failures += expect_span_text(child_at(statement, 0U), "app.child", "qualified alter fk child");
    failures += expect_span_text(child_at(foreign_key, 0U), "`fk`", "quoted alter fk name");
    failures +=
        expect_span_text(child_at(child_at(foreign_key, 1U), 0U), "`parent_id`", "quoted child");
    failures += expect_span_text(child_at(foreign_key, 2U), "app.parent", "qualified parent");
    failures += expect_span_text(child_at(child_at(foreign_key, 3U), 0U), "`id`", "quoted parent");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE child ADD FOREIGN KEY (parent_id) REFERENCES parent (id);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    foreign_key = child_at(statement, 1U);
    failures += expect_child_count(foreign_key, 3U, "unnamed alter fk definition child count");
    failures += expect_span_text(child_at(statement, 0U), "child", "unnamed alter fk child table");
    failures += expect_span_text(child_at(foreign_key, 1U), "parent", "unnamed alter fk parent");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE child ADD FOREIGN KEY idx_parent (parent_id) REFERENCES parent (id);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    foreign_key = child_at(statement, 1U);
    failures += expect_child_count(foreign_key, 4U, "indexed alter fk definition child count");
    failures += expect_node(
        child_at(foreign_key, 0U),
        MYLITE_SQL_AST_FOREIGN_KEY_INDEX_NAME,
        "indexed alter fk index node"
    );
    failures +=
        expect_span_text(child_at(child_at(foreign_key, 0U), 0U), "idx_parent", "alter fk index");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE child ADD CONSTRAINT fk FOREIGN KEY (parent_id) REFERENCES parent (id) "
        "ON UPDATE CASCADE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    foreign_key = child_at(statement, 1U);
    failures +=
        expect_child_count(foreign_key, alter_fk_action_child_count, "alter fk action child count");
    actions = child_at(foreign_key, alter_fk_action_list_index);
    failures += expect_node(
        child_at(actions, 0U),
        MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_CASCADE,
        "alter update cascade action"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE child ADD CONSTRAINT fk FOREIGN KEY (parent_id) REFERENCES parent (id) "
        "ON UPDATE SET DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_drop_foreign_key_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql(
        "ALTER TABLE child DROP FOREIGN KEY fk_child_parent;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_FOREIGN_KEY_STATEMENT,
        "alter drop foreign key statement"
    );
    failures += expect_child_count(statement, 2U, "alter drop foreign key child count");
    failures += expect_span_text(child_at(statement, 0U), "child", "alter drop fk table");
    failures += expect_span_text(child_at(statement, 1U), "fk_child_parent", "alter drop fk name");
    failures += expect_span_text(
        statement,
        "ALTER TABLE child DROP FOREIGN KEY fk_child_parent",
        "alter drop fk span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.child DROP FOREIGN KEY `MiXeD_FK`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_span_text(child_at(statement, 0U), "app.child", "qualified alter drop fk table");
    failures +=
        expect_span_text(child_at(statement, 1U), "`MiXeD_FK`", "quoted alter drop fk name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE child DROP FOREIGN KEY IF EXISTS fk_child_parent;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE child DROP FOREIGN KEY fk_child_parent, DROP INDEX fk_child_parent;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE child DROP FOREIGN KEY fk_child_parent, ALGORITHM=INPLACE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE,
        "alter drop foreign key algorithm option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE child DROP CONSTRAINT fk_child_parent;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_drop_index_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("ALTER TABLE drop_idx DROP INDEX k_v;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_INDEX_STATEMENT,
        "alter drop index statement"
    );
    failures += expect_child_count(statement, 2U, "alter drop index child count");
    failures += expect_span_text(child_at(statement, 0U), "drop_idx", "alter drop index table");
    failures += expect_span_text(child_at(statement, 1U), "k_v", "alter drop index name");
    failures +=
        expect_span_text(statement, "ALTER TABLE drop_idx DROP INDEX k_v", "alter drop index span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE app.drop_idx DROP KEY `k_v`;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.drop_idx",
        "schema-qualified alter drop index table"
    );
    failures += expect_span_text(child_at(statement, 1U), "`k_v`", "quoted alter drop key name");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE drop_idx DROP INDEX `PRIMARY`;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(child_at(statement, 1U), "`PRIMARY`", "quoted primary drop name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE drop_idx DROP INDEX PRIMARY;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE drop_idx DROP INDEX k_v, ADD INDEX k_id (id);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE drop_idx DROP INDEX k_v, ALGORITHM=INPLACE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE,
        "alter drop index algorithm option"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_rename_index_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX k_old TO k_new;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_RENAME_INDEX_STATEMENT,
        "alter rename index statement"
    );
    failures += expect_child_count(statement, 3U, "alter rename index child count");
    failures += expect_span_text(child_at(statement, 0U), "rename_idx", "alter rename index table");
    failures += expect_span_text(child_at(statement, 1U), "k_old", "alter rename old name");
    failures += expect_span_text(child_at(statement, 2U), "k_new", "alter rename new name");
    failures += expect_span_text(
        statement,
        "ALTER TABLE rename_idx RENAME INDEX k_old TO k_new",
        "alter rename index span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.rename_idx RENAME KEY `k_old` TO `k_new`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.rename_idx",
        "schema-qualified alter rename index table"
    );
    failures += expect_span_text(child_at(statement, 1U), "`k_old`", "quoted rename old name");
    failures += expect_span_text(child_at(statement, 2U), "`k_new`", "quoted rename new name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX `PRIMARY` TO `renamed`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_span_text(child_at(statement, 1U), "`PRIMARY`", "quoted primary old name");
    failures += expect_span_text(child_at(statement, 2U), "`renamed`", "quoted renamed name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX k_old TO `PRIMARY`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_span_text(child_at(statement, 2U), "`PRIMARY`", "quoted primary new name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX PRIMARY TO k_new;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX k_old TO PRIMARY;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX rename_idx.k_old TO k_new;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX k_old TO rename_idx.k_new;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX k_old k_new;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX k_old TO k_new, RENAME INDEX k2 TO k3;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX k_old TO k_new, ALGORITHM=INPLACE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE,
        "alter rename index algorithm option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE rename_idx RENAME INDEX k_old TO k_new, LOCK=NONE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_NONE,
        "alter rename index lock option"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_index_visibility_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql(
        "ALTER TABLE idx_visibility ALTER INDEX k_v INVISIBLE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_INDEX_VISIBILITY_STATEMENT,
        "alter index invisible statement"
    );
    failures += expect_child_count(statement, 2U, "alter index invisible child count");
    failures += expect_span_text(child_at(statement, 0U), "idx_visibility", "alter index table");
    failures += expect_span_text(child_at(statement, 1U), "k_v", "alter index name");
    failures += expect_column_visibility(
        statement,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_INVISIBLE,
        "alter index invisible payload"
    );
    failures += expect_span_text(
        statement,
        "ALTER TABLE idx_visibility ALTER INDEX k_v INVISIBLE",
        "alter index invisible span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.idx_visibility ALTER INDEX `k_v` VISIBLE, ALGORITHM=COPY, LOCK=NONE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.idx_visibility",
        "schema-qualified alter index visibility table"
    );
    failures += expect_span_text(child_at(statement, 1U), "`k_v`", "quoted alter index name");
    failures += expect_column_visibility(
        statement,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE,
        "alter index visible payload"
    );
    failures += expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY,
        "alter index visibility algorithm option"
    );
    failures += expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_NONE,
        "alter index visibility lock option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE idx_visibility ALTER INDEX `PRIMARY` INVISIBLE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_span_text(child_at(statement, 1U), "`PRIMARY`", "quoted primary index");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE idx_visibility ALTER INDEX PRIMARY INVISIBLE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE idx_visibility ALTER KEY k_v INVISIBLE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE idx_visibility ALTER INDEX idx_visibility.k_v INVISIBLE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE idx_visibility ALTER INDEX k_v INVISIBLE, ALTER INDEX k2 VISIBLE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_check_constraint_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *check = NULL;
    int failures = 0;

    failures += parse_sql("ALTER TABLE checked ADD CHECK (a > 0);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    check = child_at(statement, 1U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_CHECK_STATEMENT,
        "alter add check statement"
    );
    failures += expect_child_count(statement, 2U, "alter add check child count");
    failures += expect_span_text(child_at(statement, 0U), "checked", "alter add check table");
    failures += expect_node(check, MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION, "added CHECK");
    failures += expect_span_text(statement, "ALTER TABLE checked ADD CHECK (a > 0)", "add span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE app.checked ADD CONSTRAINT positive CHECK ((a > 0) AND (b IS NOT NULL)) "
        "NOT ENFORCED;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    check = child_at(statement, 1U);
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.checked",
        "schema-qualified alter add check table"
    );
    failures += expect_span_text(child_at(check, 1U), "positive", "explicit alter CHECK name");
    failures += expect_node(
        child_at(check, 2U),
        MYLITE_SQL_AST_CHECK_ENFORCEMENT_NOT_ENFORCED,
        "alter CHECK not enforced"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE checked DROP CHECK positive;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_CHECK_STATEMENT,
        "alter drop check statement"
    );
    failures += expect_child_count(statement, 2U, "alter drop check child count");
    failures += expect_span_text(child_at(statement, 1U), "positive", "alter drop check name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE checked ALTER CHECK positive NOT ENFORCED;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ALTER_CHECK_STATEMENT,
        "alter alter check statement"
    );
    failures += expect_child_count(statement, 3U, "alter alter check child count");
    failures += expect_node(
        child_at(statement, 2U),
        MYLITE_SQL_AST_CHECK_ENFORCEMENT_NOT_ENFORCED,
        "alter check not enforced"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE checked ALTER CHECK positive ENFORCED;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        child_at(statement, 2U),
        MYLITE_SQL_AST_CHECK_ENFORCEMENT_ENFORCED,
        "alter check enforced"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE checked DROP CONSTRAINT positive;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE checked ALTER CHECK positive;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE checked ADD CHECK (a > 0), ADD CHECK (b > 0);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_drop_primary_key_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("ALTER TABLE drop_pk DROP PRIMARY KEY;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_PRIMARY_KEY_STATEMENT,
        "alter drop primary key statement"
    );
    failures += expect_child_count(statement, 1U, "alter drop primary key child count");
    failures += expect_span_text(child_at(statement, 0U), "drop_pk", "alter drop primary table");
    failures +=
        expect_span_text(statement, "ALTER TABLE drop_pk DROP PRIMARY KEY", "alter drop pk span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE drop_pk DROP PRIMARY KEY, ALGORITHM=COPY, LOCK=EXCLUSIVE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY,
        "alter drop primary key algorithm option"
    );
    failures += expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_EXCLUSIVE,
        "alter drop primary key lock option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE app.drop_pk DROP PRIMARY KEY;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.drop_pk",
        "schema-qualified alter drop primary key table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE drop_pk DROP PRIMARY KEY, ADD KEY k_v (v);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE drop_pk DROP INDEX PRIMARY;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE drop_pk DROP CONSTRAINT `PRIMARY`;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_auto_increment_option_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *option = NULL;
    int failures = 0;

    failures +=
        parse_sql("ALTER TABLE auto_counter AUTO_INCREMENT=10;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    option = child_at(statement, 1U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_AUTO_INCREMENT_STATEMENT,
        "alter table auto increment statement"
    );
    failures += expect_child_count(statement, 2U, "alter table auto increment child count");
    failures +=
        expect_span_text(child_at(statement, 0U), "auto_counter", "alter auto increment table");
    failures +=
        expect_node(option, MYLITE_SQL_AST_TABLE_AUTO_INCREMENT_OPTION, "alter auto option");
    failures +=
        expect_literal(child_at(option, 0U), MYLITE_SQL_AST_LITERAL_INTEGER, "alter auto value");
    failures += expect_span_text(child_at(option, 0U), "10", "alter auto value span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE app.auto_counter AUTO_INCREMENT 0;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    option = child_at(statement, 1U);
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.auto_counter",
        "schema-qualified alter auto increment table"
    );
    failures += expect_span_text(child_at(option, 0U), "0", "alter auto increment zero value");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE auto_counter AUTO_INCREMENT=-1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE auto_counter AUTO_INCREMENT=+1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE auto_counter AUTO_INCREMENT='10';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE auto_counter AUTO_INCREMENT=NULL;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE auto_counter AUTO_INCREMENT=1.5;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE auto_counter AUTO_INCREMENT=10, ADD COLUMN other INT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_create_table_like_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *table_name = NULL;
    const struct mylite_sql_ast_node *source_table = NULL;
    const struct mylite_sql_ast_node *if_not_exists = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE IF NOT EXISTS app.clone LIKE other.source;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_name = child_at(statement, 0U);
    source_table = child_at(statement, 1U);
    if_not_exists = child_at(statement, 2U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_LIKE_STATEMENT,
        "create table like statement"
    );
    failures += expect_node(table_name, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "like target");
    failures += expect_span_text(child_at(table_name, 0U), "app", "like target schema");
    failures += expect_span_text(child_at(table_name, 1U), "clone", "like target table");
    failures += expect_node(source_table, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "like source");
    failures += expect_span_text(child_at(source_table, 0U), "other", "like source schema");
    failures += expect_span_text(child_at(source_table, 1U), "source", "like source table");
    failures += expect_node(
        if_not_exists,
        MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE,
        "like if not exists clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE clone (LIKE source);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    table_name = child_at(statement, 0U);
    source_table = child_at(statement, 1U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_LIKE_STATEMENT,
        "parenthesized create table like statement"
    );
    failures += expect_span_text(table_name, "clone", "parenthesized like target");
    failures += expect_span_text(source_table, "source", "parenthesized like source");
    failures += expect_child_count(statement, 2U, "parenthesized like child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE t (LIKE source, extra INT);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE t LIKE source ENGINE=InnoDB;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TEMPORARY TABLE IF NOT EXISTS app.temp_clone LIKE other.source;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_name = child_at(statement, 0U);
    source_table = child_at(statement, 1U);
    if_not_exists = child_at(statement, 2U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_LIKE_STATEMENT,
        "create temporary table like statement"
    );
    failures += expect_node(table_name, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "temp like target");
    failures += expect_span_text(child_at(table_name, 0U), "app", "temp like target schema");
    failures += expect_span_text(child_at(table_name, 1U), "temp_clone", "temp like target table");
    failures += expect_node(source_table, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "temp like source");
    failures += expect_span_text(child_at(source_table, 0U), "other", "temp like source schema");
    failures += expect_span_text(child_at(source_table, 1U), "source", "temp like source table");
    failures += expect_node(
        if_not_exists,
        MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE,
        "temp like if not exists clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TEMPORARY TABLE temp_clone (LIKE source);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    table_name = child_at(statement, 0U);
    source_table = child_at(statement, 1U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_LIKE_STATEMENT,
        "parenthesized temporary create table like statement"
    );
    failures += expect_span_text(table_name, "temp_clone", "parenthesized temp like target");
    failures += expect_span_text(source_table, "source", "parenthesized temp like source");
    failures += expect_child_count(statement, 2U, "parenthesized temp like child count");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_create_table_select_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *table_name = NULL;
    const struct mylite_sql_ast_node *select_statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *from_clause = NULL;
    const struct mylite_sql_ast_node *if_not_exists = NULL;
    int failures = 0;

    failures += parse_sql(
        "CREATE TABLE IF NOT EXISTS app.copy AS "
        "SELECT id AS copied_id, n FROM other.source s "
        "WHERE id >= 1 ORDER BY n DESC LIMIT 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_name = child_at(statement, 0U);
    select_statement = child_at(statement, 1U);
    if_not_exists = child_at(statement, 2U);
    select_list = child_at(select_statement, 0U);
    from_clause = child_at(select_statement, 1U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_SELECT_STATEMENT,
        "create table select statement"
    );
    failures += expect_node(table_name, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "ctas target");
    failures += expect_span_text(child_at(table_name, 0U), "app", "ctas target schema");
    failures += expect_span_text(child_at(table_name, 1U), "copy", "ctas target table");
    failures += expect_node(
        if_not_exists,
        MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE,
        "ctas if not exists clause"
    );
    failures +=
        expect_node(select_statement, MYLITE_SQL_AST_SELECT_STATEMENT, "ctas source select");
    failures +=
        expect_span_text(child_at(child_at(select_list, 0U), 1U), "copied_id", "ctas source alias");
    failures += expect_node(from_clause, MYLITE_SQL_AST_FROM_TABLE, "ctas source from");
    failures += expect_span_text(child_at(from_clause, 0U), "other.source", "ctas source table");
    failures += expect_span_text(child_at(from_clause, 1U), "s", "ctas source alias");
    failures +=
        expect_node(child_at(select_statement, 2U), MYLITE_SQL_AST_WHERE_CLAUSE, "ctas where");
    failures +=
        expect_node(child_at(select_statement, 3U), MYLITE_SQL_AST_ORDER_BY_CLAUSE, "ctas order");
    failures +=
        expect_node(child_at(select_statement, 4U), MYLITE_SQL_AST_LIMIT_CLAUSE, "ctas limit");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE copy SELECT * FROM source;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_SELECT_STATEMENT,
        "create table select without as"
    );
    failures += expect_span_text(child_at(statement, 0U), "copy", "ctas no-as target");
    failures += expect_child_count(statement, 2U, "ctas no-as child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TEMPORARY TABLE IF NOT EXISTS copy AS SELECT * FROM source;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_name = child_at(statement, 0U);
    select_statement = child_at(statement, 1U);
    if_not_exists = child_at(statement, 2U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_SELECT_STATEMENT,
        "create temporary table select statement"
    );
    failures += expect_span_text(table_name, "copy", "temporary ctas target");
    failures += expect_node(
        select_statement,
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "temporary ctas source select"
    );
    failures += expect_node(
        if_not_exists,
        MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE,
        "temporary ctas if not exists clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TEMPORARY TABLE copy SELECT * FROM source;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_SELECT_STATEMENT,
        "create temporary table select without as"
    );
    failures += expect_child_count(statement, 2U, "temporary ctas no-as child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE copy (id INT) AS SELECT id FROM source;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_default_charset_collation_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *table_options = NULL;
    const struct mylite_sql_ast_node *charset_option = NULL;
    const struct mylite_sql_ast_node *collation_option = NULL;
    int failures = 0;

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle DEFAULT CHARSET=utf8mb4 "
        "COLLATE='utf8mb4_0900_ai_ci';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_options = child_at(statement, 1U);
    charset_option = child_at(table_options, 0U);
    collation_option = child_at(table_options, 1U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DEFAULT_CHARSET_COLLATION_STATEMENT,
        "alter table default charset collation statement"
    );
    failures += expect_child_count(statement, 2U, "alter charset collation child count");
    failures += expect_span_text(
        child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter charset collation target"
    );
    failures += expect_node(
        table_options,
        MYLITE_SQL_AST_TABLE_OPTION_LIST,
        "alter charset collation option list"
    );
    failures += expect_child_count(table_options, 2U, "alter charset collation option count");
    failures +=
        expect_node(charset_option, MYLITE_SQL_AST_TABLE_CHARSET_OPTION, "alter charset option");
    failures += expect_span_text(child_at(charset_option, 0U), "utf8mb4", "alter charset name");
    failures += expect_node(
        collation_option,
        MYLITE_SQL_AST_TABLE_COLLATION_OPTION,
        "alter collation option"
    );
    failures += expect_literal(
        child_at(collation_option, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "alter string collation"
    );
    failures += expect_span_text(
        child_at(collation_option, 0U),
        "'utf8mb4_0900_ai_ci'",
        "alter collation name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle DEFAULT CHARACTER SET utf8mb4;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle DEFAULT CHARACTER SET=utf8mb4;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE simple_lifecycle CHARSET `utf8mb4`;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle DEFAULT COLLATE utf8mb4_0900_ai_ci;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle DEFAULT CHARSET=binary;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle DEFAULT COLLATE binary;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE simple_lifecycle DEFAULT CHARSET=utf8mb4 CHARSET=utf8mb4;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    table_options = child_at(statement, 1U);
    failures += expect_child_count(table_options, 2U, "duplicate alter charset options");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name DEFAULT CHARSET=DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name CHARACTER SET DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE old_name COLLATE=DEFAULT;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name DEFAULT CHARACTER SET;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name DEFAULT CHARSET=utf8mb4, COLLATE=utf8mb4_0900_ai_ci;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE old_name ENGINE=InnoDB;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name CONVERT TO CHARACTER SET utf8mb4;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ALGORITHM=INSTANT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE old_name LOCK=DEFAULT;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_order_by_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *items = NULL;
    const struct mylite_sql_ast_node *first_item = NULL;
    const struct mylite_sql_ast_node *second_item = NULL;
    int failures = 0;

    failures += parse_sql(
        "ALTER TABLE app.old_name ORDER BY app.old_name.id DESC, `value` ASC;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    items = child_at(statement, 1U);
    first_item = child_at(items, 0U);
    second_item = child_at(items, 1U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ORDER_BY_STATEMENT,
        "alter table order by statement"
    );
    failures += expect_child_count(statement, 2U, "alter table order by child count");
    failures +=
        expect_span_text(child_at(statement, 0U), "app.old_name", "alter table order by target");
    failures += expect_node(items, MYLITE_SQL_AST_ORDER_BY_ITEM_LIST, "alter order list");
    failures += expect_child_count(items, 2U, "alter order list count");
    failures += expect_node(first_item, MYLITE_SQL_AST_ORDER_BY_ITEM, "alter first order item");
    failures +=
        expect_span_text(child_at(first_item, 0U), "app.old_name.id", "alter first order key");
    failures += expect_order_direction(
        child_at(first_item, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "alter first order direction"
    );
    failures += expect_node(second_item, MYLITE_SQL_AST_ORDER_BY_ITEM, "alter second order item");
    failures += expect_span_text(child_at(second_item, 0U), "`value`", "alter second order key");
    failures += expect_order_direction(
        child_at(second_item, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_ASC,
        "alter second order direction"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE old_name ORDER BY id;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    items = child_at(statement, 1U);
    first_item = child_at(items, 0U);
    failures += expect_node(first_item, MYLITE_SQL_AST_ORDER_BY_ITEM, "alter default order item");
    failures += expect_order_direction(
        child_at(first_item, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DEFAULT,
        "alter default order direction"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE old_name ORDER BY old_name.id ASC;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE old_name ORDER BY 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE old_name ORDER BY id + 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE old_name ORDER BY;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ORDER BY id, ADD COLUMN added INT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_algorithm_lock_option_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    int failures = 0;

    failures += parse_sql(
        "ALTER TABLE app.simple_lifecycle ADD COLUMN added INT, ALGORITHM=INSTANT, LOCK=DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_child_count(statement, 2U, "alter add column options child count");
    failures += expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT,
        "alter add column algorithm option"
    );
    failures += expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_DEFAULT,
        "alter add column lock option"
    );
    failures += expect_span_text(
        statement,
        "ALTER TABLE app.simple_lifecycle ADD COLUMN added INT, ALGORITHM=INSTANT, LOCK=DEFAULT",
        "alter add column options span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE algorithm_identifier (algorithm INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    column = child_at(columns, 0U);
    failures +=
        expect_span_text(child_at(statement, 0U), "algorithm_identifier", "algorithm table");
    failures += expect_span_text(child_at(column, 0U), "algorithm", "algorithm column");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_force_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("ALTER TABLE old_name FORCE;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_FORCE_STATEMENT,
        "alter table force statement"
    );
    failures += expect_child_count(statement, 1U, "alter table force child count");
    failures += expect_span_text(child_at(statement, 0U), "old_name", "alter table force target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE app.old_name FORCE;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(child_at(statement, 0U), "app.old_name", "qualified force target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name FORCE ORDER BY id;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE old_name FORCE, FORCE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE old_name FORCE, ALGORITHM=COPY;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY,
        "alter force algorithm option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE old_name FORCE LOCK=NONE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE old_name FORCE id;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_schema_lifecycle_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("CREATE DATABASE app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT, "create database");
    failures += expect_span_text(child_at(statement, 0U), "app", "create database name");
    failures += expect_child_count(statement, 1U, "create database child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE DATABASE configured DEFAULT CHARACTER SET utf8mb4 "
        "COLLATE='utf8mb4_unicode_ci';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT, "create database options");
    failures += expect_span_text(child_at(statement, 0U), "configured", "create options name");
    failures += expect_node(
        child_at(statement, 1U),
        MYLITE_SQL_AST_TABLE_OPTION_LIST,
        "create database option list"
    );
    failures += expect_node(
        child_at(child_at(statement, 1U), 0U),
        MYLITE_SQL_AST_TABLE_CHARSET_OPTION,
        "create database charset option"
    );
    failures += expect_node(
        child_at(child_at(statement, 1U), 1U),
        MYLITE_SQL_AST_TABLE_COLLATION_OPTION,
        "create database collation option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE SCHEMA `select`;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT, "create schema");
    failures += expect_span_text(child_at(statement, 0U), "`select`", "create schema name");
    failures += expect_child_count(statement, 1U, "create schema child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE DATABASE IF NOT EXISTS app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT, "create database if");
    failures += expect_span_text(child_at(statement, 0U), "app", "create database if name");
    failures += expect_node(
        child_at(statement, 1U),
        MYLITE_SQL_AST_CREATE_SCHEMA_IF_NOT_EXISTS_CLAUSE,
        "create database if marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER DATABASE configured DEFAULT COLLATE utf8mb4_0900_bin;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_SCHEMA_DEFAULT_CHARSET_COLLATION_STATEMENT,
        "alter database options"
    );
    failures += expect_child_count(statement, 2U, "alter database option child count");
    failures += expect_span_text(child_at(statement, 0U), "configured", "alter database name");
    failures += expect_node(
        child_at(statement, 1U),
        MYLITE_SQL_AST_TABLE_OPTION_LIST,
        "alter database option list"
    );
    failures += expect_node(
        child_at(child_at(statement, 1U), 0U),
        MYLITE_SQL_AST_TABLE_COLLATION_OPTION,
        "alter database collation option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER SCHEMA DEFAULT CHARSET=binary;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_SCHEMA_DEFAULT_CHARSET_COLLATION_STATEMENT,
        "alter selected schema options"
    );
    failures += expect_child_count(statement, 1U, "alter selected schema option child count");
    failures += expect_node(
        child_at(statement, 0U),
        MYLITE_SQL_AST_TABLE_OPTION_LIST,
        "alter selected schema option list"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP DATABASE app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT, "drop database");
    failures += expect_span_text(child_at(statement, 0U), "app", "drop database name");
    failures += expect_child_count(statement, 1U, "drop database child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP SCHEMA app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT, "drop schema");
    failures += expect_span_text(child_at(statement, 0U), "app", "drop schema name");
    failures += expect_child_count(statement, 1U, "drop schema child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP SCHEMA IF EXISTS app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT, "drop schema if");
    failures += expect_span_text(child_at(statement, 0U), "app", "drop schema if name");
    failures += expect_node(
        child_at(statement, 1U),
        MYLITE_SQL_AST_DROP_SCHEMA_IF_EXISTS_CLAUSE,
        "drop schema if marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW DATABASES;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT, "show databases");
    failures += expect_child_count(statement, 0U, "show databases child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW DATABASES LIKE 'app%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT, "show databases like");
    failures +=
        expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING, "databases like");
    failures += expect_span_text(child_at(statement, 0U), "'app%'", "databases like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW SCHEMAS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT, "show schemas");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE DATABASE app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_CREATE_DATABASE_STATEMENT,
        "show create database"
    );
    failures += expect_child_count(statement, 1U, "show create database child count");
    failures += expect_span_text(child_at(statement, 0U), "app", "show create database name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE SCHEMA `select`;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_CREATE_DATABASE_STATEMENT, "show create schema");
    failures += expect_span_text(child_at(statement, 0U), "`select`", "show create schema name");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_columns_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW COLUMNS FROM numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "show columns statement");
    failures += expect_child_count(statement, 1U, "show columns child count");
    failures += expect_span_text(child_at(statement, 0U), "numbers", "show columns table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLUMNS IN app.numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "show columns in statement");
    failures += expect_child_count(statement, 1U, "show columns in child count");
    failures +=
        expect_span_text(child_at(statement, 0U), "app.numbers", "show columns qualified table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FIELDS FROM numbers IN app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "show fields statement");
    failures += expect_child_count(statement, 2U, "show fields explicit schema child count");
    failures += expect_span_text(child_at(statement, 0U), "numbers", "show fields table");
    failures += expect_span_text(child_at(statement, 1U), "app", "show fields schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FIELDS IN numbers FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT,
        "show fields in from statement"
    );
    failures += expect_child_count(statement, 2U, "show fields in from child count");
    failures += expect_span_text(child_at(statement, 0U), "numbers", "show fields in table");
    failures += expect_span_text(child_at(statement, 1U), "app", "show fields from schema");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW COLUMNS FROM app.numbers FROM other;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT,
        "show columns qualified explicit schema"
    );
    failures += expect_child_count(statement, 2U, "show columns qualified explicit child count");
    failures +=
        expect_span_text(child_at(statement, 0U), "app.numbers", "show columns qualified table");
    failures += expect_span_text(child_at(statement, 1U), "other", "show columns trailing schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLUMNS FROM numbers LIKE 'i%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "show columns like");
    failures += expect_child_count(statement, 2U, "show columns like child count");
    failures += expect_span_text(child_at(statement, 0U), "numbers", "show columns like table");
    failures +=
        expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING, "columns like");
    failures += expect_span_text(child_at(statement, 1U), "'i%'", "show columns like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW COLUMNS FROM numbers WHERE Field = 'id';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "show columns where");
    failures += expect_child_count(statement, 2U, "show columns where child count");
    failures += expect_span_text(child_at(statement, 0U), "numbers", "show columns where table");
    failures +=
        expect_node(child_at(statement, 1U), MYLITE_SQL_AST_WHERE_CLAUSE, "columns where clause");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW FULL COLUMNS FROM numbers LIKE 'i%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_FULL_COLUMNS_STATEMENT, "show full columns");
    failures += expect_child_count(statement, 2U, "show full columns like child count");
    failures += expect_span_text(child_at(statement, 0U), "numbers", "show full columns table");
    failures +=
        expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING, "full columns like");
    failures += expect_span_text(child_at(statement, 1U), "'i%'", "full columns like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW FULL FIELDS IN app.numbers FROM other;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_FULL_COLUMNS_STATEMENT, "show full fields");
    failures += expect_child_count(statement, 2U, "show full fields child count");
    failures += expect_span_text(child_at(statement, 0U), "app.numbers", "full fields table");
    failures += expect_span_text(child_at(statement, 1U), "other", "full fields schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW FULL FIELDS IN app.numbers FROM other WHERE Collation IS NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_FULL_COLUMNS_STATEMENT,
        "show full fields where"
    );
    failures += expect_child_count(statement, 3U, "show full fields where child count");
    failures += expect_span_text(child_at(statement, 0U), "app.numbers", "full fields where table");
    failures += expect_span_text(child_at(statement, 1U), "other", "full fields where schema");
    failures +=
        expect_node(child_at(statement, 2U), MYLITE_SQL_AST_WHERE_CLAUSE, "full fields where");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW FIELDS FROM app.numbers FROM other LIKE 'i\\_1';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "show fields qualified like");
    failures += expect_child_count(statement, 3U, "show fields qualified like child count");
    failures += expect_span_text(child_at(statement, 0U), "app.numbers", "fields like table");
    failures += expect_span_text(child_at(statement, 1U), "other", "fields like schema");
    failures +=
        expect_literal(child_at(statement, 2U), MYLITE_SQL_AST_LITERAL_STRING, "fields like");
    failures += expect_span_text(child_at(statement, 2U), "'i\\_1'", "fields like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DESCRIBE app.numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "describe table");
    failures += expect_span_text(child_at(statement, 0U), "app.numbers", "describe target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DESC numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "desc table");
    failures += expect_span_text(child_at(statement, 0U), "numbers", "desc target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN app.numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "explain table");
    failures += expect_span_text(child_at(statement, 0U), "app.numbers", "explain target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE TABLE app.numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT, "show create table");
    failures += expect_child_count(statement, 1U, "show create child count");
    failures += expect_span_text(child_at(statement, 0U), "app.numbers", "show create target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE columns (fields INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "columns table name");
    failures += expect_span_text(child_at(statement, 0U), "columns", "columns identifier");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_triggers_empty_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW TRIGGERS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT, "show triggers");
    failures += expect_child_count(statement, 0U, "show triggers child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL TRIGGERS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT, "show full triggers");
    failures += expect_child_count(statement, 0U, "show full triggers child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TRIGGERS FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT, "show triggers from");
    failures += expect_child_count(statement, 1U, "show triggers from child count");
    failures += expect_span_text(child_at(statement, 0U), "app", "show triggers schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TRIGGERS IN app LIKE 'account\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT, "show triggers in like");
    failures += expect_child_count(statement, 2U, "show triggers in like child count");
    failures += expect_span_text(child_at(statement, 0U), "app", "show triggers like schema");
    failures +=
        expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING, "triggers like");
    failures += expect_span_text(child_at(statement, 1U), "'account\\_%'", "triggers like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL TRIGGERS LIKE 'account';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT, "show full triggers like");
    failures += expect_child_count(statement, 1U, "show full triggers like child count");
    failures += expect_literal(
        child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "full triggers like"
    );
    failures +=
        expect_span_text(child_at(statement, 0U), "'account'", "full triggers like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE triggers (full INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "triggers table");
    failures += expect_span_text(child_at(statement, 0U), "triggers", "triggers identifier");
    failures += expect_span_text(
        child_at(child_at(child_at(statement, 1U), 0U), 0U),
        "full",
        "full identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TRIGGER FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW EXTENDED TRIGGERS FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW TRIGGERS FROM app WHERE `Table` = 'account';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW TRIGGERS FROM app LIKE 'account' WHERE `Table` = 'account';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_events_empty_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW EVENTS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT, "show events");
    failures += expect_child_count(statement, 0U, "show events child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EVENTS FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT, "show events from");
    failures += expect_child_count(statement, 1U, "show events from child count");
    failures += expect_span_text(child_at(statement, 0U), "app", "show events schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EVENTS IN app LIKE 'daily\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT, "show events in like");
    failures += expect_child_count(statement, 2U, "show events in like child count");
    failures += expect_span_text(child_at(statement, 0U), "app", "show events like schema");
    failures +=
        expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING, "events like");
    failures += expect_span_text(child_at(statement, 1U), "'daily\\_%'", "events like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EVENTS LIKE 'daily%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT, "show events like");
    failures += expect_child_count(statement, 1U, "show events like child count");
    failures +=
        expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING, "events bare like");
    failures += expect_span_text(child_at(statement, 0U), "'daily%'", "events bare like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE events (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "events table");
    failures += expect_span_text(child_at(statement, 0U), "events", "events identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL EVENTS FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EXTENDED EVENTS FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EVENT FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW EVENTS FROM app WHERE Name = 'daily_event';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW EVENTS FROM app LIKE 'daily%' WHERE Name = 'daily_event';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_open_tables_empty_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW OPEN TABLES;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT, "show open tables");
    failures += expect_child_count(statement, 0U, "show open tables child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW OPEN TABLES FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT, "show open tables from");
    failures += expect_child_count(statement, 1U, "show open tables from child count");
    failures += expect_span_text(child_at(statement, 0U), "app", "show open tables schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW OPEN TABLES IN app LIKE 'open\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT,
        "show open tables in like"
    );
    failures += expect_child_count(statement, 2U, "show open tables in like child count");
    failures += expect_span_text(child_at(statement, 0U), "app", "show open tables like schema");
    failures +=
        expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING, "open tables like");
    failures += expect_span_text(child_at(statement, 1U), "'open\\_%'", "open tables like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW OPEN TABLES LIKE 'open%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT, "show open tables like");
    failures += expect_child_count(statement, 1U, "show open tables like child count");
    failures += expect_literal(
        child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "open tables bare like"
    );
    failures +=
        expect_span_text(child_at(statement, 0U), "'open%'", "open tables bare like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE open (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "open table");
    failures += expect_span_text(child_at(statement, 0U), "open", "open identifier");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW FULL OPEN TABLES FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW EXTENDED OPEN TABLES FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW OPEN TABLES FROM app WHERE `Table` = 'open_table';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW OPEN TABLES FROM app LIKE 'open%' WHERE `Table` = 'open_table';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW OPEN TABLES FROM app ORDER BY `Table`;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW OPEN TABLES FROM app LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW OPEN TABLES FROM app.extra;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW OPEN TABLES FROM app LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW OPEN TABLES FROM app LIKE NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_routine_status_empty_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW PROCEDURE STATUS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_PROCEDURE_STATUS_STATEMENT,
        "show procedure status"
    );
    failures += expect_child_count(statement, 0U, "show procedure status child count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW PROCEDURE STATUS LIKE 'routine\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_PROCEDURE_STATUS_STATEMENT,
        "show procedure status like"
    );
    failures += expect_child_count(statement, 1U, "show procedure status like child count");
    failures += expect_literal(
        child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "procedure status like"
    );
    failures +=
        expect_span_text(child_at(statement, 0U), "'routine\\_%'", "procedure like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FUNCTION STATUS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_FUNCTION_STATUS_STATEMENT,
        "show function status"
    );
    failures += expect_child_count(statement, 0U, "show function status child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FUNCTION STATUS LIKE 'routine%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_FUNCTION_STATUS_STATEMENT,
        "show function status like"
    );
    failures += expect_child_count(statement, 1U, "show function status like child count");
    failures += expect_literal(
        child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "function status like"
    );
    failures += expect_span_text(child_at(statement, 0U), "'routine%'", "function like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE status (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "status table");
    failures += expect_span_text(child_at(statement, 0U), "status", "status identifier");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW PROCEDURE STATUS FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FUNCTION STATUS IN app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL PROCEDURE STATUS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EXTENDED FUNCTION STATUS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW PROCEDURE STATUS WHERE Name = 'routine_proc';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW PROCEDURE STATUS LIKE 'routine%' WHERE Name = 'routine_proc';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW PROCEDURE STATUS ORDER BY Name;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FUNCTION STATUS LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW PROCEDURE STATUS LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW FUNCTION STATUS LIKE NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW PROCEDURE STATUS LIKE N'routine';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW FUNCTION STATUS LIKE _utf8mb4'routine';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_processlist_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW PROCESSLIST;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_PROCESSLIST_STATEMENT, "show processlist");
    failures += expect_child_count(statement, 0U, "show processlist child count");
    failures += expect_span_text(statement, "SHOW PROCESSLIST", "show processlist span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL PROCESSLIST;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_FULL_PROCESSLIST_STATEMENT,
        "show full processlist"
    );
    failures += expect_child_count(statement, 0U, "show full processlist child count");
    failures += expect_span_text(statement, "SHOW FULL PROCESSLIST", "show full processlist span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL /* keep */ PROCESSLIST;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_FULL_PROCESSLIST_STATEMENT,
        "show full processlist with comment"
    );
    failures += expect_span_text(
        statement,
        "SHOW FULL /* keep */ PROCESSLIST",
        "show full processlist commented span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE processlist (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "processlist table");
    failures += expect_span_text(child_at(statement, 0U), "processlist", "processlist identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW PROCESSLIST LIKE 'root%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW PROCESSLIST WHERE Id > 0;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW PROCESSLIST ORDER BY Id;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW PROCESSLIST LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL PROCESSLIST LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW PROCESSLIST FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW PROCESSLIST IN app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EXTENDED PROCESSLIST;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_grants_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW GRANTS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_GRANTS_STATEMENT, "show grants");
    failures += expect_child_count(statement, 0U, "show grants child count");
    failures += expect_span_text(statement, "SHOW GRANTS", "show grants span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("show grants;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_GRANTS_STATEMENT, "lowercase show grants");
    failures += expect_child_count(statement, 0U, "lowercase show grants child count");
    failures += expect_span_text(statement, "show grants", "lowercase show grants span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW GRANTS FOR CURRENT_USER;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_GRANTS_STATEMENT, "show grants current user");
    failures += expect_child_count(statement, 0U, "show grants current user child count");
    failures += expect_span_text(
        statement,
        "SHOW GRANTS FOR CURRENT_USER",
        "show grants current user span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW GRANTS FOR CURRENT_USER();", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_GRANTS_STATEMENT,
        "show grants current user function"
    );
    failures += expect_child_count(statement, 0U, "show grants current user function child count");
    failures += expect_span_text(
        statement,
        "SHOW GRANTS FOR CURRENT_USER()",
        "show grants current user function span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE grants (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "grants table");
    failures += expect_span_text(child_at(statement, 0U), "grants", "grants identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW GRANTS FOR root;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW GRANTS FOR 'root'@'%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW GRANTS FOR CURRENT_USER USING 'r';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW GRANTS LIKE 'root%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW GRANTS LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW GRANTS WHERE User = 'root';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_warnings_diagnostics_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *limit = NULL;
    int failures = 0;

    failures += parse_sql("SHOW WARNINGS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT, "show warnings");
    failures += expect_child_count(statement, 0U, "show warnings child count");
    failures += expect_span_text(statement, "SHOW WARNINGS", "show warnings span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW WARNINGS LIMIT 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    limit = child_at(statement, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT, "show warnings limit");
    failures += expect_child_count(statement, 1U, "show warnings limit child count");
    failures += expect_node(limit, MYLITE_SQL_AST_LIMIT_CLAUSE, "show warnings limit clause");
    failures += expect_span_text(child_at(limit, 0U), "1", "show warnings limit row count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW WARNINGS LIMIT 2, 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    limit = child_at(statement, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT, "show warnings comma");
    failures += expect_child_count(limit, 2U, "show warnings comma child count");
    failures += expect_span_text(child_at(limit, 0U), "1", "show warnings comma row count");
    failures += expect_span_text(child_at(limit, 1U), "2", "show warnings comma offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW WARNINGS LIMIT 1 OFFSET 2;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    limit = child_at(statement, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT, "show warnings offset");
    failures += expect_child_count(limit, 2U, "show warnings offset child count");
    failures += expect_span_text(child_at(limit, 0U), "1", "show warnings offset row count");
    failures += expect_span_text(child_at(limit, 1U), "2", "show warnings offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COUNT(*) WARNINGS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_COUNT_WARNINGS_STATEMENT, "show count warnings");
    failures += expect_child_count(statement, 0U, "show count warnings child count");
    failures += expect_span_text(statement, "SHOW COUNT(*) WARNINGS", "show count warnings span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE warnings (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "warnings table");
    failures += expect_span_text(child_at(statement, 0U), "warnings", "warnings identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COUNT (*) WARNINGS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW WARNINGS LIMIT +1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW WARNINGS LIMIT -1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW WARNINGS LIKE 'x';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW WARNINGS WHERE Code = 1287;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW WARNINGS ORDER BY Code;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_errors_diagnostics_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *limit = NULL;
    int failures = 0;

    failures += parse_sql("SHOW ERRORS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT, "show errors");
    failures += expect_child_count(statement, 0U, "show errors child count");
    failures += expect_span_text(statement, "SHOW ERRORS", "show errors span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ERRORS LIMIT 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    limit = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT, "show errors limit");
    failures += expect_child_count(statement, 1U, "show errors limit child count");
    failures += expect_node(limit, MYLITE_SQL_AST_LIMIT_CLAUSE, "show errors limit clause");
    failures += expect_span_text(child_at(limit, 0U), "1", "show errors limit row count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ERRORS LIMIT 2, 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    limit = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT, "show errors comma");
    failures += expect_child_count(limit, 2U, "show errors comma child count");
    failures += expect_span_text(child_at(limit, 0U), "1", "show errors comma row count");
    failures += expect_span_text(child_at(limit, 1U), "2", "show errors comma offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ERRORS LIMIT 1 OFFSET 2;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    limit = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT, "show errors offset");
    failures += expect_child_count(limit, 2U, "show errors offset child count");
    failures += expect_span_text(child_at(limit, 0U), "1", "show errors offset row count");
    failures += expect_span_text(child_at(limit, 1U), "2", "show errors offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COUNT(*) ERRORS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_COUNT_ERRORS_STATEMENT, "show count errors");
    failures += expect_child_count(statement, 0U, "show count errors child count");
    failures += expect_span_text(statement, "SHOW COUNT(*) ERRORS", "show count errors span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE errors (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "errors table");
    failures += expect_span_text(child_at(statement, 0U), "errors", "errors identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COUNT (*) ERRORS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ERRORS LIMIT +1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ERRORS LIMIT -1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ERRORS LIKE 'x';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ERRORS WHERE Code = 1064;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ERRORS ORDER BY Code;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_index_empty_introspection_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW INDEX FROM numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, "show index");
    failures += expect_child_count(statement, 1U, "show index child count");
    failures += expect_span_text(child_at(statement, 0U), "numbers", "show index table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW INDEX IN app.numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, "show index in");
    failures += expect_child_count(statement, 1U, "show index in child count");
    failures += expect_span_text(child_at(statement, 0U), "app.numbers", "show index in table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW INDEXES FROM numbers FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, "show indexes");
    failures += expect_child_count(statement, 2U, "show indexes child count");
    failures += expect_span_text(child_at(statement, 0U), "numbers", "show indexes table");
    failures += expect_span_text(child_at(statement, 1U), "app", "show indexes schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW KEYS IN app.numbers IN other;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, "show keys");
    failures += expect_child_count(statement, 2U, "show keys child count");
    failures += expect_span_text(child_at(statement, 0U), "app.numbers", "show keys table");
    failures += expect_span_text(child_at(statement, 1U), "other", "show keys schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE indexes (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "indexes table");
    failures += expect_span_text(child_at(statement, 0U), "indexes", "indexes identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW INDEX;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW EXTENDED INDEX FROM numbers;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW INDEX FROM numbers WHERE Key_name = 'idx';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, "show index where");
    failures += expect_child_count(statement, 2U, "show index where child count");
    failures += expect_span_text(child_at(statement, 0U), "numbers", "show index where table");
    failures +=
        expect_node(child_at(statement, 1U), MYLITE_SQL_AST_WHERE_CLAUSE, "show index where");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW KEYS IN app.numbers IN other WHERE `Column_name` IN ('id','v');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, "show keys where");
    failures += expect_child_count(statement, 3U, "show keys where child count");
    failures += expect_span_text(child_at(statement, 0U), "app.numbers", "show keys where table");
    failures += expect_span_text(child_at(statement, 1U), "other", "show keys where schema");
    failures +=
        expect_node(child_at(statement, 2U), MYLITE_SQL_AST_WHERE_CLAUSE, "show keys where");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW INDEX FROM numbers WHERE Key_name = 'idx' ORDER BY Key_name;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW INDEX FROM numbers LIKE 'idx';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_variables_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW VARIABLES;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT, "show variables");
    failures += expect_child_count(statement, 0U, "show variables child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW GLOBAL VARIABLES;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT, "show global variables");
    failures += expect_span_text(child_at(statement, 0U), "GLOBAL", "global variables scope");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW SESSION VARIABLES LIKE 'sql\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT, "show session variables");
    failures += expect_span_text(child_at(statement, 0U), "SESSION", "session variables scope");
    failures +=
        expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING, "variables like");
    failures += expect_span_text(child_at(statement, 1U), "'sql\\_%'", "variables like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW LOCAL VARIABLES LIKE 'autocommit';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT, "show local variables");
    failures += expect_span_text(child_at(statement, 0U), "LOCAL", "local variables scope");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW VARIABLES LIKE 'SQL\\_LOG\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT, "show variables like");
    failures += expect_literal(
        child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "bare variables like"
    );
    failures += expect_span_text(child_at(statement, 0U), "'SQL\\_LOG\\_%'", "bare like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW VARIABLES WHERE Variable_name = 'autocommit';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT, "show variables where");
    failures += expect_child_count(statement, 1U, "show variables where child count");
    failures +=
        expect_node(child_at(statement, 0U), MYLITE_SQL_AST_WHERE_CLAUSE, "variables where");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW GLOBAL VARIABLES WHERE `Value` = 'ON';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(child_at(statement, 0U), "GLOBAL", "global variables where scope");
    failures +=
        expect_node(child_at(statement, 1U), MYLITE_SQL_AST_WHERE_CLAUSE, "scoped variables where");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW VARIABLES LIKE 'a%' WHERE Variable_name = 'autocommit';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW VARIABLES WHERE Variable_name = 'autocommit' ORDER BY Variable_name;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE variables (global INT, session INT, local INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "variables nonreserved identifiers"
    );
    failures += expect_span_text(child_at(statement, 0U), "variables", "variables table");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_status_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW STATUS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_STATUS_STATEMENT, "show status");
    failures += expect_child_count(statement, 0U, "show status child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW GLOBAL STATUS;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_STATUS_STATEMENT, "show global status");
    failures += expect_span_text(child_at(statement, 0U), "GLOBAL", "global status scope");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW SESSION STATUS LIKE 'threads\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_STATUS_STATEMENT, "show session status");
    failures += expect_span_text(child_at(statement, 0U), "SESSION", "session status scope");
    failures +=
        expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING, "status like");
    failures += expect_span_text(child_at(statement, 1U), "'threads\\_%'", "status like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW LOCAL STATUS LIKE 'Threads%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_STATUS_STATEMENT, "show local status");
    failures += expect_span_text(child_at(statement, 0U), "LOCAL", "local status scope");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW STATUS LIKE 'Threads\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_STATUS_STATEMENT, "show status like");
    failures +=
        expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING, "bare status like");
    failures +=
        expect_span_text(child_at(statement, 0U), "'Threads\\_%'", "bare status like pattern");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW STATUS WHERE Variable_name = 'Threads_connected';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW STATUS LIKE 'Threads%' WHERE Variable_name = 'Threads_connected';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW STATUS ORDER BY Variable_name;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW STATUS LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL STATUS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW STATUS LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW STATUS LIKE NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW STATUS LIKE N'threads';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_where_predicates(void) {
    static const char *const in_predicate_values[] = {"-2", "+1", "NULL", "TRUE", "FALSE"};

    static const struct {
        const char *sql;
        enum mylite_sql_ast_operator expected_operator;
        enum mylite_sql_ast_node_kind expected_kind;
    } cases[] = {
        {
            "SELECT id FROM simple_lifecycle WHERE id = 1;",
            MYLITE_SQL_AST_OPERATOR_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id <=> 1;",
            MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id <> 1;",
            MYLITE_SQL_AST_OPERATOR_NOT_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id != 1;",
            MYLITE_SQL_AST_OPERATOR_NOT_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id < -1;",
            MYLITE_SQL_AST_OPERATOR_LESS,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id <= +1;",
            MYLITE_SQL_AST_OPERATOR_LESS_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id > 1;",
            MYLITE_SQL_AST_OPERATOR_GREATER,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id >= 1;",
            MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id LIKE '1%';",
            MYLITE_SQL_AST_OPERATOR_LIKE,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id REGEXP '^1$';",
            MYLITE_SQL_AST_OPERATOR_REGEXP,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id RLIKE '^1$';",
            MYLITE_SQL_AST_OPERATOR_RLIKE,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id = TRUE;",
            MYLITE_SQL_AST_OPERATOR_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id <=> false;",
            MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id <=> NULL;",
            MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id = NULL;",
            MYLITE_SQL_AST_OPERATOR_EQUAL,
            MYLITE_SQL_AST_COMPARISON_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id IS NULL;",
            MYLITE_SQL_AST_OPERATOR_IS_NULL,
            MYLITE_SQL_AST_IS_NULL_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id IS NOT NULL;",
            MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL,
            MYLITE_SQL_AST_IS_NULL_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id IS TRUE;",
            MYLITE_SQL_AST_OPERATOR_IS_TRUE,
            MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id IS NOT TRUE;",
            MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE,
            MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id IS FALSE;",
            MYLITE_SQL_AST_OPERATOR_IS_FALSE,
            MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id IS NOT FALSE;",
            MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE,
            MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id IS UNKNOWN;",
            MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN,
            MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE,
        },
        {
            "SELECT id FROM simple_lifecycle WHERE id IS NOT UNKNOWN;",
            MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN,
            MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE,
        },
    };
    struct mylite_sql_parse_result result;
    int failures = 0;

    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        const struct mylite_sql_ast_node *statement = NULL;
        const struct mylite_sql_ast_node *where_clause = NULL;
        const struct mylite_sql_ast_node *predicate = NULL;

        failures += parse_sql(cases[index].sql, MYLITE_SQL_PARSE_OK, &result);
        statement = child_at(result.root, 0U);
        where_clause = child_at(statement, 2U);
        predicate = child_at(where_clause, 0U);
        failures += expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "where clause");
        failures += expect_node(predicate, cases[index].expected_kind, "where predicate");
        failures += expect_operator(predicate, cases[index].expected_operator, "where operator");
        failures += expect_span_text(child_at(predicate, 0U), "id", "where predicate column");
        mylite_sql_parse_result_deinit(&result);
    }

    failures +=
        parse_sql("SELECT id FROM simple_lifecycle WHERE TRUE;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_literal(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "scalar literal truth predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM simple_lifecycle WHERE -1;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_UNARY_EXPRESSION,
        "signed scalar literal truth predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM simple_lifecycle WHERE 1 = id;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "literal-left column predicate"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        "1",
        "literal-left column predicate literal"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 1U),
        "id",
        "literal-left column predicate column"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE NULL <=> id;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_operator(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL,
        "literal-left null-safe predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM simple_lifecycle WHERE 1 = 1;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "scalar literal comparison predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE NULL IS UNKNOWN;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_operator(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN,
        "scalar literal unknown predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'app';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "information schema string predicate"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 1U),
        "'app'",
        "information schema string predicate value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = DATABASE();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "information schema database predicate"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 1U),
        MYLITE_SQL_AST_DATABASE_FUNCTION,
        "information schema database predicate value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id BETWEEN -2 AND 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_BETWEEN_PREDICATE,
        "between predicate"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        "id",
        "between predicate column"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 1U),
        "-2",
        "between lower bound"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 2U),
        "1",
        "between upper bound"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id NOT BETWEEN -2 AND 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not between predicate"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_BETWEEN_PREDICATE,
        "not between child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE NOT id BETWEEN -2 AND 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "prefix not between predicate"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_BETWEEN_PREDICATE,
        "prefix not between child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id BETWEEN -2 AND 1 AND nn = 5 OR id = 3;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OR_PREDICATE,
        "between and or precedence root"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_AND_PREDICATE,
        "between binds before later and"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U), 0U),
        MYLITE_SQL_AST_BETWEEN_PREDICATE,
        "between precedence child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id IN (-2, +1, NULL, TRUE, FALSE);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_IN_PREDICATE,
        "in predicate"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        "id",
        "in predicate column"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 1U),
        MYLITE_SQL_AST_PREDICATE_VALUE_LIST,
        "in predicate list"
    );
    failures += expect_child_count(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 1U),
        sizeof(in_predicate_values) / sizeof(in_predicate_values[0]),
        "in predicate list count"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 1U), 0U),
        "-2",
        "in predicate first value"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 1U), 2U),
        "NULL",
        "in predicate null value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id NOT IN (-2, 1, NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not in predicate"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_IN_PREDICATE,
        "not in child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id NOT LIKE '1%';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not like predicate"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "not like child"
    );
    failures += expect_operator(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_OPERATOR_LIKE,
        "not like child operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id NOT REGEXP '^1$';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not regexp predicate"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "not regexp child"
    );
    failures += expect_operator(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_OPERATOR_REGEXP,
        "not regexp child operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id NOT RLIKE '^1$';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not rlike predicate"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "not rlike child"
    );
    failures += expect_operator(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_OPERATOR_RLIKE,
        "not rlike child operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE NOT id IN (-2, 1, NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "prefix not in predicate"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_IN_PREDICATE,
        "prefix not in child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id IN (-2, 1) AND nn = 5 OR id = 3;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OR_PREDICATE,
        "in and or precedence root"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_AND_PREDICATE,
        "in binds before later and"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U), 0U),
        MYLITE_SQL_AST_IN_PREDICATE,
        "in precedence child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE NOT id IS UNKNOWN;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "prefix not is unknown predicate"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE,
        "prefix not is unknown child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id IS TRUE AND nn = 5 OR id = 3;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OR_PREDICATE,
        "is boolean and or precedence root"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_AND_PREDICATE,
        "is boolean binds before later and"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U), 0U),
        MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE,
        "is boolean precedence child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id IN ();",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id IN (nn);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id IN ('1');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_literal(
        child_at(child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 1U), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "string IN value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id LIKE DATABASE();",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id LIKE '1%' ESCAPE '#';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id REGEXP 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id REGEXP DATABASE();",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id IS 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id IS TRUE IS TRUE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM simple_lifecycle WHERE 1 IS TRUE;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id + 1 IS TRUE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id BETWEEN NULL AND 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id BETWEEN nn AND 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM simple_lifecycle WHERE (id = +1);", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT * FROM simple_lifecycle WHERE id = 1 AND nn IS NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_AND_PREDICATE,
        "and predicate"
    );
    failures += expect_operator(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_AND,
        "and predicate operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT * FROM simple_lifecycle WHERE (id = 1) && (nn IS NOT NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_AND_PREDICATE,
        "deprecated and predicate"
    );
    failures += expect_operator(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND,
        "deprecated and predicate operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT * FROM simple_lifecycle WHERE id = 1 AND nn = 2 AND n IS NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_AND_PREDICATE,
        "chained and predicate"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_AND_PREDICATE,
        "left-associative chained and predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT * FROM simple_lifecycle WHERE id = 1 OR nn = 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OR_PREDICATE,
        "or predicate"
    );
    failures += expect_operator(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_OR,
        "or predicate operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT * FROM simple_lifecycle WHERE id = 1 || nn = 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OR_PREDICATE,
        "deprecated or predicate"
    );
    failures += expect_operator(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR,
        "deprecated or predicate operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT * FROM simple_lifecycle WHERE id = 1 OR nn = 2 AND n IS NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OR_PREDICATE,
        "or and precedence root"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 1U),
        MYLITE_SQL_AST_AND_PREDICATE,
        "and binds tighter than or"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT * FROM simple_lifecycle WHERE id = 1 XOR nn = 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_XOR_PREDICATE,
        "xor predicate"
    );
    failures += expect_operator(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR,
        "xor predicate operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT * FROM simple_lifecycle WHERE id = 1 XOR nn = 2 XOR n IS NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_XOR_PREDICATE,
        "chained xor predicate"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_XOR_PREDICATE,
        "left-associative chained xor predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT * FROM simple_lifecycle WHERE id = 1 XOR nn = 2 AND n IS NULL OR id = 3;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OR_PREDICATE,
        "or above xor precedence root"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_XOR_PREDICATE,
        "xor binds tighter than or"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U), 1U),
        MYLITE_SQL_AST_AND_PREDICATE,
        "and binds tighter than xor"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT * FROM simple_lifecycle WHERE (id = 1 OR nn = 2) XOR n IS NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_XOR_PREDICATE,
        "parentheses override xor precedence"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized or xor left operand"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT * FROM simple_lifecycle WHERE NOT id = 1 XOR nn = 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_XOR_PREDICATE,
        "not xor precedence root"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not binds tighter than xor"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT * FROM simple_lifecycle WHERE (id = 1 OR nn = 2) AND n IS NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_AND_PREDICATE,
        "parentheses override or precedence"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized or left operand"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM simple_lifecycle WHERE NOT id = 1;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not predicate"
    );
    failures += expect_operator(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT,
        "not predicate operator"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT * FROM simple_lifecycle WHERE NOT NOT id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "outer repeated not predicate"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "inner repeated not predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT * FROM simple_lifecycle WHERE NOT id = 1 AND nn = 2 OR n IS NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_OR_PREDICATE,
        "not and or precedence root"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_AND_PREDICATE,
        "not binds tighter than and"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not predicate under and"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT * FROM simple_lifecycle WHERE NOT (id = 1 OR nn = 2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not parenthesized predicate"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "not parenthesized child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE simple_lifecycle.id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified predicate column"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM simple_lifecycle WHERE id = TRUE;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_literal(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 1U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "true predicate right operand"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM users WHERE EXISTS (SELECT 1 FROM orders);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_EXISTS_PREDICATE,
        "exists predicate"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        "EXISTS (SELECT 1 FROM orders)",
        "exists predicate span"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "exists inner select"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM users WHERE NOT EXISTS (SELECT * FROM empty_orders);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "not exists predicate"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_EXISTS_PREDICATE,
        "not exists child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT u.id FROM users AS u WHERE EXISTS "
        "(SELECT 1 FROM orders AS o WHERE o.user_id = u.id);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(
            child_at(child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U), 2U),
            0U
        ),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "exists correlated comparison"
    );
    failures += expect_node(
        child_at(
            child_at(child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U), 2U),
            0U
        ),
        MYLITE_SQL_AST_COMPARISON_PREDICATE,
        "exists correlated comparison node"
    );
    failures += expect_node(
        child_at(
            child_at(
                child_at(child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U), 2U),
                0U
            ),
            1U
        ),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "exists correlated right column"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT u.id FROM users AS u WHERE u.id IN "
        "(SELECT o.user_id FROM orders AS o WHERE o.user_id = u.id);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_IN_PREDICATE,
        "IN subquery predicate"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 1U),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "IN subquery inner select"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT u.id FROM users AS u WHERE u.id NOT IN "
        "(SELECT o.user_id FROM orders AS o);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_node(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_NOT_PREDICATE,
        "NOT IN subquery predicate"
    );
    failures += expect_node(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_IN_PREDICATE,
        "NOT IN subquery child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT EXISTS (SELECT 1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_order_limit_clauses(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures +=
        parse_sql("SELECT * FROM simple_lifecycle ORDER BY id;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    order_clause = first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    failures += expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "order clause");
    failures += expect_span_text(child_at(order_clause, 0U), "id", "default order key");
    failures += expect_true(child_at(order_clause, 1U) == NULL, "default order has no direction");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id = 1 ORDER BY nn ASC LIMIT 2 OFFSET 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    order_clause = first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "asc order clause");
    failures += expect_span_text(child_at(order_clause, 0U), "nn", "asc order key");
    failures += expect_order_direction(
        child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_ASC,
        "asc order direction"
    );
    failures += expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "offset limit clause");
    failures += expect_span_text(child_at(limit_clause, 0U), "2", "offset limit row count");
    failures += expect_span_text(child_at(limit_clause, 1U), "1", "offset limit offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle ORDER BY simple_lifecycle.id DESC LIMIT 1, 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    order_clause = first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += expect_node(
        child_at(order_clause, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified order key"
    );
    failures += expect_order_direction(
        child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "desc order direction"
    );
    failures += expect_span_text(child_at(limit_clause, 0U), "2", "comma limit row count");
    failures += expect_span_text(child_at(limit_clause, 1U), "1", "comma limit offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM simple_lifecycle LIMIT 0;", MYLITE_SQL_PARSE_OK, &result);
    limit_clause = first_child_kind(child_at(result.root, 0U), MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "simple limit clause");
    failures += expect_span_text(child_at(limit_clause, 0U), "0", "simple limit row count");
    failures += expect_true(child_at(limit_clause, 1U) == NULL, "simple limit has no offset");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_group_by_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *group_clause = NULL;
    const struct mylite_sql_ast_node *having_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT g, COUNT(*) AS c FROM numbers WHERE id >= 1 GROUP BY g HAVING c > 1 "
        "ORDER BY g LIMIT 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    group_clause = first_child_kind(statement, MYLITE_SQL_AST_GROUP_BY_CLAUSE);
    having_clause = first_child_kind(statement, MYLITE_SQL_AST_HAVING_CLAUSE);
    order_clause = first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += expect_node(group_clause, MYLITE_SQL_AST_GROUP_BY_CLAUSE, "group clause");
    failures += expect_span_text(child_at(group_clause, 0U), "g", "group key");
    failures += expect_node(child_at(statement, 2U), MYLITE_SQL_AST_WHERE_CLAUSE, "group where");
    failures += expect_node(having_clause, MYLITE_SQL_AST_HAVING_CLAUSE, "group having");
    failures += expect_span_text(child_at(child_at(having_clause, 0U), 0U), "c", "having alias");
    failures += expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "group order");
    failures += expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "group limit");
    failures += expect_node(
        child_at(child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "group aggregate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT t.g AS k, SUM(t.n) AS s FROM app.numbers AS t GROUP BY t.g "
        "HAVING SUM(t.n) IS NOT NULL ORDER BY k DESC LIMIT 1 OFFSET 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    group_clause = first_child_kind(statement, MYLITE_SQL_AST_GROUP_BY_CLAUSE);
    having_clause = first_child_kind(statement, MYLITE_SQL_AST_HAVING_CLAUSE);
    order_clause = first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += expect_node(
        child_at(group_clause, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "qualified group key"
    );
    failures += expect_span_text(child_at(group_clause, 0U), "t.g", "qualified group span");
    failures += expect_node(
        child_at(child_at(having_clause, 0U), 0U),
        MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION,
        "having aggregate operand"
    );
    failures += expect_span_text(child_at(order_clause, 0U), "k", "group order alias");
    failures += expect_order_direction(
        child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "group order desc"
    );
    failures += expect_span_text(child_at(limit_clause, 0U), "1", "group limit row count");
    failures += expect_span_text(child_at(limit_clause, 1U), "1", "group limit offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT g, COUNT(*) FROM numbers GROUP BY 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT g, COUNT(*) FROM numbers GROUP BY g, n;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT g, COUNT(*) FROM numbers GROUP BY g HAVING COUNT(*) > 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    having_clause = first_child_kind(statement, MYLITE_SQL_AST_HAVING_CLAUSE);
    failures += expect_node(having_clause, MYLITE_SQL_AST_HAVING_CLAUSE, "count having");
    failures += expect_node(
        child_at(child_at(having_clause, 0U), 0U),
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "having count star"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT g, COUNT(*) FROM numbers GROUP BY g HAVING COUNT(*) + 1 > 2;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_distinct_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT DISTINCT n FROM simple_lifecycle WHERE n IS NOT NULL ORDER BY n DESC "
        "LIMIT 1 OFFSET 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    order_clause = first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "select distinct modifier"
    );
    failures += expect_child_count(select_list, 1U, "select distinct item count");
    failures +=
        expect_span_text(child_at(child_at(select_list, 0U), 0U), "n", "select distinct column");
    failures += expect_node(child_at(statement, 1U), MYLITE_SQL_AST_FROM_TABLE, "distinct table");
    failures += expect_node(child_at(statement, 2U), MYLITE_SQL_AST_WHERE_CLAUSE, "distinct where");
    failures += expect_span_text(child_at(order_clause, 0U), "n", "distinct order key");
    failures += expect_order_direction(
        child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "distinct desc direction"
    );
    failures += expect_span_text(child_at(limit_clause, 0U), "1", "distinct limit row count");
    failures += expect_span_text(child_at(limit_clause, 1U), "0", "distinct limit offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT DISTINCT * FROM simple_lifecycle ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "select distinct wildcard modifier"
    );
    failures += expect_node(
        child_at(child_at(child_at(statement, 0U), 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "select distinct wildcard"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT DISTINCT n, nn FROM simple_lifecycle ORDER BY n;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "select distinct multiple modifier"
    );
    failures += expect_child_count(child_at(statement, 0U), 2U, "select distinct multiple items");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT DISTINCTROW n FROM simple_lifecycle WHERE n IS NOT NULL ORDER BY n DESC "
        "LIMIT 1 OFFSET 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    order_clause = first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "select distinctrow modifier"
    );
    failures += expect_child_count(select_list, 1U, "select distinctrow item count");
    failures +=
        expect_span_text(child_at(child_at(select_list, 0U), 0U), "n", "select distinctrow column");
    failures += expect_order_direction(
        child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "distinctrow desc direction"
    );
    failures += expect_span_text(child_at(limit_clause, 0U), "1", "distinctrow limit row count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT DISTINCTROW * FROM simple_lifecycle ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "select distinctrow wildcard modifier"
    );
    failures += expect_node(
        child_at(child_at(child_at(statement, 0U), 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "select distinctrow wildcard"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DISTINCT n;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "select distinct scalar parsed for runtime rejection"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT DISTINCT n FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "select distinct dual parsed for runtime rejection"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT DISTINCTROW n;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "select distinctrow scalar parsed for runtime rejection"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT DISTINCTROW n FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "select distinctrow dual parsed for runtime rejection"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_sql_calc_found_rows_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT SQL_CALC_FOUND_ROWS n FROM simple_lifecycle WHERE n IS NOT NULL "
        "ORDER BY n DESC LIMIT 1 OFFSET 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    order_clause = first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "sql calc default modifier"
    );
    failures += expect_true(
        mylite_sql_ast_node_select_calc_found_rows(statement),
        "sql calc found rows flag"
    );
    failures += expect_child_count(select_list, 1U, "sql calc item count");
    failures += expect_span_text(child_at(child_at(select_list, 0U), 0U), "n", "sql calc column");
    failures += expect_node(child_at(statement, 1U), MYLITE_SQL_AST_FROM_TABLE, "sql calc table");
    failures += expect_node(child_at(statement, 2U), MYLITE_SQL_AST_WHERE_CLAUSE, "sql calc where");
    failures += expect_span_text(child_at(order_clause, 0U), "n", "sql calc order key");
    failures += expect_order_direction(
        child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "sql calc desc direction"
    );
    failures += expect_span_text(child_at(limit_clause, 0U), "1", "sql calc limit row count");
    failures += expect_span_text(child_at(limit_clause, 1U), "0", "sql calc limit offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT ALL SQL_CALC_FOUND_ROWS * FROM simple_lifecycle ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "all sql calc default modifier"
    );
    failures += expect_true(
        mylite_sql_ast_node_select_calc_found_rows(statement),
        "all sql calc found rows flag"
    );
    failures += expect_node(
        child_at(child_at(child_at(statement, 0U), 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "all sql calc wildcard"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT SQL_CALC_FOUND_ROWS COUNT(*) FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_calc_found_rows(statement),
        "sql calc aggregate parsed for runtime rejection"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SQL_CALC_FOUND_ROWS 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_calc_found_rows(statement),
        "sql calc scalar parsed for runtime rejection"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT SQL_CALC_FOUND_ROWS n FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_calc_found_rows(statement),
        "sql calc dual parsed for runtime rejection"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT DISTINCT SQL_CALC_FOUND_ROWS n FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "distinct sql calc modifier parsed for runtime rejection"
    );
    failures += expect_true(
        mylite_sql_ast_node_select_calc_found_rows(statement),
        "distinct sql calc flag parsed for runtime rejection"
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT SQL_CALC_FOUND_ROWS DISTINCT n FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT SQL_CALC_FOUND_ROWS ALL n FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_noop_modifier_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    unsigned int expected_options =
        MYLITE_SQL_AST_SELECT_OPTION_HIGH_PRIORITY | MYLITE_SQL_AST_SELECT_OPTION_STRAIGHT_JOIN |
        MYLITE_SQL_AST_SELECT_OPTION_SQL_SMALL_RESULT |
        MYLITE_SQL_AST_SELECT_OPTION_SQL_BIG_RESULT |
        MYLITE_SQL_AST_SELECT_OPTION_SQL_BUFFER_RESULT | MYLITE_SQL_AST_SELECT_OPTION_SQL_NO_CACHE;
    int failures = 0;

    failures += parse_sql(
        "SELECT HIGH_PRIORITY STRAIGHT_JOIN SQL_SMALL_RESULT SQL_BIG_RESULT "
        "SQL_BUFFER_RESULT SQL_NO_CACHE 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_options(statement) == expected_options,
        "scalar noop select modifiers"
    );
    failures += expect_true(
        !mylite_sql_ast_node_select_calc_found_rows(statement),
        "scalar noop select no sql calc flag"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT DISTINCT HIGH_PRIORITY STRAIGHT_JOIN SQL_SMALL_RESULT SQL_BIG_RESULT "
        "SQL_BUFFER_RESULT SQL_NO_CACHE SQL_CALC_FOUND_ROWS n FROM simple_lifecycle "
        "ORDER BY n DESC LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "table noop select distinct modifier"
    );
    failures += expect_true(
        mylite_sql_ast_node_select_options(statement) == expected_options,
        "table noop select modifiers"
    );
    failures += expect_true(
        mylite_sql_ast_node_select_calc_found_rows(statement),
        "table noop select sql calc flag"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT SQL_NO_CACHE SQL_BUFFER_RESULT id FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT HIGH_PRIORITY HIGH_PRIORITY id FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT SQL_CALC_FOUND_ROWS SQL_NO_CACHE id FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_locking_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_source = NULL;
    int failures = 0;

    failures += parse_sql("SELECT 1 FOR UPDATE;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_locking_clause(statement) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_UPDATE,
        "scalar select for update"
    );
    failures += expect_span_text(statement, "SELECT 1 FOR UPDATE", "scalar select for update span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1 FROM DUAL FOR SHARE;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_locking_clause(statement) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_SHARE,
        "dual select for share"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle WHERE id = 1 ORDER BY id LIMIT 1 FOR UPDATE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_locking_clause(statement) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_UPDATE,
        "table select for update"
    );
    failures += expect_node(
        first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE),
        MYLITE_SQL_AST_ORDER_BY_CLAUSE,
        "locking select order clause"
    );
    failures += expect_node(
        first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE),
        MYLITE_SQL_AST_LIMIT_CLAUSE,
        "locking select limit clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT * FROM simple_lifecycle LOCK IN SHARE MODE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_locking_clause(statement) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_LOCK_IN_SHARE_MODE,
        "table select lock in share mode"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT n, COUNT(*) FROM simple_lifecycle GROUP BY n ORDER BY n FOR SHARE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_locking_clause(statement) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_SHARE,
        "grouped select for share"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT INTO copy SELECT id FROM simple_lifecycle FOR UPDATE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    select_source = child_at(statement, 2U);
    failures += expect_true(
        mylite_sql_ast_node_select_locking_clause(select_source) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_UPDATE,
        "insert select for update"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "REPLACE INTO copy SELECT id FROM simple_lifecycle FOR SHARE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    select_source = child_at(statement, 2U);
    failures += expect_true(
        mylite_sql_ast_node_select_locking_clause(select_source) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_SHARE,
        "replace select for share"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE copy AS SELECT id FROM simple_lifecycle FOR UPDATE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    select_source = child_at(statement, 1U);
    failures += expect_true(
        mylite_sql_ast_node_select_locking_clause(select_source) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_FOR_UPDATE,
        "ctas select for update"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle FOR UPDATE NOWAIT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT id FROM simple_lifecycle FOR SHARE SKIP LOCKED;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT id FROM simple_lifecycle FOR UPDATE OF simple_lifecycle;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT id FROM simple_lifecycle FOR UPDATE FOR SHARE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT id FROM simple_lifecycle FOR UPDATE ORDER BY id;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_all_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures += parse_sql("SELECT ALL 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "select all scalar modifier"
    );
    failures += expect_child_count(select_list, 1U, "select all scalar item count");
    failures += expect_literal(
        child_at(child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "select all scalar literal"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ALL 1 FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "select all dual modifier"
    );
    failures += expect_node(child_at(statement, 1U), MYLITE_SQL_AST_FROM_DUAL, "select all dual");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT ALL n FROM simple_lifecycle WHERE n IS NOT NULL ORDER BY n DESC "
        "LIMIT 1 OFFSET 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    order_clause = first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "select all table modifier"
    );
    failures += expect_span_text(child_at(child_at(select_list, 0U), 0U), "n", "select all column");
    failures += expect_node(child_at(statement, 1U), MYLITE_SQL_AST_FROM_TABLE, "select all table");
    failures +=
        expect_node(child_at(statement, 2U), MYLITE_SQL_AST_WHERE_CLAUSE, "select all where");
    failures += expect_span_text(child_at(order_clause, 0U), "n", "select all order key");
    failures += expect_order_direction(
        child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "select all desc direction"
    );
    failures += expect_span_text(child_at(limit_clause, 0U), "1", "select all limit row count");
    failures += expect_span_text(child_at(limit_clause, 1U), "0", "select all limit offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT ALL * FROM simple_lifecycle ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "select all wildcard modifier"
    );
    failures += expect_node(
        child_at(child_at(child_at(statement, 0U), 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "select all wildcard"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ALL *;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "select all bare wildcard modifier"
    );
    failures += expect_node(
        child_at(child_at(child_at(statement, 0U), 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "select all bare wildcard"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ALL * FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "select all dual wildcard modifier"
    );
    failures += expect_node(
        child_at(child_at(child_at(statement, 0U), 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "select all dual wildcard"
    );
    failures +=
        expect_node(child_at(statement, 1U), MYLITE_SQL_AST_FROM_DUAL, "select all dual wildcard");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT ALL COUNT(*) FROM simple_lifecycle;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "select all aggregate modifier"
    );
    failures += expect_node(
        child_at(child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "select all count star"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ALL ALL 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT ALL DISTINCT n FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT ALL DISTINCTROW n FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT DISTINCT ALL n FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_union_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *terms = NULL;
    const struct mylite_sql_ast_node *first_term = NULL;
    const struct mylite_sql_ast_node *second_term = NULL;
    int failures = 0;

    failures += parse_sql("SELECT 1 UNION SELECT 2;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    terms = child_at(statement, 1U);
    first_term = child_at(terms, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT, "union statement");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_SELECT_STATEMENT, "union left");
    failures += expect_node(terms, MYLITE_SQL_AST_UNION_TERM_LIST, "union terms");
    failures += expect_child_count(terms, 1U, "union term count");
    failures += expect_node(first_term, MYLITE_SQL_AST_UNION_TERM, "union term");
    failures += expect_true(
        mylite_sql_ast_node_union_modifier(first_term) == MYLITE_SQL_AST_UNION_MODIFIER_DISTINCT,
        "default union modifier"
    );
    failures +=
        expect_node(child_at(first_term, 0U), MYLITE_SQL_AST_SELECT_STATEMENT, "union right");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM a UNION ALL SELECT id FROM b UNION DISTINCT SELECT id FROM c;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    terms = child_at(statement, 1U);
    first_term = child_at(terms, 0U);
    second_term = child_at(terms, 1U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT, "union chain statement");
    failures += expect_child_count(terms, 2U, "union chain term count");
    failures += expect_true(
        mylite_sql_ast_node_union_modifier(first_term) == MYLITE_SQL_AST_UNION_MODIFIER_ALL,
        "union all modifier"
    );
    failures += expect_true(
        mylite_sql_ast_node_union_modifier(second_term) == MYLITE_SQL_AST_UNION_MODIFIER_DISTINCT,
        "union distinct modifier"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(child_at(first_term, 0U), 0U), 0U), 0U),
        "id",
        "union all branch select item"
    );
    failures += expect_span_text(
        child_at(child_at(child_at(child_at(second_term, 0U), 0U), 0U), 0U),
        "id",
        "union distinct branch select item"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM a ORDER BY id UNION SELECT id FROM b;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        first_child_kind(child_at(statement, 0U), MYLITE_SQL_AST_ORDER_BY_CLAUSE),
        MYLITE_SQL_AST_ORDER_BY_CLAUSE,
        "union branch order parsed for runtime rejection"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_table_alias_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *from_table = NULL;
    const struct mylite_sql_ast_node *hint_list = NULL;
    const struct mylite_sql_ast_node *hint = NULL;
    const struct mylite_sql_ast_node *name_list = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT n FROM simple_lifecycle AS s WHERE n IS NOT NULL ORDER BY n DESC "
        "LIMIT 1 OFFSET 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    from_table = child_at(statement, 1U);
    order_clause = first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += expect_node(from_table, MYLITE_SQL_AST_FROM_TABLE, "alias from table");
    failures += expect_child_count(from_table, 2U, "alias from table child count");
    failures += expect_span_text(child_at(from_table, 0U), "simple_lifecycle", "alias table");
    failures += expect_span_text(child_at(from_table, 1U), "s", "as alias");
    failures += expect_node(child_at(statement, 2U), MYLITE_SQL_AST_WHERE_CLAUSE, "alias where");
    failures += expect_span_text(child_at(order_clause, 0U), "n", "alias order key");
    failures += expect_order_direction(
        child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "alias desc direction"
    );
    failures += expect_span_text(child_at(limit_clause, 0U), "1", "alias limit row count");
    failures += expect_span_text(child_at(limit_clause, 1U), "0", "alias limit offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT * FROM app.simple_lifecycle s ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    from_table = child_at(statement, 1U);
    failures += expect_node(
        child_at(child_at(child_at(statement, 0U), 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "bare alias wildcard"
    );
    failures += expect_child_count(from_table, 2U, "bare alias child count");
    failures += expect_span_text(child_at(from_table, 0U), "app.simple_lifecycle", "schema table");
    failures += expect_span_text(child_at(from_table, 1U), "s", "bare alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT ALL n FROM simple_lifecycle AS `select` ORDER BY n;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    from_table = child_at(statement, 1U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT,
        "all alias modifier"
    );
    failures += expect_span_text(child_at(from_table, 1U), "`select`", "quoted alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT DISTINCT n FROM simple_lifecycle s ORDER BY n;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    from_table = child_at(statement, 1U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "distinct alias modifier"
    );
    failures += expect_span_text(child_at(from_table, 1U), "s", "distinct alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT DISTINCTROW n FROM simple_lifecycle AS s ORDER BY n;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    from_table = child_at(statement, 1U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "distinctrow alias modifier"
    );
    failures += expect_span_text(child_at(from_table, 1U), "s", "distinctrow alias");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT COUNT(*) FROM simple_lifecycle AS s;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    from_table = child_at(statement, 1U);
    failures += expect_node(
        child_at(child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "count alias function"
    );
    failures += expect_span_text(child_at(from_table, 1U), "s", "count alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT MIN(n) FROM simple_lifecycle s;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    from_table = child_at(statement, 1U);
    failures += expect_span_text(child_at(from_table, 1U), "s", "min alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle USE INDEX (k_id) WHERE id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    from_table = child_at(statement, 1U);
    hint_list = child_at(from_table, 1U);
    hint = child_at(hint_list, 0U);
    name_list = child_at(hint, 0U);
    failures += expect_child_count(from_table, 2U, "hint without alias child count");
    failures += expect_node(hint_list, MYLITE_SQL_AST_INDEX_HINT_LIST, "use hint list");
    failures += expect_node(hint, MYLITE_SQL_AST_USE_INDEX_HINT, "use index hint");
    failures += expect_node(name_list, MYLITE_SQL_AST_IDENTIFIER_LIST, "use index names");
    failures += expect_span_text(child_at(name_list, 0U), "k_id", "use index name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle AS s FORCE KEY FOR ORDER BY (PRIMARY, k_n) "
        "ORDER BY s.id;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    from_table = child_at(statement, 1U);
    hint_list = child_at(from_table, 2U);
    hint = child_at(hint_list, 0U);
    failures += expect_child_count(from_table, 3U, "hint with alias child count");
    failures += expect_span_text(child_at(from_table, 1U), "s", "hint alias");
    failures += expect_node(hint_list, MYLITE_SQL_AST_INDEX_HINT_LIST, "force hint list");
    failures += expect_node(hint, MYLITE_SQL_AST_FORCE_INDEX_HINT, "force key hint");
    failures += expect_node(
        child_at(hint, 0U),
        MYLITE_SQL_AST_INDEX_HINT_FOR_ORDER_BY,
        "force order scope"
    );
    name_list = child_at(hint, 1U);
    failures += expect_span_text(child_at(name_list, 0U), "PRIMARY", "primary hint name");
    failures += expect_span_text(child_at(name_list, 1U), "k_n", "force second hint name");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM simple_lifecycle USE INDEX ();", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    from_table = child_at(statement, 1U);
    hint = child_at(child_at(from_table, 1U), 0U);
    name_list = child_at(hint, 0U);
    failures += expect_child_count(name_list, 0U, "empty use index names");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM simple_lifecycle FORCE INDEX ();",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT id FROM simple_lifecycle IGNORE INDEX ();",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT n FROM simple_lifecycle AS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT n FROM simple_lifecycle AS WHERE n = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_inner_join_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *from_join = NULL;
    const struct mylite_sql_ast_node *left_source = NULL;
    const struct mylite_sql_ast_node *right_source = NULL;
    const struct mylite_sql_ast_node *hint_list = NULL;
    const struct mylite_sql_ast_node *hint = NULL;
    const struct mylite_sql_ast_node *condition = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT l.id, r.w FROM lefts AS l JOIN rights AS r ON l.k = r.k "
        "WHERE l.v = 100 ORDER BY r.w LIMIT 1 OFFSET 0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    from_join = child_at(statement, 1U);
    left_source = child_at(from_join, 0U);
    right_source = child_at(from_join, 1U);
    condition = child_at(from_join, 2U);
    order_clause = first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "join from clause");
    failures += expect_child_count(from_join, 3U, "join child count");
    failures += expect_true(
        mylite_sql_ast_node_join_kind(from_join) == MYLITE_SQL_AST_JOIN_KIND_INNER,
        "join kind"
    );
    failures += expect_node(left_source, MYLITE_SQL_AST_FROM_TABLE, "join left source");
    failures += expect_span_text(child_at(left_source, 0U), "lefts", "join left table");
    failures += expect_span_text(child_at(left_source, 1U), "l", "join left alias");
    failures += expect_node(right_source, MYLITE_SQL_AST_FROM_TABLE, "join right source");
    failures += expect_span_text(child_at(right_source, 0U), "rights", "join right table");
    failures += expect_span_text(child_at(right_source, 1U), "r", "join right alias");
    failures += expect_node(condition, MYLITE_SQL_AST_COMPARISON_PREDICATE, "join on condition");
    failures += expect_operator(condition, MYLITE_SQL_AST_OPERATOR_EQUAL, "join equality operator");
    failures += expect_span_text(child_at(condition, 0U), "l.k", "join left key");
    failures += expect_span_text(child_at(condition, 1U), "r.k", "join right key");
    failures += expect_span_text(child_at(order_clause, 0U), "r.w", "join order key");
    failures += expect_span_text(child_at(limit_clause, 0U), "1", "join limit row count");
    failures += expect_span_text(child_at(limit_clause, 1U), "0", "join limit offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT l.id FROM lefts AS l USE INDEX (k_l) "
        "JOIN rights r IGNORE KEY FOR JOIN (k_r, PRIMARY) ON l.k = r.k;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    from_join = child_at(statement, 1U);
    left_source = child_at(from_join, 0U);
    right_source = child_at(from_join, 1U);
    hint_list = child_at(left_source, 2U);
    hint = child_at(hint_list, 0U);
    failures += expect_node(hint_list, MYLITE_SQL_AST_INDEX_HINT_LIST, "join left hint list");
    failures += expect_node(hint, MYLITE_SQL_AST_USE_INDEX_HINT, "join left use hint");
    failures += expect_span_text(child_at(child_at(hint, 0U), 0U), "k_l", "join left hint name");
    hint_list = child_at(right_source, 2U);
    hint = child_at(hint_list, 0U);
    failures += expect_span_text(child_at(right_source, 1U), "r", "join right alias stable");
    failures += expect_node(hint, MYLITE_SQL_AST_IGNORE_INDEX_HINT, "join right ignore hint");
    failures += expect_node(
        child_at(hint, 0U),
        MYLITE_SQL_AST_INDEX_HINT_FOR_JOIN,
        "join right hint scope"
    );
    failures += expect_span_text(child_at(child_at(hint, 1U), 1U), "PRIMARY", "join primary hint");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM lefts CROSS JOIN rights;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    from_join = child_at(statement, 1U);
    failures += expect_node(
        child_at(child_at(child_at(statement, 0U), 0U), 0U),
        MYLITE_SQL_AST_WILDCARD,
        "cross join wildcard"
    );
    failures += expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "cross join from clause");
    failures += expect_child_count(from_join, 2U, "cross join omits condition child");
    failures += expect_true(
        mylite_sql_ast_node_join_kind(from_join) == MYLITE_SQL_AST_JOIN_KIND_INNER,
        "cross join kind"
    );
    failures += expect_span_text(child_at(child_at(from_join, 0U), 0U), "lefts", "cross left");
    failures += expect_span_text(child_at(child_at(from_join, 1U), 0U), "rights", "cross right");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT l.id FROM lefts l INNER JOIN rights r ON l.k = r.k;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    from_join = child_at(statement, 1U);
    failures += expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "inner join");
    failures += expect_true(
        mylite_sql_ast_node_join_kind(from_join) == MYLITE_SQL_AST_JOIN_KIND_INNER,
        "inner join kind"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT l.id FROM lefts l LEFT JOIN rights r ON l.k = r.k;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    from_join = child_at(statement, 1U);
    failures += expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "left join");
    failures += expect_true(
        mylite_sql_ast_node_join_kind(from_join) == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER,
        "left join kind"
    );
    failures += expect_child_count(from_join, 3U, "left join child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT l.id FROM lefts l LEFT OUTER JOIN rights r ON l.k = r.k;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    from_join = child_at(statement, 1U);
    failures += expect_node(from_join, MYLITE_SQL_AST_FROM_JOIN, "left outer join");
    failures += expect_true(
        mylite_sql_ast_node_join_kind(from_join) == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER,
        "left outer join kind"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_item_alias_clause(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_item = NULL;
    const struct mylite_sql_ast_node *second_item = NULL;
    const struct mylite_sql_ast_node *third_item = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    int failures = 0;

    failures += parse_sql(
        "SELECT n AS x, nn y, nums.n AS `Customer identity`, n 'literal alias' "
        "FROM numbers AS nums ORDER BY x DESC LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    first_item = child_at(select_list, 0U);
    second_item = child_at(select_list, 1U);
    third_item = child_at(select_list, 2U);
    order_clause = first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    failures += expect_child_count(select_list, 4U, "select item alias count");
    failures += expect_span_text(child_at(first_item, 0U), "n", "as alias expression");
    failures += expect_span_text(child_at(first_item, 1U), "x", "as alias identifier");
    failures += expect_span_text(child_at(second_item, 0U), "nn", "bare alias expression");
    failures += expect_span_text(child_at(second_item, 1U), "y", "bare alias identifier");
    failures += expect_span_text(child_at(third_item, 0U), "nums.n", "qualified alias expression");
    failures += expect_span_text(
        child_at(third_item, 1U),
        "`Customer identity`",
        "quoted identifier select alias"
    );
    failures += expect_literal(
        child_at(child_at(select_list, 3U), 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "string select alias"
    );
    failures += expect_span_text(child_at(order_clause, 0U), "x", "alias order key");
    failures += expect_order_direction(
        child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "alias order direction"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT DISTINCT n AS x FROM numbers ORDER BY x;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    failures += expect_true(
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT,
        "distinct select item alias modifier"
    );
    failures += expect_span_text(
        child_at(child_at(select_list, 0U), 1U),
        "x",
        "distinct select item alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT COUNT(*) AS c FROM numbers;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    failures += expect_node(
        child_at(child_at(select_list, 0U), 0U),
        MYLITE_SQL_AST_COUNT_STAR_FUNCTION,
        "count select item alias expression"
    );
    failures += expect_span_text(child_at(child_at(select_list, 0U), 1U), "c", "count alias");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT DATABASE() AS d, USER() u FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    select_list = child_at(statement, 0U);
    failures +=
        expect_span_text(child_at(child_at(select_list, 0U), 1U), "d", "database function alias");
    failures +=
        expect_span_text(child_at(child_at(select_list, 1U), 1U), "u", "user function alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * AS x FROM numbers;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SELECT n AS 'x' FROM numbers ORDER BY 'x';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_insert_select_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql(
        "INSERT INTO app.simple_lifecycle (id, amount) "
        "SELECT id, amount FROM app.source_lifecycle WHERE id >= 1 ORDER BY amount DESC LIMIT 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_INSERT_SELECT_STATEMENT, "insert select statement");
    failures += expect_child_count(statement, 3U, "insert select statement child count");
    failures +=
        expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "insert select target");
    failures += expect_node(
        child_at(statement, 1U),
        MYLITE_SQL_AST_IDENTIFIER_LIST,
        "insert select target columns"
    );
    failures += expect_child_count(child_at(statement, 1U), 2U, "insert select target count");
    failures +=
        expect_node(child_at(statement, 2U), MYLITE_SQL_AST_SELECT_STATEMENT, "insert source");
    failures += expect_node(
        first_child_kind(child_at(statement, 2U), MYLITE_SQL_AST_ORDER_BY_CLAUSE),
        MYLITE_SQL_AST_ORDER_BY_CLAUSE,
        "insert select order clause"
    );
    failures += expect_node(
        first_child_kind(child_at(statement, 2U), MYLITE_SQL_AST_LIMIT_CLAUSE),
        MYLITE_SQL_AST_LIMIT_CLAUSE,
        "insert select limit clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT simple_lifecycle SELECT * FROM source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_INSERT_SELECT_STATEMENT, "insert select no into");
    failures +=
        expect_span_text(child_at(statement, 0U), "simple_lifecycle", "insert select target");
    failures += expect_child_count(child_at(statement, 1U), 0U, "insert select implicit columns");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT INTO app.simple_lifecycle (id, amount) "
        "SELECT 1, 'ok' FROM DUAL WHERE NOT EXISTS (SELECT 1 FROM app.guard WHERE id = 1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
        "insert select dual where statement"
    );
    failures +=
        expect_node(child_at(statement, 2U), MYLITE_SQL_AST_SELECT_STATEMENT, "dual source");
    failures += expect_node(
        child_at(child_at(statement, 2U), 1U),
        MYLITE_SQL_AST_FROM_DUAL,
        "insert select dual source"
    );
    failures += expect_node(
        first_child_kind(child_at(statement, 2U), MYLITE_SQL_AST_WHERE_CLAUSE),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "insert select dual where clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT INTO app.simple_lifecycle (id, amount) "
        "SELECT id, amount FROM app.source_a UNION ALL SELECT id, amount FROM app.source_b;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
        "insert select union source statement"
    );
    failures += expect_node(
        child_at(statement, 2U),
        MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT,
        "insert select union source"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_replace_select_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql(
        "REPLACE INTO app.simple_lifecycle (id, amount) "
        "SELECT id, amount FROM app.source_lifecycle WHERE id >= 1 ORDER BY amount DESC LIMIT 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT, "replace select statement");
    failures += expect_child_count(statement, 3U, "replace select statement child count");
    failures +=
        expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "replace select target");
    failures += expect_node(
        child_at(statement, 1U),
        MYLITE_SQL_AST_IDENTIFIER_LIST,
        "replace select target columns"
    );
    failures += expect_child_count(child_at(statement, 1U), 2U, "replace select target count");
    failures +=
        expect_node(child_at(statement, 2U), MYLITE_SQL_AST_SELECT_STATEMENT, "replace source");
    failures += expect_node(
        first_child_kind(child_at(statement, 2U), MYLITE_SQL_AST_ORDER_BY_CLAUSE),
        MYLITE_SQL_AST_ORDER_BY_CLAUSE,
        "replace select order clause"
    );
    failures += expect_node(
        first_child_kind(child_at(statement, 2U), MYLITE_SQL_AST_LIMIT_CLAUSE),
        MYLITE_SQL_AST_LIMIT_CLAUSE,
        "replace select limit clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "REPLACE simple_lifecycle SELECT * FROM source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT, "replace select no into");
    failures +=
        expect_span_text(child_at(statement, 0U), "simple_lifecycle", "replace select target");
    failures += expect_child_count(child_at(statement, 1U), 0U, "replace select implicit columns");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t SELECT 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT, "replace select no source");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_replace_modifier_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql(
        "REPLACE LOW_PRIORITY INTO app.simple_lifecycle (id) VALUES (1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT,
        "low priority replace values"
    );
    failures += expect_child_count(statement, 4U, "low priority replace values child count");
    failures += expect_node(
        child_at(statement, 3U),
        MYLITE_SQL_AST_REPLACE_LOW_PRIORITY_MODIFIER,
        "low priority replace values modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("REPLACE DELAYED simple_lifecycle VALUES (2);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT, "delayed replace values");
    failures += expect_child_count(statement, 4U, "delayed replace values child count");
    failures += expect_node(
        child_at(statement, 3U),
        MYLITE_SQL_AST_REPLACE_DELAYED_MODIFIER,
        "delayed replace values modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "REPLACE DELAYED INTO app.simple_lifecycle SET id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_REPLACE_SET_STATEMENT, "delayed replace set");
    failures += expect_child_count(statement, 3U, "delayed replace set child count");
    failures += expect_node(
        child_at(statement, 2U),
        MYLITE_SQL_AST_REPLACE_DELAYED_MODIFIER,
        "delayed replace set modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "REPLACE INTO app.simple_lifecycle (id) VALUES (DEFAULT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT, "replace default");
    failures += expect_node(
        child_at(child_at(child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_DML_DEFAULT_VALUE,
        "replace values default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "REPLACE INTO app.simple_lifecycle SET id = DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_REPLACE_SET_STATEMENT, "replace set default");
    failures += expect_node(
        child_at(child_at(child_at(statement, 1U), 0U), 1U),
        MYLITE_SQL_AST_DML_DEFAULT_VALUE,
        "replace set default value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "REPLACE LOW_PRIORITY INTO app.simple_lifecycle (id) SELECT id FROM app.source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT,
        "low priority replace select"
    );
    failures += expect_child_count(statement, 4U, "low priority replace select child count");
    failures += expect_node(
        child_at(statement, 3U),
        MYLITE_SQL_AST_REPLACE_LOW_PRIORITY_MODIFIER,
        "low priority replace select modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "REPLACE DELAYED simple_lifecycle SELECT * FROM source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT, "delayed replace select");
    failures += expect_child_count(statement, 4U, "delayed replace select child count");
    failures += expect_node(
        child_at(statement, 3U),
        MYLITE_SQL_AST_REPLACE_DELAYED_MODIFIER,
        "delayed replace select modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_insert_modifier_statements(void) {
    const size_t priority_ignore_child_count = 5U;
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql(
        "INSERT LOW_PRIORITY INTO app.simple_lifecycle (id) VALUES (1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "low priority insert values");
    failures += expect_child_count(statement, 4U, "low priority insert values child count");
    failures += expect_node(
        child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_LOW_PRIORITY_MODIFIER,
        "low priority insert values modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT HIGH_PRIORITY simple_lifecycle VALUES (2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "high priority insert values");
    failures += expect_child_count(statement, 4U, "high priority insert values child count");
    failures += expect_node(
        child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_HIGH_PRIORITY_MODIFIER,
        "high priority insert values modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT IGNORE INTO app.simple_lifecycle (id) VALUES (3);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "ignore insert values");
    failures += expect_child_count(statement, 4U, "ignore insert values child count");
    failures += expect_node(
        child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER,
        "ignore insert values modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT LOW_PRIORITY IGNORE INTO app.simple_lifecycle (id) VALUES (4);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "low priority ignore values");
    failures += expect_child_count(
        statement,
        priority_ignore_child_count,
        "low priority ignore values child count"
    );
    failures += expect_node(
        child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_LOW_PRIORITY_MODIFIER,
        "low priority ignore values priority modifier"
    );
    failures += expect_node(
        child_at(statement, 4U),
        MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER,
        "low priority ignore values ignore modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT simple_lifecycle VALUES (3);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "insert values no into");
    failures += expect_child_count(statement, 3U, "insert values no into child count");
    failures +=
        expect_span_text(child_at(statement, 0U), "simple_lifecycle", "insert values target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT DELAYED INTO app.simple_lifecycle SET id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_SET_STATEMENT, "delayed insert set");
    failures += expect_child_count(statement, 3U, "delayed insert set child count");
    failures += expect_node(
        child_at(statement, 2U),
        MYLITE_SQL_AST_INSERT_DELAYED_MODIFIER,
        "delayed insert set modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT DELAYED IGNORE INTO app.simple_lifecycle SET id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_INSERT_SET_STATEMENT, "delayed ignore insert set");
    failures += expect_child_count(statement, 4U, "delayed ignore insert set child count");
    failures += expect_node(
        child_at(statement, 2U),
        MYLITE_SQL_AST_INSERT_DELAYED_MODIFIER,
        "delayed ignore insert set delayed modifier"
    );
    failures += expect_node(
        child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER,
        "delayed ignore insert set ignore modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT INTO app.simple_lifecycle (id) VALUES (DEFAULT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "insert default");
    failures += expect_node(
        child_at(child_at(child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_DML_DEFAULT_VALUE,
        "insert values default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT INTO app.simple_lifecycle SET id = DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_SET_STATEMENT, "insert set default");
    failures += expect_node(
        child_at(child_at(child_at(statement, 1U), 0U), 1U),
        MYLITE_SQL_AST_DML_DEFAULT_VALUE,
        "insert set default value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT HIGH_PRIORITY INTO app.simple_lifecycle (id) SELECT id FROM app.source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
        "high priority insert select"
    );
    failures += expect_child_count(statement, 4U, "high priority insert select child count");
    failures += expect_node(
        child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_HIGH_PRIORITY_MODIFIER,
        "high priority insert select modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT IGNORE INTO app.simple_lifecycle (id) SELECT id FROM app.source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_INSERT_SELECT_STATEMENT, "ignore insert select");
    failures += expect_child_count(statement, 4U, "ignore insert select child count");
    failures += expect_node(
        child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER,
        "ignore insert select modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT LOW_PRIORITY IGNORE INTO app.simple_lifecycle (id) "
        "SELECT id FROM app.source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
        "low priority ignore insert select"
    );
    failures += expect_child_count(
        statement,
        priority_ignore_child_count,
        "low priority ignore insert select child count"
    );
    failures += expect_node(
        child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_LOW_PRIORITY_MODIFIER,
        "low priority ignore insert select priority modifier"
    );
    failures += expect_node(
        child_at(statement, 4U),
        MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER,
        "low priority ignore insert select ignore modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT HIGH_PRIORITY IGNORE simple_lifecycle SELECT * FROM source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
        "high priority ignore insert select"
    );
    failures += expect_child_count(
        statement,
        priority_ignore_child_count,
        "high priority ignore insert select child count"
    );
    failures += expect_node(
        child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_HIGH_PRIORITY_MODIFIER,
        "high priority ignore insert select priority modifier"
    );
    failures += expect_node(
        child_at(statement, 4U),
        MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER,
        "high priority ignore insert select ignore modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT DELAYED simple_lifecycle SELECT * FROM source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_INSERT_SELECT_STATEMENT, "delayed insert select");
    failures += expect_child_count(statement, 4U, "delayed insert select child count");
    failures += expect_node(
        child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_DELAYED_MODIFIER,
        "delayed insert select modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT DELAYED IGNORE simple_lifecycle SELECT * FROM source_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SELECT_STATEMENT,
        "delayed ignore insert select"
    );
    failures += expect_child_count(
        statement,
        priority_ignore_child_count,
        "delayed ignore insert select child count"
    );
    failures += expect_node(
        child_at(statement, 3U),
        MYLITE_SQL_AST_INSERT_DELAYED_MODIFIER,
        "delayed ignore insert select delayed modifier"
    );
    failures += expect_node(
        child_at(statement, 4U),
        MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER,
        "delayed ignore insert select ignore modifier"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_insert_on_duplicate_key_update_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *clause = NULL;
    const struct mylite_sql_ast_node *assignments = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    const struct mylite_sql_ast_node *value = NULL;
    int failures = 0;

    failures += parse_sql(
        "INSERT INTO app.simple_lifecycle (id, amount) VALUES (1, 2) "
        "ON DUPLICATE KEY UPDATE amount = VALUES(amount);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    clause = child_at(statement, 3U);
    assignments = child_at(clause, 0U);
    assignment = child_at(assignments, 0U);
    value = child_at(assignment, 1U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "duplicate insert");
    failures += expect_child_count(statement, 4U, "duplicate insert child count");
    failures += expect_node(
        clause,
        MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE,
        "duplicate insert clause"
    );
    failures += expect_node(
        assignments,
        MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT_LIST,
        "duplicate assignment list"
    );
    failures += expect_child_count(assignments, 1U, "duplicate assignment count");
    failures +=
        expect_node(assignment, MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT, "duplicate assignment");
    failures += expect_span_text(child_at(assignment, 0U), "amount", "duplicate assignment target");
    failures += expect_node(value, MYLITE_SQL_AST_INSERT_VALUES_REFERENCE, "values reference");
    failures += expect_span_text(child_at(value, 0U), "amount", "values reference column");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT DELAYED simple_lifecycle SET id = 1, amount = 2 "
        "ON DUPLICATE KEY UPDATE amount = DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    clause = child_at(statement, 3U);
    assignments = child_at(clause, 0U);
    assignment = child_at(assignments, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_SET_STATEMENT, "duplicate insert set");
    failures += expect_child_count(statement, 4U, "duplicate insert set child count");
    failures += expect_node(
        child_at(statement, 2U),
        MYLITE_SQL_AST_INSERT_DELAYED_MODIFIER,
        "duplicate insert set modifier"
    );
    failures += expect_node(
        clause,
        MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE,
        "duplicate insert set clause"
    );
    failures += expect_node(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_DML_DEFAULT_VALUE,
        "duplicate default value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT INTO simple_lifecycle VALUES (1) "
        "ON DUPLICATE KEY UPDATE simple_lifecycle.id = VALUES(simple_lifecycle.id), id = 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    clause = child_at(statement, 3U);
    assignments = child_at(clause, 0U);
    failures += expect_child_count(assignments, 2U, "duplicate wider assignment count");
    assignment = child_at(assignments, 0U);
    value = child_at(assignment, 1U);
    failures += expect_node(
        child_at(assignment, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "duplicate qualified target"
    );
    failures += expect_node(value, MYLITE_SQL_AST_INSERT_VALUES_REFERENCE, "qualified values ref");
    failures += expect_node(
        child_at(value, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "duplicate qualified values column"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_delete_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *assignment_list = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int failures = 0;

    failures += parse_sql("DELETE FROM app.simple_lifecycle;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DELETE_STATEMENT, "delete statement");
    failures += expect_child_count(statement, 1U, "delete statement target only");
    failures += expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "delete target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DELETE FROM simple_lifecycle WHERE id = +1 ORDER BY nn DESC LIMIT 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    where_clause = child_at(statement, 1U);
    order_clause = child_at(statement, 2U);
    limit_clause = child_at(statement, 3U);
    failures += expect_node(statement, MYLITE_SQL_AST_DELETE_STATEMENT, "qualified delete");
    failures += expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "delete where");
    failures += expect_operator(
        child_at(where_clause, 0U),
        MYLITE_SQL_AST_OPERATOR_EQUAL,
        "delete where operator"
    );
    failures += expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "delete order");
    failures += expect_span_text(child_at(order_clause, 0U), "nn", "delete order key");
    failures += expect_order_direction(
        child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "delete order direction"
    );
    failures += expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "delete limit");
    failures += expect_span_text(child_at(limit_clause, 0U), "2", "delete limit row count");
    failures += expect_true(child_at(limit_clause, 1U) == NULL, "delete limit has no offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DELETE FROM simple_lifecycle WHERE id = FALSE LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    where_clause = child_at(statement, 1U);
    failures += expect_literal(
        child_at(child_at(where_clause, 0U), 1U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "delete false predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DELETE FROM simple_lifecycle ORDER BY simple_lifecycle.id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    order_clause = first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    failures += expect_node(
        child_at(order_clause, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "delete qualified order key"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM simple_lifecycle LIMIT 0;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    limit_clause = first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "delete simple limit");
    failures += expect_span_text(child_at(limit_clause, 0U), "0", "delete simple limit row count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DELETE l FROM lefts AS l JOIN rights AS r ON l.k = r.k WHERE r.id IS NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    where_clause = child_at(statement, 2U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_JOINED_DELETE_STATEMENT, "joined delete statement");
    failures += expect_span_text(child_at(statement, 0U), "l", "joined delete target");
    failures +=
        expect_node(child_at(statement, 1U), MYLITE_SQL_AST_FROM_JOIN, "joined delete join");
    failures += expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "joined delete where");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DELETE FROM app.lefts USING app.lefts LEFT JOIN app.rights ON lefts.k = rights.k "
        "WHERE rights.id IS NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_JOINED_DELETE_STATEMENT,
        "using joined delete statement"
    );
    failures += expect_span_text(child_at(statement, 0U), "app.lefts", "using joined target");
    failures += expect_true(
        mylite_sql_ast_node_join_kind(child_at(statement, 1U)) ==
            MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER,
        "using joined delete left join kind"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DELETE l FROM lefts AS l JOIN rights AS r ON l.k = r.k ORDER BY l.id;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "DELETE l FROM lefts AS l JOIN rights AS r ON l.k = r.k LIMIT 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE lefts AS l JOIN rights AS r ON l.k = r.k SET l.v = 7 "
        "WHERE r.id IS NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    assignment_list = child_at(statement, 1U);
    assignment = child_at(assignment_list, 0U);
    where_clause = child_at(statement, 2U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_JOINED_UPDATE_STATEMENT, "joined update statement");
    failures +=
        expect_node(child_at(statement, 0U), MYLITE_SQL_AST_FROM_JOIN, "joined update join");
    failures += expect_true(
        mylite_sql_ast_node_join_kind(child_at(statement, 0U)) == MYLITE_SQL_AST_JOIN_KIND_INNER,
        "joined update join kind"
    );
    failures += expect_node(
        child_at(assignment, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "joined update qualified assignment target"
    );
    failures += expect_span_text(child_at(assignment, 0U), "l.v", "joined update assignment");
    failures += expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "joined update where");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE app.lefts AS l LEFT JOIN app.rights AS r ON l.k = r.k "
        "SET r.w = NULL ORDER BY r.id LIMIT 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 1U), 0U);
    order_clause = first_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    limit_clause = first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += expect_true(
        mylite_sql_ast_node_join_kind(child_at(statement, 0U)) ==
            MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER,
        "joined update left join kind"
    );
    failures += expect_literal(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "joined update NULL assignment"
    );
    failures += expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "joined update order");
    failures += expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "joined update limit");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE lefts JOIN rights SET lefts.v = 1 WHERE rights.id = 9;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_JOINED_UPDATE_STATEMENT,
        "joined update without ON statement"
    );
    failures += expect_true(
        child_at(child_at(statement, 0U), 2U) == NULL,
        "joined update without ON has no join predicate"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE lefts l JOIN rights r ON l.k = r.k SET l.v = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_set_fixed_system_variable_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *assignment_list = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    const struct mylite_sql_ast_node *target = NULL;
    const struct mylite_sql_ast_node *value = NULL;
    int failures = 0;

    failures += parse_sql("SET autocommit = 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment_list = child_at(statement, 0U);
    assignment = child_at(assignment_list, 0U);
    target = child_at(assignment, 0U);
    value = child_at(assignment, 1U);
    failures += expect_node(statement, MYLITE_SQL_AST_SET_STATEMENT, "set statement");
    failures += expect_child_count(statement, 1U, "set statement children");
    failures +=
        expect_node(assignment_list, MYLITE_SQL_AST_SET_ASSIGNMENT_LIST, "set assignment list");
    failures += expect_child_count(assignment_list, 1U, "set assignment count");
    failures += expect_node(assignment, MYLITE_SQL_AST_SET_ASSIGNMENT, "set assignment");
    failures += expect_child_count(assignment, 2U, "set assignment children");
    failures += expect_node(
        target,
        MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_TARGET,
        "set system variable target"
    );
    failures += expect_child_count(target, 1U, "set target unscoped child count");
    failures += expect_span_text(child_at(target, 0U), "autocommit", "set unscoped name");
    failures += expect_literal(value, MYLITE_SQL_AST_LITERAL_INTEGER, "set integer fixed value");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET SESSION `autocommit` = ON;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 0U), 0U);
    target = child_at(assignment, 0U);
    value = child_at(assignment, 1U);
    failures += expect_child_count(target, 2U, "set target scoped child count");
    failures += expect_span_text(child_at(target, 0U), "SESSION", "set target session scope");
    failures += expect_span_text(child_at(target, 1U), "`autocommit`", "set quoted name");
    failures += expect_literal(value, MYLITE_SQL_AST_LITERAL_TRUE, "set ON fixed value");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET LOCAL sql_warnings = OFF;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 0U), 0U);
    target = child_at(assignment, 0U);
    value = child_at(assignment, 1U);
    failures += expect_span_text(child_at(target, 0U), "LOCAL", "set target local scope");
    failures += expect_span_text(child_at(target, 1U), "sql_warnings", "set local name");
    failures += expect_literal(value, MYLITE_SQL_AST_LITERAL_FALSE, "set OFF fixed value");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET @@session.sql_mode = DEFAULT;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 0U), 0U);
    target = child_at(assignment, 0U);
    value = child_at(assignment, 1U);
    failures += expect_child_count(target, 1U, "set system variable target child count");
    failures += expect_node(
        child_at(target, 0U),
        MYLITE_SQL_AST_SYSTEM_VARIABLE,
        "set system variable token target"
    );
    failures += expect_span_text(child_at(target, 0U), "@@session.sql_mode", "set @@ target");
    failures += expect_node(value, MYLITE_SQL_AST_SET_DEFAULT_VALUE, "set default value");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SET sql_mode = "
        "'ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,"
        "ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 0U), 0U);
    failures += expect_literal(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "set sql_mode string value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET time_zone = SYSTEM;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 0U), 0U);
    value = child_at(assignment, 1U);
    failures += expect_node(value, MYLITE_SQL_AST_IDENTIFIER, "set time_zone SYSTEM value");
    failures += expect_span_text(value, "SYSTEM", "set time_zone SYSTEM span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET time_zone = UTC;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 0U), 0U);
    value = child_at(assignment, 1U);
    failures += expect_node(value, MYLITE_SQL_AST_IDENTIFIER, "set time_zone UTC value");
    failures += expect_span_text(value, "UTC", "set time_zone UTC span");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SET transaction_isolation = SERIALIZABLE;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 0U), 0U);
    value = child_at(assignment, 1U);
    failures += expect_node(
        value,
        MYLITE_SQL_AST_IDENTIFIER,
        "set transaction_isolation SERIALIZABLE value"
    );
    failures +=
        expect_span_text(value, "SERIALIZABLE", "set transaction_isolation SERIALIZABLE span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET time_zone = NULL;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 0U), 0U);
    value = child_at(assignment, 1U);
    failures += expect_literal(value, MYLITE_SQL_AST_LITERAL_NULL, "set time_zone NULL value");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET autocommit = 1, sql_notes = 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment_list = child_at(statement, 0U);
    failures += expect_child_count(assignment_list, 2U, "set assignment list supports commas");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET @name = 1, @other := @@sql_mode;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment_list = child_at(statement, 0U);
    assignment = child_at(assignment_list, 0U);
    failures += expect_child_count(assignment_list, 2U, "user variable assignment count");
    failures += expect_node(child_at(assignment, 0U), MYLITE_SQL_AST_USER_VARIABLE, "user target");
    failures += expect_span_text(child_at(assignment, 0U), "@name", "user target span");
    failures +=
        expect_literal(child_at(assignment, 1U), MYLITE_SQL_AST_LITERAL_INTEGER, "user value");
    assignment = child_at(assignment_list, 1U);
    failures +=
        expect_node(child_at(assignment, 0U), MYLITE_SQL_AST_USER_VARIABLE, "second user target");
    failures +=
        expect_node(child_at(assignment, 1U), MYLITE_SQL_AST_SYSTEM_VARIABLE, "user system value");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET @d = DEFAULT;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET app.autocommit = 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_sql_prepared_statement_lifecycle(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *using_list = NULL;
    int failures = 0;

    failures += parse_sql("PREPARE stmt FROM 'SELECT ?';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_PREPARE_STATEMENT, "prepare statement");
    failures += expect_child_count(statement, 2U, "prepare child count");
    failures += expect_span_text(child_at(statement, 0U), "stmt", "prepare name");
    failures +=
        expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING, "prepare source");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("PREPARE `MiXeD` FROM @sql;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(child_at(statement, 0U), "`MiXeD`", "quoted prepare name");
    failures +=
        expect_node(child_at(statement, 1U), MYLITE_SQL_AST_USER_VARIABLE, "prepare user source");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXECUTE stmt;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_EXECUTE_STATEMENT, "execute statement");
    failures += expect_child_count(statement, 1U, "execute child count");
    failures += expect_span_text(child_at(statement, 0U), "stmt", "execute name");
    failures += expect_true(child_at(statement, 1U) == NULL, "execute without using list");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXECUTE stmt USING @a, @`b-c`;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    using_list = child_at(statement, 1U);
    failures += expect_node(using_list, MYLITE_SQL_AST_EXECUTE_USING_LIST, "execute using list");
    failures += expect_child_count(using_list, 2U, "execute using count");
    failures += expect_span_text(child_at(using_list, 0U), "@a", "execute first variable");
    failures += expect_span_text(child_at(using_list, 1U), "@`b-c`", "execute second variable");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DEALLOCATE PREPARE stmt;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_DEALLOCATE_PREPARE_STATEMENT,
        "deallocate prepare statement"
    );
    failures += expect_child_count(statement, 1U, "deallocate child count");
    failures += expect_span_text(child_at(statement, 0U), "stmt", "deallocate name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP PREPARE stmt;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_DEALLOCATE_PREPARE_STATEMENT,
        "drop prepare statement"
    );
    failures += expect_span_text(statement, "DROP PREPARE stmt", "drop prepare span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ?;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("PREPARE stmt FROM SELECT;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("EXECUTE stmt USING 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("DEALLOCATE stmt;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_set_transaction_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *characteristics = NULL;
    int failures = 0;

    failures +=
        parse_sql("SET TRANSACTION ISOLATION LEVEL READ COMMITTED;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    characteristics = child_at(statement, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_SET_TRANSACTION_STATEMENT,
        "set transaction statement"
    );
    failures += expect_child_count(statement, 1U, "set transaction child count");
    failures += expect_span_text(
        statement,
        "SET TRANSACTION ISOLATION LEVEL READ COMMITTED",
        "set transaction span"
    );
    failures += expect_node(
        characteristics,
        MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST,
        "set transaction characteristic list"
    );
    failures += expect_child_count(characteristics, 1U, "set transaction isolation count");
    failures += expect_node(
        child_at(characteristics, 0U),
        MYLITE_SQL_AST_TRANSACTION_ISOLATION_READ_COMMITTED,
        "set transaction read committed"
    );
    failures +=
        expect_span_text(child_at(characteristics, 0U), "READ COMMITTED", "read committed span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET SESSION TRANSACTION READ ONLY;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    characteristics = child_at(statement, 1U);
    failures += expect_child_count(statement, 2U, "set session transaction child count");
    failures += expect_span_text(child_at(statement, 0U), "SESSION", "set transaction scope");
    failures += expect_child_count(characteristics, 1U, "set transaction access count");
    failures += expect_node(
        child_at(characteristics, 0U),
        MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY,
        "set transaction read only"
    );
    failures += expect_span_text(child_at(characteristics, 0U), "READ ONLY", "read only span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SET GLOBAL TRANSACTION READ WRITE, ISOLATION LEVEL SERIALIZABLE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    characteristics = child_at(statement, 1U);
    failures += expect_span_text(child_at(statement, 0U), "GLOBAL", "set global scope");
    failures += expect_child_count(characteristics, 2U, "set transaction mixed count");
    failures += expect_node(
        child_at(characteristics, 0U),
        MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_WRITE,
        "set transaction read write"
    );
    failures += expect_node(
        child_at(characteristics, 1U),
        MYLITE_SQL_AST_TRANSACTION_ISOLATION_SERIALIZABLE,
        "set transaction serializable"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "set session transaction isolation level repeatable read, read write;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    characteristics = child_at(statement, 1U);
    failures += expect_node(
        child_at(characteristics, 0U),
        MYLITE_SQL_AST_TRANSACTION_ISOLATION_REPEATABLE_READ,
        "set transaction repeatable read"
    );
    failures += expect_node(
        child_at(characteristics, 1U),
        MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_WRITE,
        "set transaction lower read write"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SET LOCAL TRANSACTION READ WRITE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SET TRANSACTION;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SET TRANSACTION READ WRITE, READ ONLY;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SET TRANSACTION ISOLATION LEVEL READ COMMITTED, ISOLATION LEVEL SERIALIZABLE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "SET TRANSACTION ISOLATION LEVEL READ-COMMITTED;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_update_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *assignment_list = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    const struct mylite_sql_ast_node *from_table = NULL;
    const struct mylite_sql_ast_node *hint_list = NULL;
    const struct mylite_sql_ast_node *hint = NULL;
    const struct mylite_sql_ast_node *name_list = NULL;
    int failures = 0;

    failures +=
        parse_sql("UPDATE app.simple_lifecycle SET amount = +1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment_list = child_at(statement, 1U);
    assignment = child_at(assignment_list, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_UPDATE_STATEMENT, "update statement");
    failures += expect_child_count(statement, 2U, "update statement required children");
    failures += expect_span_text(child_at(statement, 0U), "app.simple_lifecycle", "update target");
    failures += expect_node(
        assignment_list,
        MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST,
        "update assignment list"
    );
    failures += expect_child_count(assignment_list, 1U, "update single assignment");
    failures += expect_node(assignment, MYLITE_SQL_AST_UPDATE_ASSIGNMENT, "update assignment node");
    failures += expect_span_text(child_at(assignment, 0U), "amount", "update assignment target");
    failures += expect_operator(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "update positive assignment value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE app.simple_lifecycle FORCE KEY FOR ORDER BY (PRIMARY, k_amount) "
        "SET amount = 2 WHERE id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    from_table = child_at(statement, 0U);
    assignment_list = child_at(statement, 1U);
    where_clause = child_at(statement, 2U);
    hint_list = child_at(from_table, 1U);
    hint = child_at(hint_list, 0U);
    name_list = child_at(hint, 1U);
    failures += expect_node(from_table, MYLITE_SQL_AST_FROM_TABLE, "update hinted target");
    failures += expect_child_count(from_table, 2U, "update hinted target child count");
    failures +=
        expect_span_text(child_at(from_table, 0U), "app.simple_lifecycle", "update hinted table");
    failures += expect_node(hint_list, MYLITE_SQL_AST_INDEX_HINT_LIST, "update hint list");
    failures += expect_node(hint, MYLITE_SQL_AST_FORCE_INDEX_HINT, "update force key hint");
    failures += expect_node(
        child_at(hint, 0U),
        MYLITE_SQL_AST_INDEX_HINT_FOR_ORDER_BY,
        "update force order scope"
    );
    failures += expect_span_text(child_at(name_list, 0U), "PRIMARY", "update primary hint name");
    failures += expect_span_text(child_at(name_list, 1U), "k_amount", "update second hint name");
    failures += expect_node(
        assignment_list,
        MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST,
        "update hinted assignment list"
    );
    failures += expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "update hinted where");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE simple_lifecycle USE INDEX () SET amount = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    from_table = child_at(statement, 0U);
    hint = child_at(child_at(from_table, 1U), 0U);
    name_list = child_at(hint, 0U);
    failures += expect_child_count(name_list, 0U, "update empty use index names");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE simple_lifecycle FORCE INDEX () SET amount = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql(
        "UPDATE simple_lifecycle IGNORE INDEX () SET amount = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE counters SET n = n + 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment_list = child_at(statement, 1U);
    assignment = child_at(assignment_list, 0U);
    failures += expect_operator(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "update arithmetic add assignment"
    );
    failures +=
        expect_span_text(child_at(child_at(assignment, 1U), 0U), "n", "update arithmetic source");
    failures += expect_literal(
        child_at(child_at(assignment, 1U), 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "update arithmetic literal"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE counters SET n = n - 1;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment_list = child_at(statement, 1U);
    assignment = child_at(assignment_list, 0U);
    failures += expect_operator(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_OPERATOR_SUBTRACT,
        "update arithmetic subtract assignment"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE simple_lifecycle SET amount = NULL WHERE id = +1 ORDER BY nn DESC LIMIT 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    assignment_list = child_at(statement, 1U);
    assignment = child_at(assignment_list, 0U);
    where_clause = child_at(statement, 2U);
    order_clause = child_at(statement, 3U);
    limit_clause = child_at(statement, 4U);
    failures += expect_node(statement, MYLITE_SQL_AST_UPDATE_STATEMENT, "qualified update");
    failures += expect_literal(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "update NULL assignment value"
    );
    failures += expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "update where");
    failures += expect_operator(
        child_at(where_clause, 0U),
        MYLITE_SQL_AST_OPERATOR_EQUAL,
        "update where operator"
    );
    failures += expect_node(order_clause, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "update order");
    failures += expect_span_text(child_at(order_clause, 0U), "nn", "update order key");
    failures += expect_order_direction(
        child_at(order_clause, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "update order direction"
    );
    failures += expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "update limit");
    failures += expect_span_text(child_at(limit_clause, 0U), "2", "update limit row count");
    failures += expect_true(child_at(limit_clause, 1U) == NULL, "update limit has no offset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE simple_lifecycle SET simple_lifecycle.amount = -1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    assignment_list = child_at(statement, 1U);
    assignment = child_at(assignment_list, 0U);
    failures += expect_node(
        child_at(assignment, 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "update qualified assignment target"
    );
    failures += expect_operator(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "update negative assignment value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE simple_lifecycle SET amount = 1, nn = 2;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment_list = child_at(statement, 1U);
    failures += expect_child_count(assignment_list, 2U, "update multiple assignment list");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE simple_lifecycle SET amount = 1 LIMIT 0;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    limit_clause = first_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    failures += expect_node(limit_clause, MYLITE_SQL_AST_LIMIT_CLAUSE, "update simple limit");
    failures += expect_span_text(child_at(limit_clause, 0U), "0", "update simple limit row count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE simple_lifecycle SET amount = FALSE WHERE id = TRUE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 1U), 0U);
    where_clause = child_at(statement, 2U);
    failures += expect_literal(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "update false assignment value"
    );
    failures += expect_literal(
        child_at(child_at(where_clause, 0U), 1U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "update true predicate value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE simple_lifecycle SET amount = DEFAULT;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 1U), 0U);
    failures += expect_node(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_DML_DEFAULT_VALUE,
        "update default assignment value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE simple_lifecycle SET amount = 'text';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 1U), 0U);
    failures += expect_literal(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "update string assignment value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE target SET value = (SELECT source_value FROM source WHERE source_id = 1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    assignment = child_at(child_at(statement, 1U), 0U);
    failures += expect_node(
        child_at(assignment, 1U),
        MYLITE_SQL_AST_SCALAR_SUBQUERY,
        "update scalar subquery assignment value"
    );
    failures += expect_span_text(
        child_at(assignment, 1U),
        "(SELECT source_value FROM source WHERE source_id = 1)",
        "update scalar subquery assignment span"
    );
    failures += expect_node(
        child_at(child_at(assignment, 1U), 0U),
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "update scalar subquery inner select"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_transaction_control_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *characteristics = NULL;
    int failures = 0;

    failures += parse_sql("START TRANSACTION;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_START_TRANSACTION_STATEMENT,
        "start transaction statement"
    );
    failures += expect_child_count(statement, 0U, "start transaction children");
    failures += expect_span_text(statement, "START TRANSACTION", "start transaction span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("START TRANSACTION READ WRITE;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    characteristics = child_at(statement, 0U);
    failures += expect_child_count(statement, 1U, "start transaction read write children");
    failures += expect_span_text(
        statement,
        "START TRANSACTION READ WRITE",
        "start transaction read write span"
    );
    failures += expect_node(
        characteristics,
        MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST,
        "start transaction characteristic list"
    );
    failures += expect_child_count(characteristics, 1U, "start transaction read write count");
    failures += expect_node(
        child_at(characteristics, 0U),
        MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_WRITE,
        "start transaction read write"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "START TRANSACTION WITH CONSISTENT SNAPSHOT, READ ONLY;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    characteristics = child_at(statement, 0U);
    failures += expect_child_count(characteristics, 2U, "start transaction mixed count");
    failures += expect_node(
        child_at(characteristics, 0U),
        MYLITE_SQL_AST_TRANSACTION_CONSISTENT_SNAPSHOT,
        "start transaction consistent snapshot"
    );
    failures += expect_node(
        child_at(characteristics, 1U),
        MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY,
        "start transaction read only"
    );
    failures += expect_span_text(
        child_at(characteristics, 0U),
        "WITH CONSISTENT SNAPSHOT",
        "consistent snapshot span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "START TRANSACTION READ ONLY, READ ONLY, WITH CONSISTENT SNAPSHOT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    characteristics = child_at(statement, 0U);
    failures += expect_child_count(characteristics, 3U, "start transaction repeated count");
    failures += expect_node(
        child_at(characteristics, 0U),
        MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY,
        "start transaction first repeated read only"
    );
    failures += expect_node(
        child_at(characteristics, 1U),
        MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY,
        "start transaction second repeated read only"
    );
    failures += expect_node(
        child_at(characteristics, 2U),
        MYLITE_SQL_AST_TRANSACTION_CONSISTENT_SNAPSHOT,
        "start transaction repeated snapshot"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("BEGIN;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_START_TRANSACTION_STATEMENT, "begin");
    failures += expect_span_text(statement, "BEGIN", "begin span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("BEGIN IMMEDIATE;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_START_TRANSACTION_STATEMENT, "begin immediate");
    failures += expect_span_text(statement, "BEGIN IMMEDIATE", "begin immediate span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("begin immediate;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_START_TRANSACTION_STATEMENT, "lower begin immediate");
    failures += expect_span_text(statement, "begin immediate", "lower begin immediate span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("BEGIN WORK;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_START_TRANSACTION_STATEMENT, "begin work");
    failures += expect_span_text(statement, "BEGIN WORK", "begin work span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("COMMIT;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_COMMIT_STATEMENT, "commit");
    failures += expect_span_text(statement, "COMMIT", "commit span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("COMMIT WORK;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_COMMIT_STATEMENT, "commit work");
    failures += expect_span_text(statement, "COMMIT WORK", "commit work span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ROLLBACK;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_ROLLBACK_STATEMENT, "rollback");
    failures += expect_span_text(statement, "ROLLBACK", "rollback span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ROLLBACK WORK;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_ROLLBACK_STATEMENT, "rollback work");
    failures += expect_span_text(statement, "ROLLBACK WORK", "rollback work span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SAVEPOINT sp;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SAVEPOINT_STATEMENT, "savepoint");
    failures += expect_child_count(statement, 1U, "savepoint children");
    failures += expect_span_text(statement, "SAVEPOINT sp", "savepoint span");
    failures += expect_span_text(child_at(statement, 0U), "sp", "savepoint name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ROLLBACK TO sp;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT,
        "rollback to savepoint"
    );
    failures += expect_child_count(statement, 1U, "rollback to savepoint children");
    failures += expect_span_text(statement, "ROLLBACK TO sp", "rollback to savepoint span");
    failures += expect_span_text(child_at(statement, 0U), "sp", "rollback to savepoint name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ROLLBACK WORK TO SAVEPOINT `sp ace`;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT,
        "rollback work to savepoint"
    );
    failures += expect_span_text(
        statement,
        "ROLLBACK WORK TO SAVEPOINT `sp ace`",
        "rollback work to savepoint span"
    );
    failures += expect_span_text(child_at(statement, 0U), "`sp ace`", "quoted savepoint name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("RELEASE SAVEPOINT sp;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT, "release savepoint");
    failures += expect_child_count(statement, 1U, "release savepoint children");
    failures += expect_span_text(statement, "RELEASE SAVEPOINT sp", "release savepoint span");
    failures += expect_span_text(child_at(statement, 0U), "sp", "release savepoint name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE transaction ("
        "begin INT, immediate INT, commit INT, rollback INT, work INT, savepoint INT, "
        "isolation INT, level INT, committed INT, uncommitted INT, repeatable INT, "
        "serializable INT, only INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "transaction keywords as identifiers"
    );
    failures += expect_span_text(child_at(statement, 0U), "transaction", "transaction table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "START TRANSACTION ISOLATION LEVEL READ COMMITTED;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("BEGIN READ ONLY;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("BEGIN DEFERRED;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("BEGIN EXCLUSIVE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("BEGIN TRANSACTION;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("BEGIN IMMEDIATE TRANSACTION;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("END TRANSACTION;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("COMMIT AND CHAIN;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("COMMIT TRANSACTION;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("ROLLBACK RELEASE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("ROLLBACK TRANSACTION;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("ROLLBACK TO 'sp';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("RELEASE sp;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_table_lock_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *targets = NULL;
    const struct mylite_sql_ast_node *target = NULL;
    int failures = 0;

    failures += parse_sql("LOCK TABLES t READ;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    targets = child_at(statement, 0U);
    target = child_at(targets, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_LOCK_TABLES_STATEMENT, "lock read");
    failures += expect_span_text(statement, "LOCK TABLES t READ", "lock read span");
    failures += expect_node(targets, MYLITE_SQL_AST_LOCK_TABLE_TARGET_LIST, "lock target list");
    failures += expect_child_count(targets, 1U, "lock read target count");
    failures += expect_node(target, MYLITE_SQL_AST_LOCK_TABLE_TARGET, "lock read target");
    failures += expect_child_count(target, 2U, "lock read target children");
    failures += expect_span_text(child_at(target, 0U), "t", "lock read table");
    failures +=
        expect_node(child_at(target, 1U), MYLITE_SQL_AST_LOCK_TABLE_READ_LOCK, "lock read mode");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("LOCK TABLE t WRITE;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    targets = child_at(statement, 0U);
    target = child_at(targets, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_LOCK_TABLES_STATEMENT, "lock write");
    failures += expect_span_text(statement, "LOCK TABLE t WRITE", "lock write span");
    failures += expect_child_count(target, 2U, "lock write target children");
    failures +=
        expect_node(child_at(target, 1U), MYLITE_SQL_AST_LOCK_TABLE_WRITE_LOCK, "lock write mode");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "LOCK TABLES app.t AS reader READ LOCAL, u writer WRITE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    targets = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_LOCK_TABLES_STATEMENT, "multi lock");
    failures += expect_child_count(targets, 2U, "multi lock target count");
    target = child_at(targets, 0U);
    failures += expect_child_count(target, 3U, "first multi lock target children");
    failures += expect_span_text(child_at(target, 0U), "app.t", "first multi lock table");
    failures += expect_span_text(child_at(target, 1U), "reader", "first multi lock alias");
    failures += expect_node(
        child_at(target, 2U),
        MYLITE_SQL_AST_LOCK_TABLE_READ_LOCAL_LOCK,
        "first multi lock mode"
    );
    target = child_at(targets, 1U);
    failures += expect_child_count(target, 3U, "second multi lock target children");
    failures += expect_span_text(child_at(target, 0U), "u", "second multi lock table");
    failures += expect_span_text(child_at(target, 1U), "writer", "second multi lock alias");
    failures += expect_node(
        child_at(target, 2U),
        MYLITE_SQL_AST_LOCK_TABLE_WRITE_LOCK,
        "second multi lock mode"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UNLOCK TABLES;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_UNLOCK_TABLES_STATEMENT, "unlock tables");
    failures += expect_child_count(statement, 0U, "unlock tables children");
    failures += expect_span_text(statement, "UNLOCK TABLES", "unlock tables span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UNLOCK TABLE;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_UNLOCK_TABLES_STATEMENT, "unlock table");
    failures += expect_span_text(statement, "UNLOCK TABLE", "unlock table span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("LOCK TABLES t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("LOCK TABLES t READ LOCAL LOCAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("LOCK TABLES t LOW_PRIORITY WRITE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("UNLOCK;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("LOCK INSTANCE FOR BACKUP;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_comments_are_skipped(void) {
    struct mylite_sql_parse_result result;
    int failures = 0;

    failures +=
        parse_sql("/* regular */ SELECT -- line\n1 /*+ hint */;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 1U, "comment root");
    failures +=
        expect_node(child_at(result.root, 0U), MYLITE_SQL_AST_SELECT_STATEMENT, "comment select");

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_syntax_errors(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SELECT FROM DUAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    if (result.error_token.text == NULL || result.error_token.length != strlen("FROM") ||
        memcmp(result.error_token.text, "FROM", strlen("FROM")) != 0) {
        fprintf(stderr, "expected syntax error token FROM\n");
        failures = 1;
    }
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1 +;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT INTERVAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT INTEGER;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1 WHERE TRUE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1, * FROM DUAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DATABASE(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SCHEMA(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT USER(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CURRENT_USER(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SESSION_USER(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SYSTEM_USER(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SESSION_USER ();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SYSTEM_USER ();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SESSION_USER/**/();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SYSTEM_USER/**/();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DATABASE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SCHEMA;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DATABASE() LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT USER() LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CURRENT_USER LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT MOD();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT MOD(5);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT MOD(5,2,1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DIV(5,2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 5 DIV;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DIV 2;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1=;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT =1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1<=>;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1 AND;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT AND 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1 OR;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1 XOR;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT NOT;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1&&1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1||0;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE DATABASE IF EXISTS app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE DATABASE app DEFAULT ENCRYPTION='N';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET NAMES;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET NAMES NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET NAMES utf8mb4, latin1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SET CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SET character_set_client = utf8mb4, character_set_results = utf8mb4;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET NAMES ?;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SET NAMES CONCAT('utf8', 'mb4');", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE DATABASE a.b;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("DROP DATABASE IF NOT EXISTS app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW DATABASES LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW DATABASES LIKE NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW DATABASES LIKE N'app%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW DATABASES WHERE Database = 'app';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL DATABASES;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL TABLE STATUS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EXTENDED TABLE STATUS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLE STATUS FULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW TABLE STATUS LIKE 'a%' WHERE Name = 't';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW TABLE STATUS WHERE Name = 't' ORDER BY Name;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLE STATUS LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLE STATUS LIKE NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLE STATUS LIKE N't%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW TABLE STATUS LIKE 't%' FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CHARACTER SETS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CHARSETS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLLATIONS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW CHARACTER SET WHERE Charset = 'utf8mb4';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CHARACTER SET LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CHARACTER SET LIKE NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW CHARACTER SET LIKE N'utf8mb4';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW CHARACTER SET LIKE _utf8mb4'utf8mb4';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW COLLATION WHERE Collation = 'utf8mb4_0900_ai_ci';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLLATION LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLLATION LIKE NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW COLLATION LIKE N'utf8mb4_0900_ai_ci';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW COLLATION LIKE _utf8mb4'utf8mb4_0900_ai_ci';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE DATABASE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW CREATE DATABASE IF NOT EXISTS app;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW CREATE DATABASE IF EXISTS app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW CREATE DATABASE app LIKE 'a%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW CREATE DATABASE app DEFAULT CHARACTER SET utf8mb4;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE DATABASE app.db;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ENGINES LIKE 'InnoDB';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW ENGINES WHERE Engine = 'InnoDB';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL ENGINES;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ENGINE InnoDB STATUS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW EXTENDED FULL COLUMNS FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE TABLE t LIKE 't';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW CREATE TABLE t WHERE Table = 't';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE TABLE t FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE TEMPORARY TABLE t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL CREATE TABLE t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EXTENDED COLUMNS FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLUMNS FROM t LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SHOW COLUMNS FROM t LIKE 'i%' WHERE Field = 'id';",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DESCRIBE t id;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DESCRIBE SELECT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN t id;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN t 'i%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN t FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN SELECT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN FORMAT=JSON SELECT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN ANALYZE SELECT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN FOR CONNECTION 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN EXTENDED SELECT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN PARTITIONS SELECT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE t ();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE a.b.c (id INT);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE t (id VARCHAR);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE t (id INT DEFAULT (NULL));", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(
        child_at(
            first_child_kind(
                child_at(child_at(child_at(result.root, 0U), 1U), 0U),
                MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE
            ),
            0U
        ),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "parenthesized NULL default"
    );
    failures += expect_literal(
        child_at(
            child_at(
                first_child_kind(
                    child_at(child_at(child_at(result.root, 0U), 1U), 0U),
                    MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE
                ),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_NULL,
        "parenthesized NULL default literal"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE t (id INT DEFAULT (1 + 2));", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_operator(
        child_at(
            child_at(
                first_child_kind(
                    child_at(child_at(child_at(result.root, 0U), 1U), 0U),
                    MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE
                ),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "parenthesized expression default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE t (id INT DEFAULT '5');", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_literal(
        child_at(
            first_child_kind(
                child_at(child_at(child_at(result.root, 0U), 1U), 0U),
                MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE
            ),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_STRING,
        "string integer default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE t (id INT DEFAULT 1.5);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE t (id INT DEFAULT 0x1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE t (id INT DEFAULT 1 + 2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE t (id INT DEFAULT NULL NULL);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE t (id INT DEFAULT NULL DEFAULT NULL);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE t (id INT DEFAULT NULL NOT NULL);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE t (id INT(+1));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE t (id INT) ENGINE=DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE t (id INT) DEFAULT CHARSET=DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE t (id INT) COLLATE=DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE t (id INT) DEFAULT CHARACTER SET;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE t (id INT) ENGINE=InnoDB, DEFAULT CHARSET=utf8mb4;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE t (id INT) COMMENT=123;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE t (id INT) COMMENT=abc;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE t (id INT) COMMENT=NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE IF EXISTS t (id INT);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE IF NOT t (id INT);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP TABLE IF NOT EXISTS t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("TRUNCATE TABLE IF EXISTS t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("TRUNCATE TABLE a, b;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("TRUNCATE TABLE t WHERE id = 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("TRUNCATE TABLE t LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("TRUNCATE TEMPORARY TABLE t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("RENAME TABLE old_name new_name;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("RENAME TABLE old_name TO new_name,;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME TABLE new_name;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME new_name, ADD COLUMN added INT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME new_name, RENAME final_name;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE old_name ADD (added INT);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ADD COLUMN added INT AFTER old_name.id;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ADD COLUMN first_added INT, ADD COLUMN second_added INT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ADD COLUMN old_name.added INT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ADD COLUMN added VARCHAR;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_signed_unsigned (c INT SIGNED UNSIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_unsigned_signed (c INT UNSIGNED SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_repeated_signed (c INT SIGNED SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_repeated_unsigned (c INT UNSIGNED UNSIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_alias_zerofill (c INT2 ZEROFILL);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_alias_signed_unsigned (c INT1 SIGNED UNSIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_alias_unsigned_signed (c INT1 UNSIGNED SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_alias_repeated_signed (c INT1 SIGNED SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_alias_repeated_unsigned (c INT1 UNSIGNED UNSIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_decimal_signed (c DECIMAL SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_width_minus (c INT(-1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_width_empty (c INT());",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_width_decimal (c INT(1.0));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_width_string (c INT('1'));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_width_hex (c INT(0x1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_width_bit (c INT(b'1'));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_width_parameter (c INT(?));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_width_expression (c INT(1+1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_width_after_unsigned (c INT UNSIGNED(1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_alias_width_signed (c INT1(+1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_zerofill (c MEDIUMINT ZEROFILL);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_int5 (c INT5);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_bool_width (c BOOL(1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_boolean_width (c BOOLEAN(1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_bool_signed (c BOOL SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_bool_unsigned (c BOOL UNSIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_bool_zerofill (c BOOL ZEROFILL);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "CREATE TABLE unsupported_bool_signed_unsigned (c BOOL SIGNED UNSIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name DROP COLUMN old_name.added;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name DROP COLUMN added, DROP COLUMN other;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE old_name DROP INDEX idx;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE old_name DROP FOREIGN KEY fk;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name DROP COLUMN IF EXISTS added;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE old_name DROP (added);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name DROP COLUMN added FIRST;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name DROP COLUMN added, ALGORITHM=INSTANT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT,
        "alter drop column algorithm option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name DROP COLUMN added, LOCK=DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_DEFAULT,
        "alter drop column lock option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME old_col TO new_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_name.old_col TO new_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_col TO old_name.new_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_col new_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_col TO new_col FIRST;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_col TO new_col, RENAME COLUMN other TO final;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_col TO new_col, ALGORITHM=INSTANT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT,
        "alter rename column algorithm option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name RENAME COLUMN old_col TO new_col, LOCK=DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = child_at(result.root, 0U);
    failures += expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_DEFAULT,
        "alter rename column lock option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name MODIFY old_name.old_col BIGINT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name MODIFY old_col BIGINT AFTER old_name.other_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name MODIFY old_col BIGINT, MODIFY other_col BIGINT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name MODIFY old_col VARCHAR;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name MODIFY old_col BIGINT, ALGORITHM=INSTANT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name MODIFY old_col BIGINT, LOCK=DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name CHANGE old_name.old_col new_col BIGINT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name CHANGE old_col old_name.new_col BIGINT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name CHANGE old_col new_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name CHANGE old_col new_col BIGINT AFTER old_name.other_col;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name CHANGE old_col new_col BIGINT, CHANGE other_col final_col BIGINT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name CHANGE old_col new_col VARCHAR;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name CHANGE old_col new_col BIGINT, ALGORITHM=INSTANT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name CHANGE old_col new_col BIGINT, LOCK=DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ALTER COLUMN old_name.old_col SET DEFAULT 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ALTER old_col SET DEFAULT (1 + 1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_operator(
        child_at(child_at(child_at(child_at(result.root, 0U), 2U), 0U), 0U),
        MYLITE_SQL_AST_OPERATOR_ADD,
        "alter set parenthesized expression default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ALTER old_col SET DEFAULT '1';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    failures += expect_literal(
        child_at(child_at(child_at(result.root, 0U), 2U), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "alter set string default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ALTER COLUMN old_name.old_col DROP DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ALTER old_col DROP DEFAULT, ALTER other DROP DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ALTER COLUMN old_name.old_col SET INVISIBLE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ALTER old_col SET INVISIBLE, ALTER other SET VISIBLE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ALTER old_col SET HIDDEN;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE t (id INT INVISIBLE);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE t ADD COLUMN v INT INVISIBLE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "ALTER TABLE old_name ALTER old_col SET DEFAULT 1, ALTER other SET DEFAULT 2;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES (1.5);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES (1 + 2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES (+TRUE);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t SET id = 1 + 2;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUE ROW();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT INTO t VALUES ROW(1), (2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT INTO t VALUES (1), ROW(2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t VALUES (1.5);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t VALUES (1 + 2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t VALUE ROW(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("REPLACE INTO t VALUES ROW(1), (2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("REPLACE INTO t VALUES (1), ROW(2);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "REPLACE LOW_PRIORITY DELAYED INTO t VALUES (1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT LOW_PRIORITY HIGH_PRIORITY INTO t VALUES (1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT HIGH_PRIORITY LOW_PRIORITY INTO t VALUES (1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT LOW_PRIORITY DELAYED INTO t VALUES (1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT IGNORE LOW_PRIORITY INTO t VALUES (1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "INSERT IGNORE LOW_PRIORITY INTO t SELECT id FROM src;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT INTO LOW_PRIORITY t VALUES (1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "REPLACE HIGH_PRIORITY INTO t VALUES (1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "REPLACE INTO LOW_PRIORITY t VALUES (1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t TABLE source;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "REPLACE INTO t PARTITION (p0) VALUES (1);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("REPLACE INTO t (app.id) VALUES (1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t VALUES (?);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("REPLACE INTO t VALUES (ABS(1));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("REPLACE INTO t VALUES ((SELECT 1));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t VALUES (1.5);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t VALUES (1e0);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t VALUES (0x1);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t VALUES (b'1');", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t VALUES (DEFAULT(id));", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t SET id = 1.5;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t SET id = 1 + 2;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t SET id = DEFAULT(id);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t SET id = ?;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("REPLACE INTO t SET id = ABS(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("REPLACE INTO t SET id = (SELECT 1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t SET id = 1.5;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t SET id = 1e0;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t SET id = 0x1;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t SET id = b'1';", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "REPLACE DELAYED LOW_PRIORITY INTO t SET id = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "REPLACE HIGH_PRIORITY INTO t SET id = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM t WHERE id + 1 = 2;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM t WHERE id = '1';", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT DEFAULT(id), DEFAULT(t.id) FROM t;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DEFAULT(1) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM t WHERE id XOR nn = 2;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM t WHERE ! (id = 1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM t WHERE ABS(id) = 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM t WHERE id = b'1';", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM t JOIN other USING (id);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM t WHERE id = 1 ORDER BY nn, id;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT id FROM t WHERE id = 1 LIMIT +1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM t LIMIT TRUE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t LIMIT +1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t LIMIT FALSE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("DELETE FROM t LIMIT 1 OFFSET 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t LIMIT 1, 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t ORDER BY id, nn;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE LOW_PRIORITY FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET id = 1.5;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET id = 1 + 2;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET id = -FALSE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET id = TRUE + 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET id = other;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET id = DEFAULT(id);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET id = 1 LIMIT +1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE t SET id = 1 LIMIT FALSE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE t SET id = 1 LIMIT 1 OFFSET 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE t SET id = 1 LIMIT 1, 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "UPDATE t SET id = 1 ORDER BY id, nn LIMIT 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE LOW_PRIORITY t SET id = 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t AS x SET id = 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE t PARTITION (p0) SET id = 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "WITH c AS (SELECT id FROM t) UPDATE t SET id = 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_lexer_errors(void) {
    struct mylite_sql_parse_result result;
    int failures = 0;

    failures += parse_sql("SELECT 'unterminated", MYLITE_SQL_PARSE_LEXER_ERROR, &result);
    if (result.error_token.error != MYLITE_SQL_LEXER_ERROR_UNTERMINATED_STRING) {
        fprintf(
            stderr,
            "expected unterminated string lexer error, got %s\n",
            mylite_sql_lexer_error_name(result.error_token.error)
        );
        failures = 1;
    }

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int parse_sql(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    struct mylite_sql_parse_result *out_result
) {
    enum mylite_sql_parse_status actual = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = strlen(sql),
            .modes = 0U,
        },
        out_result
    );

    if (actual != expected_status) {
        fprintf(
            stderr,
            "parse '%s': expected %s, got %s\n",
            sql,
            mylite_sql_parse_status_name(expected_status),
            mylite_sql_parse_status_name(actual)
        );
        return 1;
    }

    return 0;
}

static int parse_sql_with_ignore_space(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    struct mylite_sql_parse_result *out_result
) {
    enum mylite_sql_parse_status actual = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = strlen(sql),
            .modes = MYLITE_SQL_MODE_IGNORE_SPACE,
        },
        out_result
    );

    if (actual != expected_status) {
        fprintf(
            stderr,
            "parse '%s': expected %s, got %s\n",
            sql,
            mylite_sql_parse_status_name(expected_status),
            mylite_sql_parse_status_name(actual)
        );
        return 1;
    }

    return 0;
}

static const struct mylite_sql_ast_node *child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }

    child = node->first_child;
    for (size_t current = 0U; current < index && child != NULL; ++current) {
        child = child->next_sibling;
    }
    return child;
}

static const struct mylite_sql_ast_node *first_child_kind(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }

    child = node->first_child;
    while (child != NULL) {
        if (child->kind == kind) {
            return child;
        }
        child = child->next_sibling;
    }

    return NULL;
}

static int expect_node(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind expected_kind,
    const char *context
) {
    if (node == NULL) {
        fprintf(
            stderr,
            "%s: expected %s, got null\n",
            context,
            mylite_sql_ast_node_kind_name(expected_kind)
        );
        return 1;
    }

    if (node->kind != expected_kind) {
        fprintf(
            stderr,
            "%s: expected %s, got %s\n",
            context,
            mylite_sql_ast_node_kind_name(expected_kind),
            mylite_sql_ast_node_kind_name(node->kind)
        );
        return 1;
    }

    return 0;
}

static int expect_child_count(
    const struct mylite_sql_ast_node *node,
    size_t expected,
    const char *context
) {
    size_t actual = mylite_sql_ast_node_child_count(node);

    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu children, got %zu\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_span_text(
    const struct mylite_sql_ast_node *node,
    const char *expected,
    const char *context
) {
    size_t expected_length = strlen(expected);

    if (node == NULL) {
        fprintf(stderr, "%s: expected span '%s', got null node\n", context, expected);
        return 1;
    }

    if (node->span.length != expected_length ||
        (expected_length > 0U && memcmp(node->span.text, expected, expected_length) != 0)) {
        fprintf(
            stderr,
            "%s: expected span '%s', got '%.*s'\n",
            context,
            expected,
            (int)node->span.length,
            node->span.text == NULL ? "" : node->span.text
        );
        return 1;
    }

    return 0;
}

static int expect_literal(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_literal_kind expected,
    const char *context
) {
    int failures = expect_node(node, MYLITE_SQL_AST_LITERAL, context);

    if (node != NULL && mylite_sql_ast_node_literal_kind(node) != expected) {
        fprintf(
            stderr,
            "%s: expected literal %s, got %s\n",
            context,
            mylite_sql_ast_literal_kind_name(expected),
            mylite_sql_ast_literal_kind_name(mylite_sql_ast_node_literal_kind(node))
        );
        failures = 1;
    }

    return failures;
}

static int expect_operator(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_operator expected,
    const char *context
) {
    if (node == NULL) {
        fprintf(
            stderr,
            "%s: expected operator %s, got null node\n",
            context,
            mylite_sql_ast_operator_name(expected)
        );
        return 1;
    }

    if (mylite_sql_ast_node_operator(node) != expected) {
        fprintf(
            stderr,
            "%s: expected operator %s, got %s\n",
            context,
            mylite_sql_ast_operator_name(expected),
            mylite_sql_ast_operator_name(mylite_sql_ast_node_operator(node))
        );
        return 1;
    }

    return 0;
}

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

static int expect_integer_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_integer_type expected_type,
    int expected_unsigned,
    const char *context
) {
    enum mylite_sql_ast_integer_type actual_type = mylite_sql_ast_node_integer_type(node);
    int actual_unsigned = mylite_sql_ast_node_integer_type_is_unsigned(node);

    if (actual_type != expected_type || actual_unsigned != expected_unsigned) {
        fprintf(
            stderr,
            "%s: expected %s unsigned=%d, got %s unsigned=%d\n",
            context,
            mylite_sql_ast_integer_type_name(expected_type),
            expected_unsigned,
            mylite_sql_ast_integer_type_name(actual_type),
            actual_unsigned
        );
        return 1;
    }

    return 0;
}

static int expect_varchar_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    const char *context
) {
    struct mylite_sql_source_span span = {0};

    if (expect_node(node, MYLITE_SQL_AST_VARCHAR_TYPE, context) != 0) {
        return 1;
    }

    span = mylite_sql_ast_node_varchar_type_length_span(node);
    if (span.text == NULL || span.length != strlen(expected_length) ||
        strncmp(span.text, expected_length, span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected VARCHAR length %s, got %.*s\n",
            context,
            expected_length,
            (int)span.length,
            span.text == NULL ? "" : span.text
        );
        return 1;
    }
    if (mylite_sql_ast_node_varchar_type_is_national(node) != 0) {
        fprintf(stderr, "%s: expected ordinary VARCHAR type, got national type\n", context);
        return 1;
    }

    return 0;
}

static int expect_national_varchar_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    const char *context
) {
    struct mylite_sql_source_span span = {0};

    if (expect_node(node, MYLITE_SQL_AST_VARCHAR_TYPE, context) != 0) {
        return 1;
    }

    span = mylite_sql_ast_node_varchar_type_length_span(node);
    if (span.text == NULL || span.length != strlen(expected_length) ||
        strncmp(span.text, expected_length, span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected national VARCHAR length %s, got %.*s\n",
            context,
            expected_length,
            (int)span.length,
            span.text == NULL ? "" : span.text
        );
        return 1;
    }
    if (mylite_sql_ast_node_varchar_type_is_national(node) == 0) {
        fprintf(stderr, "%s: expected national VARCHAR type\n", context);
        return 1;
    }

    return 0;
}

static int expect_char_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    int expected_explicit_length,
    const char *context
) {
    struct mylite_sql_source_span span = {0};
    int has_explicit_length = 0;

    if (expect_node(node, MYLITE_SQL_AST_CHAR_TYPE, context) != 0) {
        return 1;
    }

    has_explicit_length = mylite_sql_ast_node_char_type_has_explicit_length(node);
    if (has_explicit_length != expected_explicit_length) {
        fprintf(
            stderr,
            "%s: expected explicit CHAR length %d, got %d\n",
            context,
            expected_explicit_length,
            has_explicit_length
        );
        return 1;
    }
    if (mylite_sql_ast_node_char_type_is_national(node) != 0) {
        fprintf(stderr, "%s: expected ordinary CHAR type, got national type\n", context);
        return 1;
    }
    if (expected_length == NULL) {
        return 0;
    }

    span = mylite_sql_ast_node_char_type_length_span(node);
    if (span.text == NULL || span.length != strlen(expected_length) ||
        strncmp(span.text, expected_length, span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected CHAR length %s, got %.*s\n",
            context,
            expected_length,
            (int)span.length,
            span.text == NULL ? "" : span.text
        );
        return 1;
    }

    return 0;
}

static int expect_national_char_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    int expected_explicit_length,
    const char *context
) {
    struct mylite_sql_source_span span = {0};
    int has_explicit_length = 0;

    if (expect_node(node, MYLITE_SQL_AST_CHAR_TYPE, context) != 0) {
        return 1;
    }

    has_explicit_length = mylite_sql_ast_node_char_type_has_explicit_length(node);
    if (has_explicit_length != expected_explicit_length) {
        fprintf(
            stderr,
            "%s: expected explicit national CHAR length %d, got %d\n",
            context,
            expected_explicit_length,
            has_explicit_length
        );
        return 1;
    }
    if (mylite_sql_ast_node_char_type_is_national(node) == 0) {
        fprintf(stderr, "%s: expected national CHAR type\n", context);
        return 1;
    }
    if (expected_length == NULL) {
        return 0;
    }

    span = mylite_sql_ast_node_char_type_length_span(node);
    if (span.text == NULL || span.length != strlen(expected_length) ||
        strncmp(span.text, expected_length, span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected national CHAR length %s, got %.*s\n",
            context,
            expected_length,
            (int)span.length,
            span.text == NULL ? "" : span.text
        );
        return 1;
    }

    return 0;
}

static int expect_text_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_text_type expected,
    const char *context
) {
    enum mylite_sql_ast_text_type actual = MYLITE_SQL_AST_TEXT_TYPE_NONE;

    if (expect_node(node, MYLITE_SQL_AST_TEXT_TYPE, context) != 0) {
        return 1;
    }

    actual = mylite_sql_ast_node_text_type(node);
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected text type %s, got %s\n",
            context,
            mylite_sql_ast_text_type_name(expected),
            mylite_sql_ast_text_type_name(actual)
        );
        return 1;
    }

    return 0;
}

static int expect_binary_string_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_binary_string_type expected,
    const char *context
) {
    enum mylite_sql_ast_binary_string_type actual = MYLITE_SQL_AST_BINARY_STRING_TYPE_NONE;

    if (expect_node(node, MYLITE_SQL_AST_BINARY_STRING_TYPE, context) != 0) {
        return 1;
    }

    actual = mylite_sql_ast_node_binary_string_type(node);
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected binary string type %s, got %s\n",
            context,
            mylite_sql_ast_binary_string_type_name(expected),
            mylite_sql_ast_binary_string_type_name(actual)
        );
        return 1;
    }

    return 0;
}

static int expect_bit_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    int expected_explicit_length,
    const char *context
) {
    struct mylite_sql_source_span span = {0};
    int has_explicit_length = 0;

    if (expect_node(node, MYLITE_SQL_AST_BIT_TYPE, context) != 0) {
        return 1;
    }

    has_explicit_length = mylite_sql_ast_node_bit_type_has_length(node);
    if (has_explicit_length != expected_explicit_length) {
        fprintf(
            stderr,
            "%s: expected explicit BIT length %d, got %d\n",
            context,
            expected_explicit_length,
            has_explicit_length
        );
        return 1;
    }
    if (expected_length == NULL) {
        return 0;
    }

    span = mylite_sql_ast_node_bit_type_length_span(node);
    if (span.text == NULL || span.length != strlen(expected_length) ||
        strncmp(span.text, expected_length, span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected BIT length %s, got %.*s\n",
            context,
            expected_length,
            (int)span.length,
            span.text == NULL ? "" : span.text
        );
        return 1;
    }

    return 0;
}

static int expect_year_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_width,
    int expected_explicit_width,
    const char *context
) {
    struct mylite_sql_source_span span = {0};
    int has_explicit_width = 0;

    if (expect_node(node, MYLITE_SQL_AST_YEAR_TYPE, context) != 0) {
        return 1;
    }

    has_explicit_width = mylite_sql_ast_node_year_type_has_width(node);
    if (has_explicit_width != expected_explicit_width) {
        fprintf(
            stderr,
            "%s: expected explicit YEAR width %d, got %d\n",
            context,
            expected_explicit_width,
            has_explicit_width
        );
        return 1;
    }
    if (expected_width == NULL) {
        return 0;
    }

    span = mylite_sql_ast_node_year_type_width_span(node);
    if (span.text == NULL || span.length != strlen(expected_width) ||
        strncmp(span.text, expected_width, span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected YEAR width %s, got %.*s\n",
            context,
            expected_width,
            (int)span.length,
            span.text == NULL ? "" : span.text
        );
        return 1;
    }

    return 0;
}

static int expect_decimal_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_decimal_type expected,
    const char *expected_precision,
    const char *expected_scale,
    int expected_unsigned,
    const char *context
) {
    struct mylite_sql_source_span precision_span = {0};
    struct mylite_sql_source_span scale_span = {0};
    enum mylite_sql_ast_decimal_type actual = MYLITE_SQL_AST_DECIMAL_TYPE_DECIMAL;
    int actual_unsigned = 0;

    if (expect_node(node, MYLITE_SQL_AST_DECIMAL_TYPE, context) != 0) {
        return 1;
    }

    actual = mylite_sql_ast_node_decimal_type(node);
    actual_unsigned = mylite_sql_ast_node_decimal_type_is_unsigned(node);
    if (actual != expected || actual_unsigned != expected_unsigned) {
        fprintf(
            stderr,
            "%s: expected decimal type %s unsigned=%d, got %s unsigned=%d\n",
            context,
            mylite_sql_ast_decimal_type_name(expected),
            expected_unsigned,
            mylite_sql_ast_decimal_type_name(actual),
            actual_unsigned
        );
        return 1;
    }

    if (expected_precision == NULL) {
        if (mylite_sql_ast_node_decimal_type_has_precision(node) != 0) {
            fprintf(stderr, "%s: expected no decimal precision\n", context);
            return 1;
        }
        return 0;
    }
    if (mylite_sql_ast_node_decimal_type_has_precision(node) == 0) {
        fprintf(stderr, "%s: expected decimal precision %s\n", context, expected_precision);
        return 1;
    }
    precision_span = mylite_sql_ast_node_decimal_type_precision_span(node);
    if (precision_span.text == NULL || precision_span.length != strlen(expected_precision) ||
        strncmp(precision_span.text, expected_precision, precision_span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected decimal precision %s, got %.*s\n",
            context,
            expected_precision,
            (int)precision_span.length,
            precision_span.text == NULL ? "" : precision_span.text
        );
        return 1;
    }

    if (expected_scale == NULL) {
        if (mylite_sql_ast_node_decimal_type_has_scale(node) != 0) {
            fprintf(stderr, "%s: expected no decimal scale\n", context);
            return 1;
        }
        return 0;
    }
    if (mylite_sql_ast_node_decimal_type_has_scale(node) == 0) {
        fprintf(stderr, "%s: expected decimal scale %s\n", context, expected_scale);
        return 1;
    }
    scale_span = mylite_sql_ast_node_decimal_type_scale_span(node);
    if (scale_span.text == NULL || scale_span.length != strlen(expected_scale) ||
        strncmp(scale_span.text, expected_scale, scale_span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected decimal scale %s, got %.*s\n",
            context,
            expected_scale,
            (int)scale_span.length,
            scale_span.text == NULL ? "" : scale_span.text
        );
        return 1;
    }

    return 0;
}

static int expect_approximate_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_approximate_type expected,
    const char *expected_precision,
    int expected_unsigned,
    const char *context
) {
    struct mylite_sql_source_span precision_span = {0};
    enum mylite_sql_ast_approximate_type actual = MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT;
    int actual_unsigned = 0;

    if (expect_node(node, MYLITE_SQL_AST_APPROXIMATE_TYPE, context) != 0) {
        return 1;
    }

    actual = mylite_sql_ast_node_approximate_type(node);
    actual_unsigned = mylite_sql_ast_node_approximate_type_is_unsigned(node);
    if (actual != expected || actual_unsigned != expected_unsigned) {
        fprintf(
            stderr,
            "%s: expected approximate type %s unsigned=%d, got %s unsigned=%d\n",
            context,
            mylite_sql_ast_approximate_type_name(expected),
            expected_unsigned,
            mylite_sql_ast_approximate_type_name(actual),
            actual_unsigned
        );
        return 1;
    }

    if (expected_precision == NULL) {
        if (mylite_sql_ast_node_approximate_type_has_precision(node) != 0) {
            fprintf(stderr, "%s: expected no approximate precision\n", context);
            return 1;
        }
        return 0;
    }
    if (mylite_sql_ast_node_approximate_type_has_precision(node) == 0) {
        fprintf(stderr, "%s: expected approximate precision %s\n", context, expected_precision);
        return 1;
    }
    precision_span = mylite_sql_ast_node_approximate_type_precision_span(node);
    if (precision_span.text == NULL || precision_span.length != strlen(expected_precision) ||
        strncmp(precision_span.text, expected_precision, precision_span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected approximate precision %s, got %.*s\n",
            context,
            expected_precision,
            (int)precision_span.length,
            precision_span.text == NULL ? "" : precision_span.text
        );
        return 1;
    }

    return 0;
}

static int expect_integer_display_width(
    const struct mylite_sql_ast_node *node,
    const char *expected_width,
    const char *context
) {
    int has_width = mylite_sql_ast_node_integer_type_has_display_width(node);
    struct mylite_sql_source_span span = mylite_sql_ast_node_integer_type_display_width_span(node);

    if (expected_width == NULL) {
        if (has_width) {
            fprintf(stderr, "%s: expected no display width\n", context);
            return 1;
        }
        return 0;
    }

    if (!has_width) {
        fprintf(
            stderr,
            "%s: expected display width %s, got no display width\n",
            context,
            expected_width
        );
        return 1;
    }
    if (span.text == NULL || span.length != strlen(expected_width) ||
        strncmp(span.text, expected_width, span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected display width %s, got %.*s\n",
            context,
            expected_width,
            (int)span.length,
            span.text == NULL ? "" : span.text
        );
        return 1;
    }

    return 0;
}

static int expect_integer_bool_alias(const struct mylite_sql_ast_node *node, const char *context) {
    if (mylite_sql_ast_node_integer_type_is_bool_alias(node) == 0) {
        fprintf(stderr, "%s: expected bool alias marker\n", context);
        return 1;
    }

    return 0;
}

static int expect_integer_serial_alias(
    const struct mylite_sql_ast_node *node,
    const char *context
) {
    if (mylite_sql_ast_node_integer_type_is_serial_alias(node) == 0) {
        fprintf(stderr, "%s: expected serial alias marker\n", context);
        return 1;
    }

    return 0;
}

static int expect_nullability(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_nullability expected,
    const char *context
) {
    enum mylite_sql_ast_nullability actual = mylite_sql_ast_node_nullability(node);

    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %s, got %s\n",
            context,
            mylite_sql_ast_nullability_name(expected),
            mylite_sql_ast_nullability_name(actual)
        );
        return 1;
    }

    return 0;
}

static int expect_order_direction(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_order_direction expected,
    const char *context
) {
    enum mylite_sql_ast_order_direction actual = mylite_sql_ast_node_order_direction(node);

    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected order direction %s, got %s\n",
            context,
            mylite_sql_ast_order_direction_name(expected),
            mylite_sql_ast_order_direction_name(actual)
        );
        return 1;
    }

    return 0;
}

static int expect_column_visibility(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_column_visibility expected,
    const char *context
) {
    enum mylite_sql_ast_column_visibility actual = mylite_sql_ast_node_column_visibility(node);

    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected column visibility %s, got %s\n",
            context,
            mylite_sql_ast_column_visibility_name(expected),
            mylite_sql_ast_column_visibility_name(actual)
        );
        return 1;
    }

    return 0;
}
