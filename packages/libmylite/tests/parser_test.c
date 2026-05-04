#include "sql/mylite_ast.h"
#include "sql/mylite_parser.h"

#include <stdio.h>
#include <string.h>

static int test_empty_script(void);
static int test_use_statements(void);
static int test_schema_lifecycle_statements(void);
static int test_connection_charset_statements(void);
static int test_create_table_integer_boolean_columns(void);
static int test_create_table_string_binary_columns(void);
static int test_create_table_numeric_columns(void);
static int test_create_table_temporal_columns(void);
static int test_create_table_column_attributes(void);
static int test_create_table_primary_keys_auto_increment(void);
static int test_create_table_unique_secondary_indexes(void);
static int test_create_table_base_execution_syntax(void);
static int test_create_drop_index_syntax(void);
static int test_alter_table_column_operations_syntax(void);
static int test_alter_table_key_constraint_operations_syntax(void);
static int test_rename_table_syntax(void);
static int test_truncate_table_syntax(void);
static int test_show_variables_syntax(void);
static int test_show_status_syntax(void);
static int test_show_engines_syntax(void);
static int test_show_character_set_syntax(void);
static int test_show_collation_syntax(void);
static int test_show_tables_syntax(void);
static int test_show_table_status_syntax(void);
static int test_show_columns_syntax(void);
static int test_show_index_syntax(void);
static int test_show_create_database_syntax(void);
static int test_show_create_table_syntax(void);
static int test_show_diagnostics_syntax(void);
static int test_describe_table_syntax(void);
static int test_drop_table_syntax(void);
static int test_insert_values_syntax(void);
static int test_insert_set_syntax(void);
static int test_replace_syntax(void);
static int test_insert_on_duplicate_key_update_syntax(void);
static int test_update_single_table_syntax(void);
static int test_delete_single_table_syntax(void);
static int test_transaction_statement_syntax(void);
static int test_savepoint_statement_syntax(void);
static int test_select_expression_list(void);
static int test_expression_operator_foundation_syntax(void);
static int test_scalar_function_call_syntax(void);
static int test_case_expression_syntax(void);
static int test_cast_expression_syntax(void);
static int test_convert_expression_syntax(void);
static int test_information_schema_select(void);
static int test_select_table_core_syntax(void);
static int test_select_inner_join_syntax(void);
static int test_select_outer_join_syntax(void);
static int test_select_where_clause_syntax(void);
static int test_select_order_limit_offset_syntax(void);
static int test_select_distinct_syntax(void);
static int test_union_query_expression_syntax(void);
static int test_aggregate_grouping_syntax(void);
static int test_subquery_expression_syntax(void);
static int test_unary_and_parenthesized_expression(void);
static int test_literal_categories(void);
static int test_qualified_identifier_keyword_part(void);
static int test_comments_are_skipped(void);
static int test_syntax_errors(void);
static int test_lexer_errors(void);
static int parse_sql(const char *sql, enum mylite_sql_parse_status expected_status,
                     struct mylite_sql_parse_result *out_result);
static const struct mylite_sql_ast_node *child_at(const struct mylite_sql_ast_node *node,
                                                  size_t index);
static int expect_node(const struct mylite_sql_ast_node *node,
                       enum mylite_sql_ast_node_kind expected_kind, const char *context);
static int expect_child_count(const struct mylite_sql_ast_node *node, size_t expected,
                              const char *context);
static int expect_bool(bool actual, bool expected, const char *context);
static int expect_span_text(const struct mylite_sql_ast_node *node, const char *expected,
                            const char *context);
static int expect_function_call(const struct mylite_sql_ast_node *node, const char *expected_name,
                                size_t expected_arg_count, const char *context);
static int expect_aggregate_call(const struct mylite_sql_ast_node *node,
                                 enum mylite_sql_ast_aggregate_kind expected_kind,
                                 enum mylite_sql_ast_aggregate_argument expected_argument,
                                 const char *expected_name, const char *context);
static int expect_join_type(const struct mylite_sql_ast_node *node,
                            enum mylite_sql_ast_join_type expected, const char *context);
static int expect_join_condition_type(const struct mylite_sql_ast_node *node,
                                      enum mylite_sql_ast_join_condition_type expected,
                                      const char *context);
static int expect_select_duplicate_mode(const struct mylite_sql_ast_node *node,
                                        enum mylite_sql_ast_select_duplicate_mode expected,
                                        bool explicit_mode, bool conflict, size_t modifier_count,
                                        const char *context);
static int expect_union_duplicate_mode(const struct mylite_sql_ast_node *node,
                                       enum mylite_sql_ast_set_duplicate_mode expected,
                                       const char *context);
static int expect_subquery_quantifier(const struct mylite_sql_ast_node *node,
                                      enum mylite_sql_ast_subquery_quantifier expected,
                                      const char *context);
static int expect_literal(const struct mylite_sql_ast_node *node,
                          enum mylite_sql_ast_literal_kind expected, const char *context);
static int expect_operator(const struct mylite_sql_ast_node *node,
                           enum mylite_sql_ast_operator expected, const char *context);
static int expect_schema_option(const struct mylite_sql_ast_node *node,
                                enum mylite_sql_ast_schema_option expected, const char *context);
static int expect_column_type(const struct mylite_sql_ast_node *node,
                              enum mylite_sql_ast_column_type expected, const char *context);
static int expect_column_attribute(const struct mylite_sql_ast_node *node,
                                   enum mylite_sql_ast_column_attribute expected,
                                   const char *context);
static int expect_column_format(const struct mylite_sql_ast_node *node,
                                enum mylite_sql_ast_column_format expected, const char *context);
static int expect_column_storage(const struct mylite_sql_ast_node *node,
                                 enum mylite_sql_ast_column_storage expected, const char *context);
static int expect_key_part_order(const struct mylite_sql_ast_node *node,
                                 enum mylite_sql_ast_key_part_order expected, const char *context);
static int expect_order_item_direction(const struct mylite_sql_ast_node *node,
                                       enum mylite_sql_ast_key_part_order expected,
                                       const char *context);
static int expect_group_item_direction(const struct mylite_sql_ast_node *node,
                                       enum mylite_sql_ast_key_part_order expected,
                                       const char *context);
static int expect_limit_bound(const struct mylite_sql_ast_node *node, uint64_t expected,
                              const char *context);
static int expect_index_algorithm(const struct mylite_sql_ast_node *node,
                                  enum mylite_sql_ast_index_algorithm expected,
                                  const char *context);
static int expect_index_option(const struct mylite_sql_ast_node *node,
                               enum mylite_sql_ast_index_option expected, const char *context);
static int expect_index_class(const struct mylite_sql_ast_node *node,
                              enum mylite_sql_ast_index_class expected, const char *context);
static int expect_ddl_table_option(const struct mylite_sql_ast_node *node,
                                   enum mylite_sql_ast_ddl_table_option expected,
                                   const char *context);
static int expect_alter_table_action(const struct mylite_sql_ast_node *node,
                                     enum mylite_sql_ast_alter_table_action expected,
                                     bool column_keyword, const char *context);
static int
expect_alter_table_column_position(const struct mylite_sql_ast_node *node,
                                   enum mylite_sql_ast_alter_table_column_position expected,
                                   const char *context);
static int expect_table_option(const struct mylite_sql_ast_node *node,
                               enum mylite_sql_ast_table_option expected, const char *context);
static int expect_transaction_access_mode(const struct mylite_sql_ast_node *node,
                                          enum mylite_sql_ast_transaction_access_mode expected,
                                          const char *context);
static int expect_transaction_completion(const struct mylite_sql_ast_node *node,
                                         enum mylite_sql_ast_transaction_chain expected_chain,
                                         enum mylite_sql_ast_transaction_release expected_release,
                                         const char *context);
static int expect_current_timestamp(const struct mylite_sql_ast_node *node, bool has_precision,
                                    uint64_t precision, const char *context);

int main(void)
{
    int failures = 0;

    failures += test_empty_script();
    failures += test_use_statements();
    failures += test_schema_lifecycle_statements();
    failures += test_connection_charset_statements();
    failures += test_create_table_integer_boolean_columns();
    failures += test_create_table_string_binary_columns();
    failures += test_create_table_numeric_columns();
    failures += test_create_table_temporal_columns();
    failures += test_create_table_column_attributes();
    failures += test_create_table_primary_keys_auto_increment();
    failures += test_create_table_unique_secondary_indexes();
    failures += test_create_table_base_execution_syntax();
    failures += test_create_drop_index_syntax();
    failures += test_alter_table_column_operations_syntax();
    failures += test_alter_table_key_constraint_operations_syntax();
    failures += test_rename_table_syntax();
    failures += test_truncate_table_syntax();
    failures += test_show_variables_syntax();
    failures += test_show_status_syntax();
    failures += test_show_engines_syntax();
    failures += test_show_character_set_syntax();
    failures += test_show_collation_syntax();
    failures += test_show_tables_syntax();
    failures += test_show_table_status_syntax();
    failures += test_show_columns_syntax();
    failures += test_show_index_syntax();
    failures += test_show_create_database_syntax();
    failures += test_show_create_table_syntax();
    failures += test_show_diagnostics_syntax();
    failures += test_describe_table_syntax();
    failures += test_drop_table_syntax();
    failures += test_insert_values_syntax();
    failures += test_insert_set_syntax();
    failures += test_replace_syntax();
    failures += test_insert_on_duplicate_key_update_syntax();
    failures += test_update_single_table_syntax();
    failures += test_delete_single_table_syntax();
    failures += test_transaction_statement_syntax();
    failures += test_savepoint_statement_syntax();
    failures += test_select_expression_list();
    failures += test_expression_operator_foundation_syntax();
    failures += test_scalar_function_call_syntax();
    failures += test_case_expression_syntax();
    failures += test_cast_expression_syntax();
    failures += test_convert_expression_syntax();
    failures += test_information_schema_select();
    failures += test_select_table_core_syntax();
    failures += test_select_inner_join_syntax();
    failures += test_select_outer_join_syntax();
    failures += test_select_where_clause_syntax();
    failures += test_select_order_limit_offset_syntax();
    failures += test_select_distinct_syntax();
    failures += test_union_query_expression_syntax();
    failures += test_aggregate_grouping_syntax();
    failures += test_subquery_expression_syntax();
    failures += test_unary_and_parenthesized_expression();
    failures += test_literal_categories();
    failures += test_qualified_identifier_keyword_part();
    failures += test_comments_are_skipped();
    failures += test_syntax_errors();
    failures += test_lexer_errors();

    return failures == 0 ? 0 : 1;
}

static int test_empty_script(void)
{
    struct mylite_sql_parse_result result;
    int failures = 0;

    failures += parse_sql("", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(result.root, MYLITE_SQL_AST_SCRIPT, "empty root");
    failures += expect_child_count(result.root, 0U, "empty root");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 1U, "trailing semicolon root");
    failures += expect_node(child_at(result.root, 0U), MYLITE_SQL_AST_SELECT_STATEMENT,
                            "trailing semicolon statement");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 123", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(result.root, MYLITE_SQL_AST_SCRIPT, "select 123 root");
    failures += expect_node(child_at(result.root, 0U), MYLITE_SQL_AST_SELECT_STATEMENT,
                            "select 123 statement");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_use_statements(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *first_use = NULL;
    const struct mylite_sql_ast_node *second_use = NULL;
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
    return failures;
}

static int test_schema_lifecycle_statements(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *options = NULL;
    int failures = 0;

    failures += parse_sql("CREATE DATABASE IF NOT EXISTS mylite_app DEFAULT CHARACTER SET utf8mb4 "
                          "COLLATE utf8mb4_bin ENCRYPTION='N';",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    options = child_at(statement, 2U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT, "create schema");
    failures +=
        expect_node(child_at(statement, 0U), MYLITE_SQL_AST_IF_NOT_EXISTS, "create if not exists");
    failures += expect_span_text(child_at(statement, 1U), "mylite_app", "create schema name");
    failures += expect_child_count(options, 3U, "create options");
    failures += expect_schema_option(
        child_at(options, 0U), MYLITE_SQL_AST_SCHEMA_OPTION_CHARACTER_SET, "create charset option");
    failures += expect_schema_option(child_at(options, 1U), MYLITE_SQL_AST_SCHEMA_OPTION_COLLATE,
                                     "create collate option");
    failures += expect_schema_option(child_at(options, 2U), MYLITE_SQL_AST_SCHEMA_OPTION_ENCRYPTION,
                                     "create encryption option");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE DATABASE encryption DEFAULT CHARSET 'utf8mb4' "
                          "COLLATE 'utf8mb4_bin';",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    options = child_at(statement, 1U);
    failures += expect_span_text(child_at(statement, 0U), "encryption", "nonreserved schema name");
    failures += expect_child_count(options, 2U, "create charset alias options");
    failures += expect_literal(child_at(child_at(options, 0U), 0U), MYLITE_SQL_AST_LITERAL_STRING,
                               "quoted charset value");
    failures += expect_literal(child_at(child_at(options, 1U), 0U), MYLITE_SQL_AST_LITERAL_STRING,
                               "quoted collate value");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER SCHEMA DEFAULT COLLATE utf8mb4_0900_ai_ci READ ONLY = 1;",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    options = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT, "alter schema");
    failures += expect_child_count(statement, 1U, "alter omitted schema child count");
    failures += expect_child_count(options, 2U, "alter options");
    failures += expect_schema_option(child_at(options, 0U), MYLITE_SQL_AST_SCHEMA_OPTION_COLLATE,
                                     "alter collate option");
    failures += expect_schema_option(child_at(options, 1U), MYLITE_SQL_AST_SCHEMA_OPTION_READ_ONLY,
                                     "alter read only option");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP SCHEMA IF EXISTS `select`;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT, "drop schema");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_IF_EXISTS, "drop if exists");
    failures += expect_span_text(child_at(statement, 1U), "`select`", "drop schema name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW DATABASES; SHOW SCHEMAS;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(child_at(result.root, 0U), MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT,
                            "show databases");
    failures += expect_node(child_at(result.root, 1U), MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT,
                            "show schemas");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW DATABASES LIKE 'my%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_connection_charset_statements(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SET NAMES utf8mb4;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SET_NAMES_STATEMENT, "set names");
    failures += expect_span_text(child_at(statement, 0U), "utf8mb4", "set names charset");
    failures += expect_child_count(statement, 1U, "set names child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET NAMES 'latin1' COLLATE 'latin1_bin';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SET_NAMES_STATEMENT, "set names collate");
    failures += expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING,
                               "set names quoted charset");
    failures += expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING,
                               "set names quoted collation");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET NAMES binary; SET NAMES DEFAULT;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 2U, "set names script");
    failures +=
        expect_span_text(child_at(child_at(result.root, 0U), 0U), "binary", "set names binary");
    failures += expect_node(child_at(child_at(result.root, 1U), 0U), MYLITE_SQL_AST_DEFAULT,
                            "set names default");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET CHARACTER SET utf8mb3;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT, "set character set");
    failures += expect_span_text(child_at(statement, 0U), "utf8mb3", "set character set charset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET CHARACTER SET 'latin1';", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT, "set character set");
    failures += expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING,
                               "set character set quoted charset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET CHARSET binary;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT, "set charset");
    failures += expect_span_text(child_at(statement, 0U), "binary", "set charset charset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SET CHARSET DEFAULT;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT, "set charset default");
    failures +=
        expect_node(child_at(statement, 0U), MYLITE_SQL_AST_DEFAULT, "set charset default value");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SET NAMES DEFAULT COLLATE utf8mb4_bin;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SET SESSION sql_mode = 'ANSI_QUOTES';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_create_table_integer_boolean_columns(void)
{
    enum {
        expected_column_count = 12,
        int_display_width = 10,
        integer_alias_column = 4,
        bigint_column = 5,
        bool_column = 6,
        boolean_column = 7,
        int1_column = 8,
        int2_column = 9,
        int3_column = 10,
        middleint_column = 11,
        int4_column = 2,
        int8_column = 3,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *column_type = NULL;
    int failures = 0;

    failures += parse_sql("CREATE TABLE app.integer_types ("
                          "a TINYINT, b SMALLINT SIGNED, c MEDIUMINT UNSIGNED, "
                          "d INT(10), e INTEGER, f BIGINT(20) UNSIGNED, "
                          "g BOOL, h BOOLEAN, i INT1, j INT2, k INT3, l MIDDLEINT);",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);

    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, "create table");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "create table name");
    failures += expect_node(columns, MYLITE_SQL_AST_COLUMN_DEFINITION_LIST, "column list");
    failures += expect_child_count(columns, expected_column_count, "integer columns");

    column = child_at(columns, 0U);
    column_type = child_at(column, 1U);
    failures += expect_span_text(child_at(column, 0U), "a", "first column name");
    failures +=
        expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_TINYINT, "tinyint column");

    column_type = child_at(child_at(columns, 1U), 1U);
    failures +=
        expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_SMALLINT, "smallint column");
    if (column_type != NULL && !column_type->column_type_signed) {
        fprintf(stderr, "smallint signed flag was not set\n");
        failures = 1;
    }

    column_type = child_at(child_at(columns, 2U), 1U);
    failures +=
        expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMINT, "mediumint column");
    if (column_type != NULL && !column_type->column_type_unsigned) {
        fprintf(stderr, "mediumint unsigned flag was not set\n");
        failures = 1;
    }

    column_type = child_at(child_at(columns, 3U), 1U);
    failures += expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_INT, "int column");
    if (column_type != NULL && (!column_type->has_column_display_width ||
                                column_type->column_display_width != int_display_width)) {
        fprintf(stderr, "int display width was not recorded as 10\n");
        failures = 1;
    }
    failures += expect_span_text(column_type, "INT(10)", "int display width span");

    failures += expect_column_type(child_at(child_at(columns, integer_alias_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_INT, "integer alias column");
    failures += expect_column_type(child_at(child_at(columns, bigint_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_BIGINT, "bigint column");
    failures += expect_column_type(child_at(child_at(columns, bool_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_BOOL, "bool column");
    failures += expect_column_type(child_at(child_at(columns, boolean_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_BOOLEAN, "boolean column");
    failures += expect_column_type(child_at(child_at(columns, int1_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_TINYINT, "int1 alias");
    failures += expect_column_type(child_at(child_at(columns, int2_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_SMALLINT, "int2 alias");
    failures += expect_column_type(child_at(child_at(columns, int3_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMINT, "int3 alias");
    failures += expect_column_type(child_at(child_at(columns, middleint_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMINT, "middleint alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE signedness (a INT SIGNED UNSIGNED, "
                          "b INT UNSIGNED SIGNED, c INT4, d INT8);",
                          MYLITE_SQL_PARSE_OK, &result);
    columns = child_at(child_at(result.root, 0U), 1U);
    column_type = child_at(child_at(columns, 0U), 1U);
    if (column_type != NULL &&
        (!column_type->column_type_signed || !column_type->column_type_unsigned)) {
        fprintf(stderr, "mixed signedness flags were not set\n");
        failures = 1;
    }
    failures += expect_column_type(child_at(child_at(columns, int4_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_INT, "int4 alias");
    failures += expect_column_type(child_at(child_at(columns, int8_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_BIGINT, "int8 alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE `schema`.`widths` (`select` INT(0), `from` INT(255));",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE bad_int0_alias (a INT0);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_bool_width (a BOOL(1));", MYLITE_SQL_PARSE_SYNTAX_ERROR,
                          &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_bool_signed (a BOOL SIGNED);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_bool_unsigned (a BOOL UNSIGNED);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_boolean_width (a BOOLEAN(1));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_boolean_signed (a BOOLEAN SIGNED);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_bool_unsigned (a BOOLEAN UNSIGNED);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_int_width (a INT(256));", MYLITE_SQL_PARSE_SYNTAX_ERROR,
                          &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_int_negative (a INT(-1));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE bad_int_alias (a INT5);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE bad_int6_alias (a INT6);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE bad_int7_alias (a INT7);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE bad_int9_alias (a INT9);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE supported_attributes (a INT NOT NULL);",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_create_table_string_binary_columns(void)
{
    enum {
        expected_column_count = 31,
        varchar_length = 7,
        char_column = 0,
        varchar_column = 3,
        binary_column = 6,
        varbinary_column = 7,
        tinytext_column = 8,
        text_column = 9,
        longtext_column = 12,
        blob_column = 14,
        longblob_column = 16,
        char_byte_column = 20,
        nchar_column = 24,
        nvarchar_column = 25,
        bare_char_byte_column = 26,
        national_char_column = 27,
        national_varchar_column = 28,
        char_collate_binary_column = 29,
        text_collate_binary_column = 30,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column_type = NULL;
    int failures = 0;

    failures += parse_sql("CREATE TABLE app.string_binary_types ("
                          "a CHAR, b CHAR(4) CHARACTER SET latin1, "
                          "c CHARACTER(5) COLLATE latin1_swedish_ci, "
                          "d VARCHAR(7), e CHAR VARYING(8), "
                          "f CHARACTER VARYING(9) BINARY, "
                          "g BINARY, h VARBINARY(9), "
                          "i TINYTEXT, j TEXT, k TEXT(63) CHARACTER SET binary, "
                          "l MEDIUMTEXT, m LONGTEXT, n TINYBLOB, o BLOB, "
                          "p MEDIUMBLOB, q LONGBLOB, r TEXT BINARY, "
                          "s CHAR CHARACTER SET binary, t VARCHAR(4) CHARSET binary, "
                          "u CHAR(4) BYTE, v VARCHAR(4) BYTE, "
                          "w LONG VARCHAR, x LONG VARBINARY, y NCHAR(4), z NVARCHAR(4), "
                          "aa CHAR BYTE, ab NATIONAL CHAR(3), ac NATIONAL VARCHAR(3), "
                          "ad CHAR(4) COLLATE binary, ae TEXT COLLATE binary);",
                          MYLITE_SQL_PARSE_OK, &result);
    columns = child_at(child_at(result.root, 0U), 1U);
    failures += expect_node(columns, MYLITE_SQL_AST_COLUMN_DEFINITION_LIST, "string column list");
    failures += expect_child_count(columns, expected_column_count, "string columns");

    column_type = child_at(child_at(columns, char_column), 1U);
    failures += expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_CHAR, "char column");
    failures += expect_span_text(column_type, "CHAR", "char span");

    column_type = child_at(child_at(columns, varchar_column), 1U);
    failures +=
        expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR, "varchar column");
    if (column_type != NULL &&
        (!column_type->has_column_length || column_type->column_length != varchar_length)) {
        fprintf(stderr, "varchar length was not recorded as 7\n");
        failures = 1;
    }

    failures += expect_column_type(child_at(child_at(columns, binary_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_BINARY, "binary column");
    failures += expect_column_type(child_at(child_at(columns, varbinary_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_VARBINARY, "varbinary column");
    failures += expect_column_type(child_at(child_at(columns, tinytext_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_TINYTEXT, "tinytext column");
    failures += expect_column_type(child_at(child_at(columns, text_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_TEXT, "text column");
    failures += expect_column_type(child_at(child_at(columns, longtext_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_LONGTEXT, "longtext column");
    failures += expect_column_type(child_at(child_at(columns, blob_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_BLOB, "blob column");
    failures += expect_column_type(child_at(child_at(columns, longblob_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_LONGBLOB, "longblob column");

    column_type = child_at(child_at(columns, char_byte_column), 1U);
    failures +=
        expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_CHAR, "char byte column");
    if (column_type != NULL && !column_type->column_byte_attribute) {
        fprintf(stderr, "char byte attribute was not recorded\n");
        failures = 1;
    }

    column_type = child_at(child_at(columns, bare_char_byte_column), 1U);
    failures +=
        expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_CHAR, "bare char byte column");
    if (column_type != NULL && !column_type->column_byte_attribute) {
        fprintf(stderr, "bare char byte attribute was not recorded\n");
        failures = 1;
    }

    column_type = child_at(child_at(columns, nchar_column), 1U);
    failures += expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_CHAR, "nchar column");
    if (column_type != NULL && !column_type->column_national_attribute) {
        fprintf(stderr, "nchar national attribute was not recorded\n");
        failures = 1;
    }

    column_type = child_at(child_at(columns, nvarchar_column), 1U);
    failures +=
        expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR, "nvarchar column");
    if (column_type != NULL && !column_type->column_national_attribute) {
        fprintf(stderr, "nvarchar national attribute was not recorded\n");
        failures = 1;
    }

    column_type = child_at(child_at(columns, national_char_column), 1U);
    failures +=
        expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_CHAR, "national char column");
    if (column_type != NULL && !column_type->column_national_attribute) {
        fprintf(stderr, "national char attribute was not recorded\n");
        failures = 1;
    }

    column_type = child_at(child_at(columns, national_varchar_column), 1U);
    failures += expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR,
                                   "national varchar column");
    if (column_type != NULL && !column_type->column_national_attribute) {
        fprintf(stderr, "national varchar attribute was not recorded\n");
        failures = 1;
    }

    column_type = child_at(child_at(columns, char_collate_binary_column), 1U);
    failures += expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_CHAR,
                                   "char collate binary column");
    if (column_type != NULL && !column_type->has_column_collation) {
        fprintf(stderr, "char collate binary collation was not recorded\n");
        failures = 1;
    }

    column_type = child_at(child_at(columns, text_collate_binary_column), 1U);
    failures += expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_TEXT,
                                   "text collate binary column");
    if (column_type != NULL && !column_type->has_column_collation) {
        fprintf(stderr, "text collate binary collation was not recorded\n");
        failures = 1;
    }
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_char_length (a CHAR(256));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_binary_length (a BINARY(256));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_varchar_missing (a VARCHAR);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_varchar_length (a VARCHAR(16384));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_varbinary_missing (a VARBINARY);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_varbinary_length (a VARBINARY(65536));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_tinytext_length (a TINYTEXT(1));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_longtext_length (a LONGTEXT(1));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_blob_charset (a BLOB CHARACTER SET utf8mb4);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_mediumblob_length (a MEDIUMBLOB(1));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_long_varchar_length (a LONG VARCHAR(1));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_long_varbinary_length (a LONG VARBINARY(1));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_varchar_byte_missing (a VARCHAR BYTE);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_text_length (a TEXT(4294967296));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_blob_length (a BLOB(4294967296));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_varchar_length_overflow "
                          "(a VARCHAR(18446744073709551616));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_text_length_overflow "
                          "(a TEXT(18446744073709551616));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_blob_length_overflow "
                          "(a BLOB(18446744073709551616));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_collation (a CHAR CHARACTER SET utf8mb4 "
                          "COLLATE latin1_swedish_ci);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_binary_collation (a CHAR CHARACTER SET utf8mb4 "
                          "COLLATE binary);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_create_table_numeric_columns(void)
{
    enum {
        expected_column_count = 30,
        decimal_precision = 10,
        decimal_scale = 2,
        float_double_cutover_precision = 25,
        numeric_column = 3,
        fixed_column = 4,
        float_column = 5,
        float_selector_column = 6,
        float_scaled_column = 7,
        double_column = 8,
        double_precision_column = 9,
        real_column = 10,
        float4_column = 11,
        float8_column = 12,
        decimal_unsigned_column = 13,
        decimal_zerofill_column = 14,
        float_zerofill_column = 15,
        double_mixed_column = 16,
        float25_unsigned_column = 17,
        float25_scaled_unsigned_column = 18,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column_type = NULL;
    int failures = 0;

    failures += parse_sql("CREATE TABLE app.numeric_types ("
                          "a DECIMAL, b DECIMAL(10), c DECIMAL(10,2), "
                          "d NUMERIC(8,3), e FIXED(7,2), "
                          "f FLOAT, g FLOAT(25), h FLOAT(25,2), "
                          "i DOUBLE, j DOUBLE PRECISION, k REAL, l FLOAT4, m FLOAT8, "
                          "n DECIMAL(10,2) UNSIGNED, o DECIMAL ZEROFILL SIGNED, "
                          "p FLOAT ZEROFILL SIGNED, q DOUBLE UNSIGNED ZEROFILL SIGNED, "
                          "r FLOAT(25) UNSIGNED, s FLOAT(25,2) UNSIGNED, "
                          "t DEC, u DECIMAL(0), v DECIMAL(0,0), "
                          "w FLOAT(255,30), x DOUBLE(255,30), "
                          "y FLOAT4(10), z FLOAT4(25), aa FLOAT4(10,2), "
                          "ab FLOAT8(10,2), ac DOUBLE PRECISION(10,2), ad REAL(10,2));",
                          MYLITE_SQL_PARSE_OK, &result);
    columns = child_at(child_at(result.root, 0U), 1U);
    failures += expect_node(columns, MYLITE_SQL_AST_COLUMN_DEFINITION_LIST, "numeric column list");
    failures += expect_child_count(columns, expected_column_count, "numeric columns");

    column_type = child_at(child_at(columns, 2U), 1U);
    failures += expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL,
                                   "decimal scaled column");
    if (column_type != NULL &&
        (!column_type->has_column_precision || column_type->column_precision != decimal_precision ||
         !column_type->has_column_scale || column_type->column_scale != decimal_scale)) {
        fprintf(stderr, "decimal precision/scale was not recorded as 10,2\n");
        failures = 1;
    }
    failures += expect_span_text(column_type, "DECIMAL(10,2)", "decimal span");

    failures += expect_column_type(child_at(child_at(columns, numeric_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL, "numeric alias");
    failures += expect_column_type(child_at(child_at(columns, fixed_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL, "fixed alias");
    failures += expect_column_type(child_at(child_at(columns, float_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_FLOAT, "float column");
    failures += expect_column_type(child_at(child_at(columns, float_selector_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_FLOAT, "float selector column");
    failures += expect_column_type(child_at(child_at(columns, float_scaled_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_FLOAT, "float scaled column");
    failures += expect_column_type(child_at(child_at(columns, double_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE, "double column");
    failures += expect_column_type(child_at(child_at(columns, double_precision_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE, "double precision column");
    failures += expect_column_type(child_at(child_at(columns, real_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE, "real column");
    failures += expect_column_type(child_at(child_at(columns, float4_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_FLOAT, "float4 alias");
    failures += expect_column_type(child_at(child_at(columns, float8_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE, "float8 alias");

    column_type = child_at(child_at(columns, decimal_unsigned_column), 1U);
    if (column_type != NULL && !column_type->column_type_unsigned) {
        fprintf(stderr, "decimal unsigned flag was not recorded\n");
        failures = 1;
    }

    column_type = child_at(child_at(columns, decimal_zerofill_column), 1U);
    if (column_type != NULL &&
        (!column_type->column_zerofill_attribute || !column_type->column_type_signed)) {
        fprintf(stderr, "decimal zerofill/signed flags were not recorded\n");
        failures = 1;
    }

    column_type = child_at(child_at(columns, float_zerofill_column), 1U);
    if (column_type != NULL &&
        (!column_type->column_zerofill_attribute || !column_type->column_type_signed)) {
        fprintf(stderr, "float zerofill/signed flags were not recorded\n");
        failures = 1;
    }

    column_type = child_at(child_at(columns, double_mixed_column), 1U);
    if (column_type != NULL &&
        (!column_type->column_type_unsigned || !column_type->column_zerofill_attribute ||
         !column_type->column_type_signed)) {
        fprintf(stderr, "double mixed attribute flags were not recorded\n");
        failures = 1;
    }

    column_type = child_at(child_at(columns, float25_unsigned_column), 1U);
    if (column_type != NULL && (!column_type->has_column_precision ||
                                column_type->column_precision != float_double_cutover_precision ||
                                !column_type->column_type_unsigned)) {
        fprintf(stderr, "float(25) unsigned metadata was not recorded\n");
        failures = 1;
    }

    column_type = child_at(child_at(columns, float25_scaled_unsigned_column), 1U);
    if (column_type != NULL &&
        (!column_type->has_column_precision ||
         column_type->column_precision != float_double_cutover_precision ||
         !column_type->has_column_scale || column_type->column_scale != decimal_scale ||
         !column_type->column_type_unsigned)) {
        fprintf(stderr, "float(25,2) unsigned metadata was not recorded\n");
        failures = 1;
    }
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_decimal_precision (a DECIMAL(66));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_decimal_scale (a DECIMAL(65,31));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_decimal_scale_gt_precision (a DECIMAL(10,11));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_decimal_negative (a DECIMAL(-1));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_decimal_missing_scale (a DECIMAL(10,));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_decimal_missing_precision (a DECIMAL(,2));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_decimal_precision_overflow "
                          "(a DECIMAL(18446744073709551616));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_decimal_zero_scale (a DECIMAL(0,1));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_float_precision (a FLOAT(54));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_float_zero_display (a FLOAT(0,0));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_float_zero_scale (a FLOAT(0,1));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_float_display (a FLOAT(256,30));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_float_scale (a FLOAT(10,31));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_double_precision_only (a DOUBLE(10));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_double_zero_display (a DOUBLE(0,0));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_double_zero_scale (a DOUBLE(0,1));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_double_precision_only_zerofill "
                          "(a DOUBLE(10) ZEROFILL);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_double_overflow "
                          "(a DOUBLE(18446744073709551616,1));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_create_table_temporal_columns(void)
{
    enum {
        expected_column_count = 16,
        time_fsp_column = 2,
        datetime_fsp_column = 6,
        timestamp_column = 7,
        timestamp_fsp_column = 10,
        year_column = 11,
        year_width_column = 12,
        year_leading_zero_column = 13,
        datetime_leading_zero_column = 14,
    };
    const unsigned long long temporal_fsp_max = 6ULL;
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column_type = NULL;
    int failures = 0;

    failures += parse_sql("CREATE TABLE app.temporal_types ("
                          "a DATE, b TIME, c TIME(1), d TIME(6), "
                          "e DATETIME, f DATETIME(0), g DATETIME(6), "
                          "h TIMESTAMP, i TIMESTAMP(0), j TIMESTAMP(1), k TIMESTAMP(6), "
                          "l YEAR, m YEAR(4), n YEAR(004), o DATETIME(06), p TIME(00));",
                          MYLITE_SQL_PARSE_OK, &result);
    columns = child_at(child_at(result.root, 0U), 1U);
    failures += expect_node(columns, MYLITE_SQL_AST_COLUMN_DEFINITION_LIST, "temporal column list");
    failures += expect_child_count(columns, expected_column_count, "temporal columns");

    failures += expect_column_type(child_at(child_at(columns, 0U), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_DATE, "date column");
    failures += expect_column_type(child_at(child_at(columns, 1U), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_TIME, "time column");

    column_type = child_at(child_at(columns, time_fsp_column), 1U);
    failures += expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_TIME, "time fsp column");
    if (column_type != NULL &&
        (!column_type->has_column_precision || column_type->column_precision != 1ULL)) {
        fprintf(stderr, "time precision was not recorded as 1\n");
        failures = 1;
    }

    failures += expect_column_type(child_at(child_at(columns, 4U), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_DATETIME, "datetime column");

    column_type = child_at(child_at(columns, datetime_fsp_column), 1U);
    failures +=
        expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_DATETIME, "datetime fsp column");
    if (column_type != NULL &&
        (!column_type->has_column_precision || column_type->column_precision != temporal_fsp_max)) {
        fprintf(stderr, "datetime precision was not recorded as 6\n");
        failures = 1;
    }

    failures += expect_column_type(child_at(child_at(columns, timestamp_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_TIMESTAMP, "timestamp column");

    column_type = child_at(child_at(columns, timestamp_fsp_column), 1U);
    failures += expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_TIMESTAMP,
                                   "timestamp fsp column");
    if (column_type != NULL &&
        (!column_type->has_column_precision || column_type->column_precision != temporal_fsp_max)) {
        fprintf(stderr, "timestamp precision was not recorded as 6\n");
        failures = 1;
    }

    failures += expect_column_type(child_at(child_at(columns, year_column), 1U),
                                   MYLITE_SQL_AST_COLUMN_TYPE_YEAR, "year column");

    column_type = child_at(child_at(columns, year_width_column), 1U);
    failures +=
        expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_YEAR, "year width column");
    if (column_type != NULL &&
        (!column_type->has_column_precision || column_type->column_precision != 4ULL)) {
        fprintf(stderr, "year width was not recorded as 4\n");
        failures = 1;
    }

    column_type = child_at(child_at(columns, datetime_leading_zero_column), 1U);
    failures += expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_DATETIME,
                                   "datetime leading zero fsp");
    if (column_type != NULL &&
        (!column_type->has_column_precision || column_type->column_precision != temporal_fsp_max)) {
        fprintf(stderr, "datetime leading-zero precision was not recorded as 6\n");
        failures = 1;
    }

    column_type = child_at(child_at(columns, year_leading_zero_column), 1U);
    if (column_type != NULL &&
        (!column_type->has_column_precision || column_type->column_precision != 4ULL)) {
        fprintf(stderr, "year leading-zero width was not recorded as 4\n");
        failures = 1;
    }
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE temporal_keyword_names ("
                          "date INT, time INT, datetime INT, timestamp INT, year INT);",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE bad_date_fsp (a DATE(0));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_date_fsp_one (a DATE(1));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE bad_time_fsp (a TIME(7));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_datetime_fsp (a DATETIME(7));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_timestamp_fsp (a TIMESTAMP(7));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_time_negative (a TIME(-1));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_time_empty_fsp (a TIME());",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_time_string_fsp (a TIME('1'));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_time_scale (a TIME(1,2));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_datetime_scale (a DATETIME(1,2));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_time_overflow_scale "
                          "(a TIME(1,18446744073709551616));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_datetime_overflow "
                          "(a DATETIME(18446744073709551616));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_time_overflow "
                          "(a TIME(18446744073709551616));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_timestamp_overflow "
                          "(a TIMESTAMP(18446744073709551616));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_year_zero (a YEAR(0));", MYLITE_SQL_PARSE_SYNTAX_ERROR,
                          &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE bad_year_one (a YEAR(1));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE bad_year_two (a YEAR(2));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_year_three (a YEAR(3));", MYLITE_SQL_PARSE_SYNTAX_ERROR,
                          &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_year_five (a YEAR(5));", MYLITE_SQL_PARSE_SYNTAX_ERROR,
                          &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_year_overflow "
                          "(a YEAR(18446744073709551616));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_time_unsigned (a TIME UNSIGNED);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_create_table_column_attributes(void)
{
    enum {
        expected_column_count = 24,
        timestamp_precision = 6,
        null_column = 0,
        not_null_column = 1,
        default_int_column = 2,
        default_negative_column = 3,
        default_positive_column = 4,
        default_hex_column = 5,
        default_bit_column = 6,
        default_empty_string_column = 7,
        default_expression_column = 8,
        default_nested_expression_column = 9,
        default_current_timestamp_column = 10,
        default_current_timestamp_parens_column = 11,
        timestamp_update_column = 12,
        parenthesized_current_timestamp_column = 13,
        comment_column = 14,
        visible_column = 15,
        invisible_column = 16,
        column_format_default_column = 17,
        column_format_fixed_column = 18,
        column_format_dynamic_column = 19,
        storage_default_column = 20,
        storage_disk_column = 21,
        storage_memory_column = 22,
        repeated_attribute_column = 23,
        repeated_attribute_count = 6,
        repeated_invisible_attribute = 5,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *attributes = NULL;
    const struct mylite_sql_ast_node *attribute = NULL;
    const struct mylite_sql_ast_node *value = NULL;
    int failures = 0;

    failures += parse_sql("CREATE TABLE app.attr_valid ("
                          "a INT NULL, b INT NOT NULL, c INT DEFAULT 7, "
                          "d INT DEFAULT -1, e INT DEFAULT +2, "
                          "f INT DEFAULT 0x10, g INT DEFAULT b'101', "
                          "h VARCHAR(20) DEFAULT '', i INT DEFAULT (1 + 2), "
                          "j INT DEFAULT ((1 + 2) * 3), "
                          "k TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                          "l TIMESTAMP DEFAULT CURRENT_TIMESTAMP(), "
                          "m TIMESTAMP(6) DEFAULT CURRENT_TIMESTAMP(6) "
                          "ON UPDATE CURRENT_TIMESTAMP(6), "
                          "n TIMESTAMP DEFAULT (CURRENT_TIMESTAMP), "
                          "o INT COMMENT 'hello', p INT VISIBLE, q INT INVISIBLE, "
                          "r INT COLUMN_FORMAT DEFAULT, s INT COLUMN_FORMAT FIXED, "
                          "t INT COLUMN_FORMAT DYNAMIC, u INT STORAGE DEFAULT, "
                          "v INT STORAGE DISK, w INT STORAGE MEMORY, "
                          "x INT NULL NOT NULL DEFAULT 1 DEFAULT 2 VISIBLE INVISIBLE);",
                          MYLITE_SQL_PARSE_OK, &result);
    columns = child_at(child_at(result.root, 0U), 1U);
    failures +=
        expect_node(columns, MYLITE_SQL_AST_COLUMN_DEFINITION_LIST, "attribute column list");
    failures += expect_child_count(columns, expected_column_count, "attribute columns");

    attributes = child_at(child_at(columns, null_column), 2U);
    failures += expect_node(attributes, MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST, "null attributes");
    failures += expect_child_count(attributes, 1U, "null attribute count");
    failures += expect_column_attribute(child_at(attributes, 0U),
                                        MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NULL, "null attribute");

    attributes = child_at(child_at(columns, not_null_column), 2U);
    failures += expect_column_attribute(
        child_at(attributes, 0U), MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NOT_NULL, "not null attribute");

    attributes = child_at(child_at(columns, default_int_column), 2U);
    attribute = child_at(attributes, 0U);
    failures += expect_column_attribute(attribute, MYLITE_SQL_AST_COLUMN_ATTRIBUTE_DEFAULT,
                                        "integer default attribute");
    failures += expect_literal(child_at(attribute, 0U), MYLITE_SQL_AST_LITERAL_INTEGER,
                               "integer default literal");

    attributes = child_at(child_at(columns, default_negative_column), 2U);
    value = child_at(child_at(attributes, 0U), 0U);
    failures += expect_node(value, MYLITE_SQL_AST_UNARY_EXPRESSION, "negative default");
    failures += expect_operator(value, MYLITE_SQL_AST_OPERATOR_NEGATIVE, "negative default");

    attributes = child_at(child_at(columns, default_positive_column), 2U);
    value = child_at(child_at(attributes, 0U), 0U);
    failures += expect_node(value, MYLITE_SQL_AST_UNARY_EXPRESSION, "positive default");
    failures += expect_operator(value, MYLITE_SQL_AST_OPERATOR_POSITIVE, "positive default");

    attributes = child_at(child_at(columns, default_hex_column), 2U);
    failures += expect_literal(child_at(child_at(attributes, 0U), 0U), MYLITE_SQL_AST_LITERAL_HEX,
                               "hex default literal");

    attributes = child_at(child_at(columns, default_bit_column), 2U);
    failures += expect_literal(child_at(child_at(attributes, 0U), 0U), MYLITE_SQL_AST_LITERAL_BIT,
                               "bit default literal");

    attributes = child_at(child_at(columns, default_empty_string_column), 2U);
    failures += expect_literal(child_at(child_at(attributes, 0U), 0U),
                               MYLITE_SQL_AST_LITERAL_STRING, "empty string default literal");

    attributes = child_at(child_at(columns, default_expression_column), 2U);
    value = child_at(child_at(attributes, 0U), 0U);
    failures +=
        expect_node(value, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION, "expression default wrapper");
    failures +=
        expect_operator(child_at(value, 0U), MYLITE_SQL_AST_OPERATOR_ADD, "expression default add");

    attributes = child_at(child_at(columns, default_nested_expression_column), 2U);
    value = child_at(child_at(attributes, 0U), 0U);
    failures += expect_node(value, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
                            "nested expression default wrapper");
    failures += expect_operator(child_at(value, 0U), MYLITE_SQL_AST_OPERATOR_MULTIPLY,
                                "nested expression default multiply");

    attributes = child_at(child_at(columns, default_current_timestamp_column), 2U);
    failures += expect_current_timestamp(child_at(child_at(attributes, 0U), 0U), false, 0U,
                                         "bare current timestamp default");

    attributes = child_at(child_at(columns, default_current_timestamp_parens_column), 2U);
    failures += expect_current_timestamp(child_at(child_at(attributes, 0U), 0U), false, 0U,
                                         "empty-parens current timestamp default");

    attributes = child_at(child_at(columns, timestamp_update_column), 2U);
    failures += expect_child_count(attributes, 2U, "timestamp update attribute count");
    failures += expect_current_timestamp(child_at(child_at(attributes, 0U), 0U), true,
                                         timestamp_precision, "fsp current timestamp default");
    failures += expect_column_attribute(
        child_at(attributes, 1U), MYLITE_SQL_AST_COLUMN_ATTRIBUTE_ON_UPDATE, "on update attribute");
    failures += expect_current_timestamp(child_at(child_at(attributes, 1U), 0U), true,
                                         timestamp_precision, "fsp current timestamp update");

    attributes = child_at(child_at(columns, parenthesized_current_timestamp_column), 2U);
    value = child_at(child_at(attributes, 0U), 0U);
    failures += expect_node(value, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
                            "parenthesized current timestamp wrapper");
    failures +=
        expect_current_timestamp(child_at(value, 0U), false, 0U, "parenthesized current timestamp");

    attributes = child_at(child_at(columns, comment_column), 2U);
    failures += expect_column_attribute(
        child_at(attributes, 0U), MYLITE_SQL_AST_COLUMN_ATTRIBUTE_COMMENT, "comment attribute");
    failures += expect_literal(child_at(child_at(attributes, 0U), 0U),
                               MYLITE_SQL_AST_LITERAL_STRING, "comment literal");

    attributes = child_at(child_at(columns, visible_column), 2U);
    failures += expect_column_attribute(
        child_at(attributes, 0U), MYLITE_SQL_AST_COLUMN_ATTRIBUTE_VISIBLE, "visible attribute");

    attributes = child_at(child_at(columns, invisible_column), 2U);
    failures += expect_column_attribute(
        child_at(attributes, 0U), MYLITE_SQL_AST_COLUMN_ATTRIBUTE_INVISIBLE, "invisible attribute");

    attributes = child_at(child_at(columns, column_format_default_column), 2U);
    failures += expect_column_format(child_at(attributes, 0U), MYLITE_SQL_AST_COLUMN_FORMAT_DEFAULT,
                                     "column format default");

    attributes = child_at(child_at(columns, column_format_fixed_column), 2U);
    failures += expect_column_format(child_at(attributes, 0U), MYLITE_SQL_AST_COLUMN_FORMAT_FIXED,
                                     "column format fixed");

    attributes = child_at(child_at(columns, column_format_dynamic_column), 2U);
    failures += expect_column_format(child_at(attributes, 0U), MYLITE_SQL_AST_COLUMN_FORMAT_DYNAMIC,
                                     "column format dynamic");

    attributes = child_at(child_at(columns, storage_default_column), 2U);
    failures += expect_column_storage(child_at(attributes, 0U),
                                      MYLITE_SQL_AST_COLUMN_STORAGE_DEFAULT, "storage default");

    attributes = child_at(child_at(columns, storage_disk_column), 2U);
    failures += expect_column_storage(child_at(attributes, 0U), MYLITE_SQL_AST_COLUMN_STORAGE_DISK,
                                      "storage disk");

    attributes = child_at(child_at(columns, storage_memory_column), 2U);
    failures += expect_column_storage(child_at(attributes, 0U),
                                      MYLITE_SQL_AST_COLUMN_STORAGE_MEMORY, "storage memory");

    attributes = child_at(child_at(columns, repeated_attribute_column), 2U);
    failures +=
        expect_child_count(attributes, repeated_attribute_count, "repeated attribute count");
    failures += expect_column_attribute(
        child_at(attributes, 0U), MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NULL, "repeated null attribute");
    failures +=
        expect_column_attribute(child_at(attributes, 1U), MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NOT_NULL,
                                "repeated not null attribute");
    failures +=
        expect_column_attribute(child_at(attributes, 2U), MYLITE_SQL_AST_COLUMN_ATTRIBUTE_DEFAULT,
                                "repeated first default");
    failures +=
        expect_column_attribute(child_at(attributes, 3U), MYLITE_SQL_AST_COLUMN_ATTRIBUTE_DEFAULT,
                                "repeated second default");
    failures +=
        expect_column_attribute(child_at(attributes, 4U), MYLITE_SQL_AST_COLUMN_ATTRIBUTE_VISIBLE,
                                "repeated visible attribute");
    failures += expect_column_attribute(child_at(attributes, repeated_invisible_attribute),
                                        MYLITE_SQL_AST_COLUMN_ATTRIBUTE_INVISIBLE,
                                        "repeated invisible attribute");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE attr_keyword_names ("
                          "comment INT, visible INT, invisible INT, storage INT, "
                          "column_format INT, disk INT, memory INT, dynamic INT, fixed INT);",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE timestamp_zero_fsp "
                          "(a TIMESTAMP DEFAULT CURRENT_TIMESTAMP(0) "
                          "ON UPDATE CURRENT_TIMESTAMP(0));",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_default_identifier (default INT);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_current_timestamp_identifier "
                          "(current_timestamp INT);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_comment_literal (a INT COMMENT 123);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_comment_equal (a INT COMMENT = 'x');",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_column_format (a INT COLUMN_FORMAT COMPRESSED);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_column_format_equal (a INT COLUMN_FORMAT = FIXED);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_storage (a INT STORAGE FLASH);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_storage_equal (a INT STORAGE = DISK);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_default_equal (a INT DEFAULT = 1);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_on_update_missing_value (a TIMESTAMP ON UPDATE);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_on_missing_update (a TIMESTAMP ON CURRENT_TIMESTAMP);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_on_update_literal (a TIMESTAMP ON UPDATE 1);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_current_timestamp_default_fsp "
                          "(a TIMESTAMP DEFAULT CURRENT_TIMESTAMP(7));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_current_timestamp_update_fsp "
                          "(a TIMESTAMP ON UPDATE CURRENT_TIMESTAMP(7));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_current_timestamp_negative "
                          "(a TIMESTAMP DEFAULT CURRENT_TIMESTAMP(-1));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_current_timestamp_string "
                          "(a TIMESTAMP DEFAULT CURRENT_TIMESTAMP('1'));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_current_timestamp_scale "
                          "(a TIMESTAMP DEFAULT CURRENT_TIMESTAMP(1,2));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_current_timestamp_overflow "
                          "(a TIMESTAMP DEFAULT "
                          "CURRENT_TIMESTAMP(18446744073709551616));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_default_unparenthesized_expression "
                          "(a INT DEFAULT 1 + 2);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_default_signed_string "
                          "(a VARCHAR(20) DEFAULT +'abc');",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_default_signed_hex "
                          "(a INT DEFAULT +0x10);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_default_signed_null "
                          "(a INT DEFAULT -NULL);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_default_signed_boolean "
                          "(a INT DEFAULT +TRUE);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_default_function (a VARCHAR(20) DEFAULT UPPER('x'));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_default_parenthesized_function "
                          "(a VARCHAR(20) DEFAULT (UPPER('x')));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE supported_auto_increment (a INT AUTO_INCREMENT);",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE supported_inline_primary_key (a INT PRIMARY KEY);",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_create_table_primary_keys_auto_increment(void)
{
    enum {
        expected_element_count = 10,
        id_column = 0,
        shorthand_column = 1,
        no_key_auto_column = 2,
        nullable_primary_column = 3,
        decimal_auto_column = 5,
        float_auto_column = 6,
        named_primary_constraint = 7,
        constraint_without_name = 8,
        constraint_with_index_name = 9,
        prefix_length = 10,
        key_block_size = 8,
        named_primary_option_count = 5,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *elements = NULL;
    const struct mylite_sql_ast_node *attributes = NULL;
    const struct mylite_sql_ast_node *constraint = NULL;
    const struct mylite_sql_ast_node *key_parts = NULL;
    const struct mylite_sql_ast_node *key_part = NULL;
    const struct mylite_sql_ast_node *options = NULL;
    int failures = 0;

    failures += parse_sql("CREATE TABLE app.primary_valid ("
                          "id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, "
                          "shorthand INT KEY, no_key BIGINT AUTO_INCREMENT, "
                          "nullable_pk INT NULL PRIMARY KEY, "
                          "v VARCHAR(20) NOT NULL DEFAULT '' COMMENT 'v' VISIBLE, "
                          "d DECIMAL AUTO_INCREMENT PRIMARY KEY, "
                          "f FLOAT AUTO_INCREMENT PRIMARY KEY, "
                          "PRIMARY KEY pk_named USING BTREE (id, v(10) DESC) "
                          "KEY_BLOCK_SIZE = 8 COMMENT 'pk' VISIBLE "
                          "ENGINE_ATTRIBUTE='{}' SECONDARY_ENGINE_ATTRIBUTE '', "
                          "CONSTRAINT PRIMARY KEY (shorthand ASC) USING HASH INVISIBLE, "
                          "CONSTRAINT named PRIMARY KEY named_pk (nullable_pk DESC));",
                          MYLITE_SQL_PARSE_OK, &result);
    elements = child_at(child_at(result.root, 0U), 1U);
    failures +=
        expect_node(elements, MYLITE_SQL_AST_COLUMN_DEFINITION_LIST, "primary element list");
    failures += expect_child_count(elements, expected_element_count, "primary element count");

    attributes = child_at(child_at(elements, id_column), 2U);
    failures += expect_child_count(attributes, 2U, "id primary attribute count");
    failures += expect_column_attribute(child_at(attributes, 0U),
                                        MYLITE_SQL_AST_COLUMN_ATTRIBUTE_AUTO_INCREMENT,
                                        "id auto_increment attribute");
    failures += expect_column_attribute(child_at(attributes, 1U),
                                        MYLITE_SQL_AST_COLUMN_ATTRIBUTE_PRIMARY_KEY,
                                        "id primary attribute");
    failures +=
        expect_span_text(child_at(attributes, 1U), "PRIMARY KEY", "id primary attribute span");

    attributes = child_at(child_at(elements, shorthand_column), 2U);
    failures += expect_column_attribute(child_at(attributes, 0U),
                                        MYLITE_SQL_AST_COLUMN_ATTRIBUTE_PRIMARY_KEY,
                                        "inline key alias attribute");
    failures += expect_span_text(child_at(attributes, 0U), "KEY", "inline key alias span");

    attributes = child_at(child_at(elements, no_key_auto_column), 2U);
    failures += expect_column_attribute(child_at(attributes, 0U),
                                        MYLITE_SQL_AST_COLUMN_ATTRIBUTE_AUTO_INCREMENT,
                                        "no-key auto_increment attribute");

    attributes = child_at(child_at(elements, nullable_primary_column), 2U);
    failures += expect_child_count(attributes, 2U, "nullable primary attributes");
    failures +=
        expect_column_attribute(child_at(attributes, 0U), MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NULL,
                                "nullable primary null attribute");
    failures += expect_column_attribute(child_at(attributes, 1U),
                                        MYLITE_SQL_AST_COLUMN_ATTRIBUTE_PRIMARY_KEY,
                                        "nullable primary key attribute");

    attributes = child_at(child_at(elements, decimal_auto_column), 2U);
    failures += expect_column_attribute(child_at(attributes, 0U),
                                        MYLITE_SQL_AST_COLUMN_ATTRIBUTE_AUTO_INCREMENT,
                                        "decimal auto_increment attribute");
    failures += expect_column_attribute(child_at(attributes, 1U),
                                        MYLITE_SQL_AST_COLUMN_ATTRIBUTE_PRIMARY_KEY,
                                        "decimal primary attribute");

    attributes = child_at(child_at(elements, float_auto_column), 2U);
    failures += expect_column_attribute(child_at(attributes, 0U),
                                        MYLITE_SQL_AST_COLUMN_ATTRIBUTE_AUTO_INCREMENT,
                                        "float auto_increment attribute");

    constraint = child_at(elements, named_primary_constraint);
    failures +=
        expect_node(constraint, MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT, "named primary constraint");
    failures += expect_span_text(child_at(constraint, 0U), "pk_named", "primary index name");
    failures +=
        expect_index_algorithm(child_at(constraint, 1U), MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE,
                               "primary index type before list");
    key_parts = child_at(constraint, 2U);
    failures += expect_node(key_parts, MYLITE_SQL_AST_KEY_PART_LIST, "named primary key parts");
    failures += expect_child_count(key_parts, 2U, "named primary key part count");
    key_part = child_at(key_parts, 0U);
    failures += expect_span_text(child_at(key_part, 0U), "id", "first key part name");
    failures +=
        expect_key_part_order(key_part, MYLITE_SQL_AST_KEY_PART_ORDER_NONE, "first key part order");
    key_part = child_at(key_parts, 1U);
    failures += expect_span_text(child_at(key_part, 0U), "v", "prefix key part name");
    failures += expect_child_count(key_part, 2U, "prefix key part children");
    if (child_at(key_part, 1U) != NULL &&
        (!child_at(key_part, 1U)->has_column_length ||
         child_at(key_part, 1U)->column_length != prefix_length)) {
        fprintf(stderr, "prefix key part did not record length 10\n");
        failures = 1;
    }
    failures += expect_key_part_order(key_part, MYLITE_SQL_AST_KEY_PART_ORDER_DESC,
                                      "prefix key part desc order");
    options = child_at(constraint, 3U);
    failures += expect_node(options, MYLITE_SQL_AST_INDEX_OPTION_LIST, "named primary options");
    failures +=
        expect_child_count(options, named_primary_option_count, "named primary option count");
    failures += expect_index_option(child_at(options, 0U),
                                    MYLITE_SQL_AST_INDEX_OPTION_KEY_BLOCK_SIZE, "key block option");
    if (child_at(child_at(options, 0U), 0U) != NULL &&
        child_at(child_at(options, 0U), 0U)->column_length != key_block_size) {
        fprintf(stderr, "key block size was not recorded as 8\n");
        failures = 1;
    }
    failures += expect_index_option(child_at(options, 1U), MYLITE_SQL_AST_INDEX_OPTION_COMMENT,
                                    "primary comment option");
    failures += expect_index_option(child_at(options, 2U), MYLITE_SQL_AST_INDEX_OPTION_VISIBLE,
                                    "primary visible option");
    failures +=
        expect_index_option(child_at(options, 3U), MYLITE_SQL_AST_INDEX_OPTION_ENGINE_ATTRIBUTE,
                            "primary engine attribute option");
    failures += expect_index_option(child_at(options, 4U),
                                    MYLITE_SQL_AST_INDEX_OPTION_SECONDARY_ENGINE_ATTRIBUTE,
                                    "primary secondary engine attribute option");

    constraint = child_at(elements, constraint_without_name);
    failures +=
        expect_node(constraint, MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT, "constraint without symbol");
    key_parts = child_at(constraint, 0U);
    failures += expect_node(key_parts, MYLITE_SQL_AST_KEY_PART_LIST, "constraint key parts");
    key_part = child_at(key_parts, 0U);
    failures +=
        expect_key_part_order(key_part, MYLITE_SQL_AST_KEY_PART_ORDER_ASC, "constraint asc order");
    options = child_at(constraint, 1U);
    failures += expect_index_option(child_at(options, 0U), MYLITE_SQL_AST_INDEX_OPTION_USING,
                                    "post-list using option");
    failures +=
        expect_index_algorithm(child_at(child_at(options, 0U), 0U),
                               MYLITE_SQL_AST_INDEX_ALGORITHM_HASH, "post-list hash index type");
    failures += expect_index_option(child_at(options, 1U), MYLITE_SQL_AST_INDEX_OPTION_INVISIBLE,
                                    "primary invisible option");

    constraint = child_at(elements, constraint_with_index_name);
    failures += expect_span_text(child_at(constraint, 0U), "named", "primary constraint symbol");
    failures += expect_span_text(child_at(constraint, 1U), "named_pk", "primary index name");
    key_parts = child_at(constraint, 2U);
    failures += expect_key_part_order(child_at(key_parts, 0U), MYLITE_SQL_AST_KEY_PART_ORDER_DESC,
                                      "named constraint desc order");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE pk_keyword_names ("
                          "auto_increment INT, btree INT, hash INT, "
                          "key_block_size INT, engine_attribute INT, "
                          "secondary_engine_attribute INT);",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE pk_prefix_zero "
                          "(a VARCHAR(10), PRIMARY KEY (a(0)));",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE pk_option_order ("
                          "a INT, PRIMARY KEY USING BTREE (a) COMMENT 'pk' "
                          "KEY_BLOCK_SIZE 8 VISIBLE, "
                          "b INT, PRIMARY KEY (b) COMMENT 'pk' USING BTREE);",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_primary_identifier (primary INT);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_key_identifier (key INT);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_constraint_identifier (constraint INT);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_asc_identifier (asc INT);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_desc_identifier (desc INT);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_index_identifier (index INT);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_primary_empty (a INT, PRIMARY KEY ());",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_primary_trailing (a INT, PRIMARY KEY (a,));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_primary_functional "
                          "(a INT, PRIMARY KEY ((a + 1)));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_primary_overflow_prefix "
                          "(a VARCHAR(10), PRIMARY KEY (a(18446744073709551616)));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_primary_negative_prefix "
                          "(a VARCHAR(10), PRIMARY KEY (a(-1)));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_primary_string_prefix "
                          "(a VARCHAR(10), PRIMARY KEY (a('1')));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_primary_key_block_string "
                          "(a INT, PRIMARY KEY (a) KEY_BLOCK_SIZE '8');",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_primary_key_block_negative "
                          "(a INT, PRIMARY KEY (a) KEY_BLOCK_SIZE -1);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_primary_key_block_overflow "
                          "(a INT, PRIMARY KEY (a) "
                          "KEY_BLOCK_SIZE 18446744073709551616);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_primary_prelist_comment "
                          "(a INT, PRIMARY KEY COMMENT 'pk' (a));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_primary_comment_equal "
                          "(a INT, PRIMARY KEY (a) COMMENT = 'pk');",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_primary_comment_number "
                          "(a INT, PRIMARY KEY (a) COMMENT 8);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_primary_using_rtree "
                          "(a INT, PRIMARY KEY USING RTREE (a));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_primary_engine_attribute_number "
                          "(a INT, PRIMARY KEY (a) ENGINE_ATTRIBUTE 123);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_primary_secondary_engine_attribute_number "
                          "(a INT, PRIMARY KEY (a) SECONDARY_ENGINE_ATTRIBUTE = 123);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_create_table_unique_secondary_indexes(void)
{
    enum {
        expected_element_count = 19,
        unique_column = 0,
        unique_key_column = 1,
        primary_key_shorthand_column = 2,
        unnamed_secondary_index = 8,
        unnamed_index_keyword = 9,
        named_secondary_index = 10,
        typed_secondary_index = 11,
        unnamed_unique_index = 12,
        named_unique_key = 13,
        typed_unique_key = 14,
        typed_unique_index = 15,
        repeated_using_unique_key = 16,
        named_unique_constraint = 17,
        unnamed_unique_constraint = 18,
        typed_secondary_option_count = 5,
        prefix_length = 5,
        key_block_size = 8,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *elements = NULL;
    const struct mylite_sql_ast_node *attributes = NULL;
    const struct mylite_sql_ast_node *index = NULL;
    const struct mylite_sql_ast_node *key_parts = NULL;
    const struct mylite_sql_ast_node *key_part = NULL;
    const struct mylite_sql_ast_node *options = NULL;
    int failures = 0;

    failures += parse_sql("CREATE TABLE app.indexes_valid ("
                          "a INT UNIQUE, b VARCHAR(20) UNIQUE KEY, c INT KEY, "
                          "btree INT, hash INT, key_block_size INT, "
                          "engine_attribute INT, secondary_engine_attribute INT, "
                          "KEY (a), "
                          "INDEX (hash), "
                          "INDEX idx_b USING BTREE (b(5) DESC, a ASC) COMMENT 'hello' "
                          "VISIBLE KEY_BLOCK_SIZE = 8, "
                          "KEY USING HASH (btree) USING HASH USING BTREE INVISIBLE "
                          "ENGINE_ATTRIBUTE '{}' SECONDARY_ENGINE_ATTRIBUTE = '{}', "
                          "UNIQUE (a), UNIQUE KEY uk_b (b), "
                          "UNIQUE KEY USING BTREE (hash), "
                          "UNIQUE INDEX ux_c USING BTREE (c), "
                          "UNIQUE KEY uq_hash (a) USING HASH USING BTREE, "
                          "CONSTRAINT uq_d UNIQUE KEY unique_d (btree DESC), "
                          "CONSTRAINT UNIQUE uq_a (a));",
                          MYLITE_SQL_PARSE_OK, &result);
    elements = child_at(child_at(result.root, 0U), 1U);
    failures += expect_node(elements, MYLITE_SQL_AST_COLUMN_DEFINITION_LIST, "index element list");
    failures += expect_child_count(elements, expected_element_count, "index element count");

    attributes = child_at(child_at(elements, unique_column), 2U);
    failures += expect_column_attribute(child_at(attributes, 0U),
                                        MYLITE_SQL_AST_COLUMN_ATTRIBUTE_UNIQUE_KEY,
                                        "inline unique attribute");
    failures += expect_span_text(child_at(attributes, 0U), "UNIQUE", "inline unique span");

    attributes = child_at(child_at(elements, unique_key_column), 2U);
    failures += expect_column_attribute(child_at(attributes, 0U),
                                        MYLITE_SQL_AST_COLUMN_ATTRIBUTE_UNIQUE_KEY,
                                        "inline unique key attribute");
    failures += expect_span_text(child_at(attributes, 0U), "UNIQUE KEY", "inline unique key span");

    attributes = child_at(child_at(elements, primary_key_shorthand_column), 2U);
    failures += expect_column_attribute(child_at(attributes, 0U),
                                        MYLITE_SQL_AST_COLUMN_ATTRIBUTE_PRIMARY_KEY,
                                        "inline key remains primary shorthand");

    index = child_at(elements, unnamed_secondary_index);
    failures += expect_node(index, MYLITE_SQL_AST_SECONDARY_INDEX, "unnamed secondary index");
    key_parts = child_at(index, 0U);
    failures += expect_node(key_parts, MYLITE_SQL_AST_KEY_PART_LIST, "unnamed secondary parts");
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 0U), "a", "unnamed secondary key part");
    failures += expect_node(child_at(index, 1U), MYLITE_SQL_AST_INDEX_OPTION_LIST,
                            "unnamed secondary options");

    index = child_at(elements, unnamed_index_keyword);
    failures += expect_node(index, MYLITE_SQL_AST_SECONDARY_INDEX, "unnamed INDEX secondary");
    key_parts = child_at(index, 0U);
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 0U), "hash", "unnamed INDEX key part");

    index = child_at(elements, named_secondary_index);
    failures += expect_node(index, MYLITE_SQL_AST_SECONDARY_INDEX, "named secondary index");
    failures += expect_span_text(child_at(index, 0U), "idx_b", "secondary index name");
    failures += expect_index_algorithm(child_at(index, 1U), MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE,
                                       "secondary pre-list index type");
    key_parts = child_at(index, 2U);
    failures += expect_child_count(key_parts, 2U, "secondary key part count");
    key_part = child_at(key_parts, 0U);
    failures += expect_span_text(child_at(key_part, 0U), "b", "secondary prefix key part");
    if (child_at(key_part, 1U) != NULL &&
        (!child_at(key_part, 1U)->has_column_length ||
         child_at(key_part, 1U)->column_length != prefix_length)) {
        fprintf(stderr, "secondary prefix key part did not record length 5\n");
        failures = 1;
    }
    failures += expect_key_part_order(key_part, MYLITE_SQL_AST_KEY_PART_ORDER_DESC,
                                      "secondary prefix desc order");
    key_part = child_at(key_parts, 1U);
    failures +=
        expect_key_part_order(key_part, MYLITE_SQL_AST_KEY_PART_ORDER_ASC, "secondary asc order");
    options = child_at(index, 3U);
    failures += expect_child_count(options, 3U, "secondary option count");
    failures += expect_index_option(child_at(options, 0U), MYLITE_SQL_AST_INDEX_OPTION_COMMENT,
                                    "secondary comment option");
    failures += expect_index_option(child_at(options, 1U), MYLITE_SQL_AST_INDEX_OPTION_VISIBLE,
                                    "secondary visible option");
    failures +=
        expect_index_option(child_at(options, 2U), MYLITE_SQL_AST_INDEX_OPTION_KEY_BLOCK_SIZE,
                            "secondary key block option");
    if (child_at(child_at(options, 2U), 0U) != NULL &&
        child_at(child_at(options, 2U), 0U)->column_length != key_block_size) {
        fprintf(stderr, "secondary key block size was not recorded as 8\n");
        failures = 1;
    }

    index = child_at(elements, typed_secondary_index);
    failures += expect_node(index, MYLITE_SQL_AST_SECONDARY_INDEX, "typed secondary index");
    failures += expect_index_algorithm(child_at(index, 0U), MYLITE_SQL_AST_INDEX_ALGORITHM_HASH,
                                       "secondary hash pre-list index type");
    key_parts = child_at(index, 1U);
    failures += expect_span_text(child_at(child_at(key_parts, 0U), 0U), "btree",
                                 "fallback keyword key part");
    options = child_at(index, 2U);
    failures +=
        expect_child_count(options, typed_secondary_option_count, "typed secondary option count");
    failures += expect_index_option(child_at(options, 0U), MYLITE_SQL_AST_INDEX_OPTION_USING,
                                    "secondary post-list hash using option");
    failures += expect_index_option(child_at(options, 1U), MYLITE_SQL_AST_INDEX_OPTION_USING,
                                    "secondary post-list btree using option");
    failures += expect_index_option(child_at(options, 2U), MYLITE_SQL_AST_INDEX_OPTION_INVISIBLE,
                                    "secondary invisible option");
    failures +=
        expect_index_option(child_at(options, 3U), MYLITE_SQL_AST_INDEX_OPTION_ENGINE_ATTRIBUTE,
                            "secondary engine attribute option");
    failures += expect_index_option(child_at(options, 4U),
                                    MYLITE_SQL_AST_INDEX_OPTION_SECONDARY_ENGINE_ATTRIBUTE,
                                    "secondary secondary-engine attribute option");

    index = child_at(elements, unnamed_unique_index);
    failures += expect_node(index, MYLITE_SQL_AST_UNIQUE_INDEX, "unnamed unique index");
    failures +=
        expect_node(child_at(index, 0U), MYLITE_SQL_AST_KEY_PART_LIST, "unnamed unique parts");

    index = child_at(elements, named_unique_key);
    failures += expect_node(index, MYLITE_SQL_AST_UNIQUE_INDEX, "named unique key");
    failures += expect_span_text(child_at(index, 0U), "uk_b", "unique key name");
    failures +=
        expect_node(child_at(index, 1U), MYLITE_SQL_AST_KEY_PART_LIST, "named unique key parts");

    index = child_at(elements, typed_unique_key);
    failures += expect_node(index, MYLITE_SQL_AST_UNIQUE_INDEX, "typed unique key");
    failures += expect_index_algorithm(child_at(index, 0U), MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE,
                                       "unique key pre-list type");
    key_parts = child_at(index, 1U);
    failures +=
        expect_span_text(child_at(child_at(key_parts, 0U), 0U), "hash", "typed unique key part");

    index = child_at(elements, typed_unique_index);
    failures += expect_node(index, MYLITE_SQL_AST_UNIQUE_INDEX, "typed unique index");
    failures += expect_span_text(child_at(index, 0U), "ux_c", "unique index name");
    failures += expect_index_algorithm(child_at(index, 1U), MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE,
                                       "unique index pre-list type");

    index = child_at(elements, repeated_using_unique_key);
    failures += expect_span_text(child_at(index, 0U), "uq_hash", "repeated using unique name");
    options = child_at(index, 2U);
    failures += expect_child_count(options, 2U, "repeated using unique options");
    failures += expect_index_option(child_at(options, 0U), MYLITE_SQL_AST_INDEX_OPTION_USING,
                                    "unique post-list hash using option");
    failures += expect_index_option(child_at(options, 1U), MYLITE_SQL_AST_INDEX_OPTION_USING,
                                    "unique post-list btree using option");

    index = child_at(elements, named_unique_constraint);
    failures += expect_node(index, MYLITE_SQL_AST_UNIQUE_INDEX, "named unique constraint");
    failures += expect_span_text(child_at(index, 0U), "uq_d", "unique constraint name");
    failures += expect_span_text(child_at(index, 1U), "unique_d", "unique constraint index name");
    key_parts = child_at(index, 2U);
    failures += expect_key_part_order(child_at(key_parts, 0U), MYLITE_SQL_AST_KEY_PART_ORDER_DESC,
                                      "unique constraint desc order");

    index = child_at(elements, unnamed_unique_constraint);
    failures += expect_node(index, MYLITE_SQL_AST_UNIQUE_INDEX, "constraint unique without symbol");
    failures += expect_span_text(child_at(index, 0U), "uq_a", "constraint unique index name");
    failures +=
        expect_node(child_at(index, 1U), MYLITE_SQL_AST_KEY_PART_LIST, "constraint unique parts");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE index_keyword_names (unique INT);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_inline_unique_index (a INT UNIQUE INDEX);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_secondary_empty (a INT, KEY ());",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_secondary_trailing (a INT, KEY (a,));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_secondary_missing_parts (a INT, KEY idx);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_unique_missing_parts (a INT, UNIQUE KEY idx);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_secondary_functional "
                          "(a INT, KEY idx ((a + 1)));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_secondary_fulltext "
                          "(a TEXT, FULLTEXT KEY idx (a));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_secondary_spatial "
                          "(a INT, SPATIAL KEY idx (a));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_secondary_comment_equal "
                          "(a INT, KEY idx (a) COMMENT = 'x');",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_secondary_key_block_string "
                          "(a INT, KEY idx (a) KEY_BLOCK_SIZE '8');",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_unique_overflow_prefix "
                          "(a VARCHAR(10), UNIQUE KEY uq (a(18446744073709551616)));",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_unique_engine_attribute_number "
                          "(a INT, UNIQUE KEY uq (a) ENGINE_ATTRIBUTE 123);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_create_table_base_execution_syntax(void)
{
    enum {
        expected_option_count = 5,
        auto_increment_value = 42,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *options = NULL;
    int failures = 0;

    failures += parse_sql("CREATE TABLE IF NOT EXISTS app.base_options "
                          "(id INT PRIMARY KEY, name VARCHAR(20), KEY name_idx (name)) "
                          "ENGINE=InnoDB DEFAULT CHARACTER SET = latin1 "
                          "COLLATE = latin1_swedish_ci COMMENT = 'comment' "
                          "AUTO_INCREMENT = 42;",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
                            "create table base statement");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "create table qualified name");
    failures += expect_node(child_at(statement, 1U), MYLITE_SQL_AST_COLUMN_DEFINITION_LIST,
                            "create table base elements");
    failures += expect_node(child_at(statement, 2U), MYLITE_SQL_AST_IF_NOT_EXISTS,
                            "create table if not exists");
    options = child_at(statement, 3U);
    failures += expect_node(options, MYLITE_SQL_AST_TABLE_OPTION_LIST, "table option list");
    failures += expect_child_count(options, expected_option_count, "table option count");
    failures += expect_table_option(child_at(options, 0U), MYLITE_SQL_AST_TABLE_OPTION_ENGINE,
                                    "engine option");
    failures += expect_span_text(child_at(child_at(options, 0U), 0U), "InnoDB", "engine value");
    failures += expect_table_option(child_at(options, 1U),
                                    MYLITE_SQL_AST_TABLE_OPTION_CHARACTER_SET, "charset option");
    failures += expect_span_text(child_at(child_at(options, 1U), 0U), "latin1", "charset value");
    failures += expect_table_option(child_at(options, 2U), MYLITE_SQL_AST_TABLE_OPTION_COLLATE,
                                    "collate option");
    failures += expect_span_text(child_at(child_at(options, 2U), 0U), "latin1_swedish_ci",
                                 "collation value");
    failures += expect_table_option(child_at(options, 3U), MYLITE_SQL_AST_TABLE_OPTION_COMMENT,
                                    "comment option");
    failures += expect_literal(child_at(child_at(options, 3U), 0U), MYLITE_SQL_AST_LITERAL_STRING,
                               "comment value");
    failures += expect_table_option(
        child_at(options, 4U), MYLITE_SQL_AST_TABLE_OPTION_AUTO_INCREMENT, "auto increment option");
    if (child_at(child_at(options, 4U), 0U) != NULL &&
        child_at(child_at(options, 4U), 0U)->column_length != auto_increment_value) {
        fprintf(stderr, "table AUTO_INCREMENT option was not recorded as 42\n");
        failures = 1;
    }
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE charset_short (a INT) DEFAULT CHARSET utf8mb4;",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE option_defaults (a INT) DEFAULT CHARSET DEFAULT "
                          "COLLATE DEFAULT;",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_engine_string (a INT) ENGINE='InnoDB';",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_comment_number (a INT) COMMENT = 1;",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_auto_increment_string (a INT) AUTO_INCREMENT = '1';",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE bad_unknown_option (a INT) ROW_FORMAT = DYNAMIC;",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_create_drop_index_syntax(void)
{
    enum {
        prefix_length = 3,
        create_index_options_child = 4,
        create_index_option_count = 5,
        create_index_ddl_options_child = 5,
        key_block_size = 8,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *key_parts = NULL;
    const struct mylite_sql_ast_node *key_part = NULL;
    const struct mylite_sql_ast_node *options = NULL;
    const struct mylite_sql_ast_node *ddl_options = NULL;
    int failures = 0;

    failures += parse_sql("CREATE INDEX idx_a USING BTREE ON app.t "
                          "(a(3) DESC, b ASC) COMMENT 'hello' INVISIBLE "
                          "KEY_BLOCK_SIZE = 8 ENGINE_ATTRIBUTE '{}' "
                          "SECONDARY_ENGINE_ATTRIBUTE = '' ALGORITHM=INPLACE LOCK=NONE;",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_CREATE_INDEX_STATEMENT, "create index statement");
    failures += expect_index_class(statement, MYLITE_SQL_AST_INDEX_CLASS_ORDINARY,
                                   "ordinary create index class");
    failures += expect_span_text(child_at(statement, 0U), "idx_a", "create index name");
    failures += expect_index_algorithm(
        child_at(statement, 1U), MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE, "create index pre-type");
    failures += expect_node(child_at(statement, 2U), MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "create index qualified table");
    key_parts = child_at(statement, 3U);
    failures += expect_child_count(key_parts, 2U, "create index key part count");
    key_part = child_at(key_parts, 0U);
    failures += expect_span_text(child_at(key_part, 0U), "a", "create index prefix column");
    if (child_at(key_part, 1U) != NULL &&
        (!child_at(key_part, 1U)->has_column_length ||
         child_at(key_part, 1U)->column_length != prefix_length)) {
        fprintf(stderr, "create index prefix key part did not record length 3\n");
        failures = 1;
    }
    failures +=
        expect_key_part_order(key_part, MYLITE_SQL_AST_KEY_PART_ORDER_DESC, "create index desc");
    failures += expect_key_part_order(child_at(key_parts, 1U), MYLITE_SQL_AST_KEY_PART_ORDER_ASC,
                                      "create index asc");
    options = child_at(statement, create_index_options_child);
    failures += expect_child_count(options, create_index_option_count, "create index option count");
    failures += expect_index_option(child_at(options, 0U), MYLITE_SQL_AST_INDEX_OPTION_COMMENT,
                                    "create index comment option");
    failures += expect_index_option(child_at(options, 1U), MYLITE_SQL_AST_INDEX_OPTION_INVISIBLE,
                                    "create index invisible option");
    failures +=
        expect_index_option(child_at(options, 2U), MYLITE_SQL_AST_INDEX_OPTION_KEY_BLOCK_SIZE,
                            "create index key block option");
    if (child_at(child_at(options, 2U), 0U) != NULL &&
        child_at(child_at(options, 2U), 0U)->column_length != key_block_size) {
        fprintf(stderr, "create index key block size was not recorded as 8\n");
        failures = 1;
    }
    failures +=
        expect_index_option(child_at(options, 3U), MYLITE_SQL_AST_INDEX_OPTION_ENGINE_ATTRIBUTE,
                            "create index engine attribute option");
    failures += expect_index_option(child_at(options, 4U),
                                    MYLITE_SQL_AST_INDEX_OPTION_SECONDARY_ENGINE_ATTRIBUTE,
                                    "create index secondary engine attribute option");
    ddl_options = child_at(statement, create_index_ddl_options_child);
    failures += expect_child_count(ddl_options, 2U, "create index ddl option count");
    failures += expect_ddl_table_option(child_at(ddl_options, 0U),
                                        MYLITE_SQL_AST_DDL_TABLE_OPTION_ALGORITHM,
                                        "create index algorithm option");
    failures +=
        expect_ddl_table_option(child_at(ddl_options, 1U), MYLITE_SQL_AST_DDL_TABLE_OPTION_LOCK,
                                "create index lock option");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE UNIQUE INDEX uq_a ON t (a) USING HASH TYPE BTREE VISIBLE;",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_index_class(statement, MYLITE_SQL_AST_INDEX_CLASS_UNIQUE,
                                   "unique create index class");
    failures += expect_span_text(child_at(statement, 0U), "uq_a", "unique create index name");
    failures += expect_node(child_at(statement, 1U), MYLITE_SQL_AST_IDENTIFIER,
                            "unique create index table without pre-type");
    key_parts = child_at(statement, 2U);
    failures += expect_node(key_parts, MYLITE_SQL_AST_KEY_PART_LIST, "unique create index parts");
    options = child_at(statement, 3U);
    failures += expect_child_count(options, 3U, "unique create index option count");
    failures += expect_index_algorithm(child_at(child_at(options, 0U), 0U),
                                       MYLITE_SQL_AST_INDEX_ALGORITHM_HASH,
                                       "unique create index hash option");
    failures += expect_index_algorithm(child_at(child_at(options, 1U), 0U),
                                       MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE,
                                       "unique create index type option");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE FULLTEXT INDEX ft_body ON docs (body) WITH PARSER ngram;",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_index_class(child_at(result.root, 0U), MYLITE_SQL_AST_INDEX_CLASS_FULLTEXT,
                                   "fulltext create index class");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE FULLTEXT INDEX ft_body_comment ON docs (body) WITH PARSER ngram "
                          "COMMENT 'ft';",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE INDEX idx_parser ON t (a) WITH PARSER ngram;",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE UNIQUE INDEX uq_parser ON t (a) WITH PARSER ngram;",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE SPATIAL INDEX sp_g ON geo (g);", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_index_class(child_at(result.root, 0U), MYLITE_SQL_AST_INDEX_CLASS_SPATIAL,
                                   "spatial create index class");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP INDEX `PRIMARY` ON t ALGORITHM=COPY LOCK=EXCLUSIVE;",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_INDEX_STATEMENT, "drop index statement");
    failures += expect_span_text(child_at(statement, 0U), "`PRIMARY`", "drop quoted primary");
    ddl_options = child_at(statement, 2U);
    failures += expect_child_count(ddl_options, 2U, "drop index ddl option count");
    failures += expect_ddl_table_option(child_at(ddl_options, 0U),
                                        MYLITE_SQL_AST_DDL_TABLE_OPTION_ALGORITHM,
                                        "drop index algorithm option");
    failures += expect_ddl_table_option(
        child_at(ddl_options, 1U), MYLITE_SQL_AST_DDL_TABLE_OPTION_LOCK, "drop index lock option");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE INDEX idx_lock ON t (a) LOCK=SHARED ALGORITHM DEFAULT;",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE INDEX parser ON t (algorithm);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_span_text(child_at(statement, 0U), "parser", "create index nonreserved parser name");
    key_parts = child_at(statement, 2U);
    key_part = child_at(key_parts, 0U);
    failures += expect_span_text(child_at(key_part, 0U), "algorithm",
                                 "create index nonreserved algorithm key part");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE INDEX ON t (a);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE INDEX IF NOT EXISTS idx ON t (a);", MYLITE_SQL_PARSE_SYNTAX_ERROR,
                          &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP INDEX IF EXISTS idx ON t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP KEY idx ON t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP INDEX PRIMARY ON t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE INDEX idx ON t (a) COMMENT = 'bad';",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE INDEX idx ON t (a) WITH PARSER ngram;",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE INDEX idx ON t ();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE INDEX idx ON t (a,);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_column_operations_syntax(void)
{
    enum {
        complex_varchar_length = 20,
        alter_rename_change_modify_item_count = 5,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *items = NULL;
    const struct mylite_sql_ast_node *action = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *column_type = NULL;
    const struct mylite_sql_ast_node *attributes = NULL;
    const struct mylite_sql_ast_node *position = NULL;
    int failures = 0;

    failures += parse_sql("ALTER TABLE app.t ADD c INT AFTER id, DROP COLUMN old_col, "
                          "ALGORITHM DEFAULT, LOCK=SHARED;",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_ALTER_TABLE_STATEMENT, "alter table statement");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "alter table qualified name");
    items = child_at(statement, 1U);
    failures += expect_node(items, MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST, "alter item list");
    failures += expect_child_count(items, 4U, "alter item count");

    action = child_at(items, 0U);
    failures += expect_alter_table_action(action, MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_COLUMN,
                                          false, "alter add action");
    column = child_at(action, 0U);
    failures += expect_span_text(child_at(column, 0U), "c", "alter add column name");
    failures +=
        expect_column_type(child_at(column, 1U), MYLITE_SQL_AST_COLUMN_TYPE_INT, "alter add type");
    position = child_at(action, 1U);
    failures += expect_alter_table_column_position(
        position, MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION_AFTER, "alter add after");
    failures += expect_span_text(child_at(position, 0U), "id", "alter add after target");

    action = child_at(items, 1U);
    failures += expect_alter_table_action(action, MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_COLUMN,
                                          true, "alter drop action");
    failures += expect_span_text(child_at(action, 0U), "old_col", "alter drop column name");
    failures += expect_ddl_table_option(
        child_at(items, 2U), MYLITE_SQL_AST_DDL_TABLE_OPTION_ALGORITHM, "alter algorithm option");
    failures += expect_ddl_table_option(child_at(items, 3U), MYLITE_SQL_AST_DDL_TABLE_OPTION_LOCK,
                                        "alter lock option");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE t RENAME COLUMN old_name TO new_name, "
                          "CHANGE old_name new_name BIGINT NOT NULL DEFAULT 5 FIRST, "
                          "MODIFY COLUMN new_name VARCHAR(20) COMMENT 'x' AFTER old_name, "
                          "ALGORITHM=INSTANT, LOCK NONE;",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    items = child_at(statement, 1U);
    failures += expect_child_count(items, (size_t)alter_rename_change_modify_item_count,
                                   "alter rename/change/modify item count");

    action = child_at(items, 0U);
    failures += expect_alter_table_action(action, MYLITE_SQL_AST_ALTER_TABLE_ACTION_RENAME_COLUMN,
                                          false, "alter rename action");
    failures += expect_span_text(child_at(action, 0U), "old_name", "alter rename old name");
    failures += expect_span_text(child_at(action, 1U), "new_name", "alter rename new name");

    action = child_at(items, 1U);
    failures += expect_alter_table_action(action, MYLITE_SQL_AST_ALTER_TABLE_ACTION_CHANGE_COLUMN,
                                          false, "alter change action");
    failures += expect_span_text(child_at(action, 0U), "old_name", "alter change old name");
    column = child_at(action, 1U);
    failures += expect_span_text(child_at(column, 0U), "new_name", "alter change new name");
    failures += expect_column_type(child_at(column, 1U), MYLITE_SQL_AST_COLUMN_TYPE_BIGINT,
                                   "alter change type");
    attributes = child_at(column, 2U);
    failures += expect_child_count(attributes, 2U, "alter change attributes");
    position = child_at(action, 2U);
    failures += expect_alter_table_column_position(
        position, MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION_FIRST, "alter change first");

    action = child_at(items, 2U);
    failures += expect_alter_table_action(action, MYLITE_SQL_AST_ALTER_TABLE_ACTION_MODIFY_COLUMN,
                                          true, "alter modify action");
    column = child_at(action, 0U);
    failures += expect_span_text(child_at(column, 0U), "new_name", "alter modify column name");
    failures += expect_column_type(child_at(column, 1U), MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR,
                                   "alter modify type");
    position = child_at(action, 1U);
    failures += expect_alter_table_column_position(
        position, MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION_AFTER, "alter modify after");
    failures += expect_span_text(child_at(position, 0U), "old_name", "alter modify after target");
    failures += expect_ddl_table_option(
        child_at(items, 3U), MYLITE_SQL_AST_DDL_TABLE_OPTION_ALGORITHM, "alter instant option");
    failures +=
        expect_span_text(child_at(items, 3U), "ALGORITHM=INSTANT", "alter instant option span");
    failures += expect_ddl_table_option(child_at(items, 4U), MYLITE_SQL_AST_DDL_TABLE_OPTION_LOCK,
                                        "alter lock none option");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE t ADD COLUMN complex_col VARCHAR(20) "
                          "CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT 'q' "
                          "COMMENT 'text' INVISIBLE;",
                          MYLITE_SQL_PARSE_OK, &result);
    action = child_at(child_at(child_at(result.root, 0U), 1U), 0U);
    failures += expect_alter_table_action(action, MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_COLUMN,
                                          true, "alter add column keyword");
    column = child_at(action, 0U);
    column_type = child_at(column, 1U);
    attributes = child_at(column, 2U);
    failures +=
        expect_column_type(column_type, MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR, "alter complex type");
    if (column_type != NULL &&
        (!column_type->has_column_length || column_type->column_length != complex_varchar_length)) {
        fprintf(stderr, "alter complex VARCHAR length was not recorded as 20\n");
        failures = 1;
    }
    failures += expect_child_count(attributes, 4U, "alter complex attributes");
    failures +=
        expect_column_attribute(child_at(attributes, 0U), MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NOT_NULL,
                                "alter complex not null");
    failures += expect_column_attribute(
        child_at(attributes, 1U), MYLITE_SQL_AST_COLUMN_ATTRIBUTE_DEFAULT, "alter complex default");
    failures += expect_column_attribute(
        child_at(attributes, 2U), MYLITE_SQL_AST_COLUMN_ATTRIBUTE_COMMENT, "alter complex comment");
    failures +=
        expect_column_attribute(child_at(attributes, 3U), MYLITE_SQL_AST_COLUMN_ATTRIBUTE_INVISIBLE,
                                "alter complex invisible");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE t DROP old_col, CHANGE COLUMN old_col new_col INT, "
                          "MODIFY new_col INT;",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE t ADD c INT,;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE t RENAME COLUMN old_name new_name;",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE t ADD c INT AFTER;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE t ALGORITHM=MERGE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE t LOCK=WRITE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE t ADD c INT FIRST AFTER id;", MYLITE_SQL_PARSE_SYNTAX_ERROR,
                          &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE t ADD c VARCHAR;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_key_constraint_operations_syntax(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *items = NULL;
    const struct mylite_sql_ast_node *action = NULL;
    const struct mylite_sql_ast_node *index = NULL;
    const struct mylite_sql_ast_node *key_parts = NULL;
    const struct mylite_sql_ast_node *options = NULL;
    const struct mylite_sql_ast_node *reference = NULL;
    int failures = 0;

    failures += parse_sql("ALTER TABLE t ADD PRIMARY KEY USING BTREE (id, v(8) DESC) "
                          "COMMENT 'pk' INVISIBLE, "
                          "ADD CONSTRAINT uq_sym UNIQUE KEY uq_name USING HASH (email ASC) "
                          "VISIBLE, ADD INDEX idx_a (a), ADD KEY (b DESC) KEY_BLOCK_SIZE=8, "
                          "ADD FULLTEXT INDEX ft_body (body) WITH PARSER ngram COMMENT 'ft', "
                          "ADD SPATIAL KEY sp_g (g);",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    items = child_at(statement, 1U);
    failures += expect_child_count(items, 6U, "alter key add action count");

    action = child_at(items, 0U);
    failures += expect_alter_table_action(action, MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_PRIMARY_KEY,
                                          false, "alter add primary key action");
    index = child_at(action, 0U);
    failures +=
        expect_node(index, MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT, "alter add primary key node");
    key_parts = child_at(index, 1U);
    failures += expect_child_count(key_parts, 2U, "alter add primary key part count");
    failures += expect_key_part_order(child_at(key_parts, 1U), MYLITE_SQL_AST_KEY_PART_ORDER_DESC,
                                      "alter add primary desc part");
    options = child_at(index, 2U);
    failures += expect_index_option(child_at(options, 0U), MYLITE_SQL_AST_INDEX_OPTION_COMMENT,
                                    "alter add primary comment");
    failures += expect_index_option(child_at(options, 1U), MYLITE_SQL_AST_INDEX_OPTION_INVISIBLE,
                                    "alter add primary invisible");

    action = child_at(items, 1U);
    failures +=
        expect_alter_table_action(action, MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_UNIQUE_INDEX, false,
                                  "alter add unique action");
    index = child_at(action, 0U);
    failures += expect_node(index, MYLITE_SQL_AST_UNIQUE_INDEX, "alter add unique node");
    failures += expect_span_text(child_at(index, 0U), "uq_sym", "alter add unique constraint");
    failures += expect_span_text(child_at(index, 1U), "uq_name", "alter add unique index name");
    failures += expect_index_algorithm(child_at(index, 2U), MYLITE_SQL_AST_INDEX_ALGORITHM_HASH,
                                       "alter add unique index type");

    action = child_at(items, 2U);
    failures +=
        expect_alter_table_action(action, MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_SECONDARY_INDEX,
                                  false, "alter add index action");
    failures +=
        expect_span_text(child_at(child_at(action, 0U), 0U), "idx_a", "alter add index name");

    action = child_at(items, 3U);
    failures +=
        expect_alter_table_action(action, MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_SECONDARY_INDEX,
                                  false, "alter add key action");
    index = child_at(action, 0U);
    failures += expect_node(child_at(index, 0U), MYLITE_SQL_AST_KEY_PART_LIST,
                            "alter add unnamed key parts");
    failures +=
        expect_key_part_order(child_at(child_at(index, 0U), 0U), MYLITE_SQL_AST_KEY_PART_ORDER_DESC,
                              "alter add unnamed key desc");

    action = child_at(items, 4U);
    failures +=
        expect_alter_table_action(action, MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_FULLTEXT_INDEX,
                                  false, "alter add fulltext action");
    if (action != NULL && action->index_class != MYLITE_SQL_AST_INDEX_CLASS_FULLTEXT) {
        fprintf(stderr, "alter add fulltext index class was not recorded\n");
        failures = 1;
    }
    options = child_at(child_at(action, 0U), 2U);
    failures += expect_index_option(child_at(options, 0U), MYLITE_SQL_AST_INDEX_OPTION_WITH_PARSER,
                                    "alter fulltext parser option");

    action = child_at(items, 5U);
    failures +=
        expect_alter_table_action(action, MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_SPATIAL_INDEX,
                                  false, "alter add spatial action");
    if (action != NULL && action->index_class != MYLITE_SQL_AST_INDEX_CLASS_SPATIAL) {
        fprintf(stderr, "alter add spatial index class was not recorded\n");
        failures = 1;
    }
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE t DROP PRIMARY KEY, DROP INDEX idx, DROP KEY old_k, "
                          "RENAME INDEX old_idx TO new_idx, RENAME KEY old_key TO new_key, "
                          "ALTER INDEX idx INVISIBLE, ALTER INDEX idx2 VISIBLE;",
                          MYLITE_SQL_PARSE_OK, &result);
    items = child_at(child_at(result.root, 0U), 1U);
    failures += expect_child_count(items, 7U, "alter key maintenance action count");
    failures += expect_alter_table_action(child_at(items, 0U),
                                          MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_PRIMARY_KEY, false,
                                          "alter drop primary action");
    action = child_at(items, 1U);
    failures += expect_alter_table_action(action, MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_INDEX,
                                          false, "alter drop index action");
    if (action != NULL &&
        action->alter_table_index_spelling != MYLITE_SQL_AST_ALTER_TABLE_INDEX_SPELLING_INDEX) {
        fprintf(stderr, "alter DROP INDEX spelling was not recorded\n");
        failures = 1;
    }
    action = child_at(items, 2U);
    if (action != NULL &&
        action->alter_table_index_spelling != MYLITE_SQL_AST_ALTER_TABLE_INDEX_SPELLING_KEY) {
        fprintf(stderr, "alter DROP KEY spelling was not recorded\n");
        failures = 1;
    }
    failures += expect_alter_table_action(child_at(items, 3U),
                                          MYLITE_SQL_AST_ALTER_TABLE_ACTION_RENAME_INDEX, false,
                                          "alter rename index action");
    failures += expect_alter_table_action(child_at(items, 4U),
                                          MYLITE_SQL_AST_ALTER_TABLE_ACTION_RENAME_INDEX, false,
                                          "alter rename key action");
    action = child_at(items, 5U);
    failures +=
        expect_alter_table_action(action, MYLITE_SQL_AST_ALTER_TABLE_ACTION_ALTER_INDEX_VISIBILITY,
                                  false, "alter index invisible action");
    if (action != NULL && action->index_option != MYLITE_SQL_AST_INDEX_OPTION_INVISIBLE) {
        fprintf(stderr, "alter index invisible option was not recorded\n");
        failures = 1;
    }
    action = child_at(items, 6U);
    if (action != NULL && action->index_option != MYLITE_SQL_AST_INDEX_OPTION_VISIBLE) {
        fprintf(stderr, "alter index visible option was not recorded\n");
        failures = 1;
    }
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE child ADD CHECK (a > 0) NOT ENFORCED, "
                          "ADD CONSTRAINT chk_a CHECK (a <> 0) ENFORCED, "
                          "DROP CHECK chk_a, DROP CONSTRAINT chk_b, "
                          "ALTER CHECK chk_c NOT ENFORCED, ALTER CONSTRAINT chk_d ENFORCED, "
                          "ADD CONSTRAINT fk_pid FOREIGN KEY fk_pid_idx (pid, other_id) "
                          "REFERENCES parent (id, other_id) MATCH SIMPLE ON DELETE CASCADE "
                          "ON UPDATE SET NULL, DROP FOREIGN KEY fk_pid;",
                          MYLITE_SQL_PARSE_OK, &result);
    items = child_at(child_at(result.root, 0U), 1U);
    failures += expect_child_count(items, 8U, "alter constraint action count");
    action = child_at(items, 0U);
    failures += expect_alter_table_action(action, MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_CHECK,
                                          false, "alter add check action");
    if (action != NULL &&
        action->constraint_enforcement != MYLITE_SQL_AST_CONSTRAINT_ENFORCEMENT_NOT_ENFORCED) {
        fprintf(stderr, "alter add check NOT ENFORCED was not recorded\n");
        failures = 1;
    }
    action = child_at(items, 1U);
    if (action != NULL &&
        action->constraint_enforcement != MYLITE_SQL_AST_CONSTRAINT_ENFORCEMENT_ENFORCED) {
        fprintf(stderr, "alter add check ENFORCED was not recorded\n");
        failures = 1;
    }
    failures += expect_alter_table_action(
        child_at(items, 2U), MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_CHECK_OR_CONSTRAINT, false,
        "alter drop check action");
    failures += expect_alter_table_action(
        child_at(items, 3U), MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_CHECK_OR_CONSTRAINT, false,
        "alter drop constraint action");
    failures += expect_alter_table_action(
        child_at(items, 4U), MYLITE_SQL_AST_ALTER_TABLE_ACTION_ALTER_CHECK_OR_CONSTRAINT, false,
        "alter check enforcement action");
    failures += expect_alter_table_action(
        child_at(items, 5U), MYLITE_SQL_AST_ALTER_TABLE_ACTION_ALTER_CHECK_OR_CONSTRAINT, false,
        "alter constraint enforcement action");
    action = child_at(items, 6U);
    failures += expect_alter_table_action(action, MYLITE_SQL_AST_ALTER_TABLE_ACTION_ADD_FOREIGN_KEY,
                                          false, "alter add foreign key action");
    failures += expect_span_text(child_at(action, 0U), "fk_pid", "alter foreign constraint");
    failures += expect_span_text(child_at(action, 1U), "fk_pid_idx", "alter foreign index");
    failures += expect_child_count(child_at(action, 2U), 2U, "alter foreign child columns");
    reference = child_at(action, 3U);
    failures += expect_child_count(child_at(reference, 1U), 2U, "alter foreign parent columns");
    options = child_at(reference, 2U);
    failures += expect_child_count(options, 3U, "alter foreign reference option count");
    if (child_at(options, 0U) != NULL &&
        child_at(options, 0U)->reference_match != MYLITE_SQL_AST_REFERENCE_MATCH_SIMPLE) {
        fprintf(stderr, "alter foreign MATCH SIMPLE was not recorded\n");
        failures = 1;
    }
    if (child_at(options, 1U) != NULL &&
        child_at(options, 1U)->reference_action != MYLITE_SQL_AST_REFERENCE_ACTION_CASCADE) {
        fprintf(stderr, "alter foreign ON DELETE CASCADE was not recorded\n");
        failures = 1;
    }
    if (child_at(options, 2U) != NULL &&
        child_at(options, 2U)->reference_action != MYLITE_SQL_AST_REFERENCE_ACTION_SET_NULL) {
        fprintf(stderr, "alter foreign ON UPDATE SET NULL was not recorded\n");
        failures = 1;
    }
    failures += expect_alter_table_action(child_at(items, 7U),
                                          MYLITE_SQL_AST_ALTER_TABLE_ACTION_DROP_FOREIGN_KEY, false,
                                          "alter drop foreign key action");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE t ADD PRIMARY KEY ();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE t ADD INDEX idx (a,);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE t RENAME INDEX old_idx new_idx;",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("ALTER TABLE t ALTER CHECK chk_a;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE t ADD FOREIGN KEY fk (pid) REFERENCES parent ();",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP KEY idx ON t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_rename_table_syntax(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *pairs = NULL;
    const struct mylite_sql_ast_node *pair = NULL;
    const struct mylite_sql_ast_node *items = NULL;
    const struct mylite_sql_ast_node *action = NULL;
    int failures = 0;

    failures += parse_sql("RENAME TABLE app.old_name TO app.new_name, "
                          "`from` TO `to`, a TO b;",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    pairs = child_at(statement, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_RENAME_TABLE_STATEMENT, "rename table statement");
    failures += expect_node(pairs, MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST, "rename table pair list");
    failures += expect_child_count(pairs, 3U, "rename table pair count");
    pair = child_at(pairs, 0U);
    failures += expect_node(pair, MYLITE_SQL_AST_RENAME_TABLE_PAIR, "rename table pair");
    failures += expect_node(child_at(pair, 0U), MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "rename source qualified");
    failures += expect_span_text(child_at(child_at(pair, 0U), 0U), "app", "rename source schema");
    failures +=
        expect_span_text(child_at(child_at(pair, 0U), 1U), "old_name", "rename source table");
    failures += expect_node(child_at(pair, 1U), MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "rename target qualified");
    failures += expect_span_text(child_at(child_at(pair, 1U), 0U), "app", "rename target schema");
    failures +=
        expect_span_text(child_at(child_at(pair, 1U), 1U), "new_name", "rename target table");
    pair = child_at(pairs, 1U);
    failures += expect_span_text(child_at(pair, 0U), "`from`", "rename quoted source");
    failures += expect_span_text(child_at(pair, 1U), "`to`", "rename quoted target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE old_name RENAME new_name; "
                          "ALTER TABLE old_name RENAME TO app.new_name; "
                          "ALTER TABLE app.old_name RENAME AS new_name;",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 3U, "alter table rename script count");

    statement = child_at(result.root, 0U);
    items = child_at(statement, 1U);
    action = child_at(items, 0U);
    failures += expect_alter_table_action(action, MYLITE_SQL_AST_ALTER_TABLE_ACTION_RENAME_TABLE,
                                          false, "alter table rename bare action");
    failures += expect_span_text(child_at(action, 0U), "new_name", "alter table rename target");

    statement = child_at(result.root, 1U);
    items = child_at(statement, 1U);
    action = child_at(items, 0U);
    failures += expect_alter_table_action(action, MYLITE_SQL_AST_ALTER_TABLE_ACTION_RENAME_TABLE,
                                          false, "alter table rename to action");
    failures += expect_node(child_at(action, 0U), MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "alter table rename qualified target");

    statement = child_at(result.root, 2U);
    items = child_at(statement, 1U);
    action = child_at(items, 0U);
    failures += expect_alter_table_action(action, MYLITE_SQL_AST_ALTER_TABLE_ACTION_RENAME_TABLE,
                                          false, "alter table rename as action");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "alter table rename qualified source");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("ALTER TABLE t RENAME TABLE u;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("RENAME TABLE t TO;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("RENAME TABLE t TO u,;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("RENAME TABLE t u;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_truncate_table_syntax(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("TRUNCATE TABLE app.old_name; TRUNCATE old_name; "
                          "TRUNCATE TABLE `app`.`old`;",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 3U, "truncate script count");

    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT, "truncate table statement");
    failures += expect_child_count(statement, 1U, "truncate child count");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "truncate qualified target");
    failures +=
        expect_span_text(child_at(child_at(statement, 0U), 0U), "app", "truncate target schema");
    failures += expect_span_text(child_at(child_at(statement, 0U), 1U), "old_name",
                                 "truncate target table");

    statement = child_at(result.root, 1U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT, "truncate bare statement");
    failures +=
        expect_node(child_at(statement, 0U), MYLITE_SQL_AST_IDENTIFIER, "truncate bare target");
    failures += expect_span_text(child_at(statement, 0U), "old_name", "truncate bare table");

    statement = child_at(result.root, 2U);
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "truncate quoted qualified target");
    failures +=
        expect_span_text(child_at(child_at(statement, 0U), 0U), "`app`", "truncate quoted schema");
    failures +=
        expect_span_text(child_at(child_at(statement, 0U), 1U), "`old`", "truncate quoted table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE truncate (id INT); TRUNCATE TABLE truncate; "
                          "SELECT TRUNCATE(1.234, 2);",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 3U, "truncate keyword identifier script count");
    statement = child_at(result.root, 1U);
    failures += expect_node(statement, MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT,
                            "truncate keyword identifier statement");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_IDENTIFIER,
                            "truncate keyword identifier target");
    failures +=
        expect_span_text(child_at(statement, 0U), "truncate", "truncate keyword identifier table");
    statement = child_at(result.root, 2U);
    failures += expect_function_call(child_at(child_at(child_at(statement, 0U), 0U), 0U),
                                     "TRUNCATE", 2U, "truncate scalar function keyword call");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("TRUNCATE TABLE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("TRUNCATE TABLE t, u;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("TRUNCATE TABLE IF EXISTS t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("TRUNCATE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("TRUNCATE TABLE t TO u;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_variables_syntax(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW VARIABLES; SHOW GLOBAL VARIABLES; SHOW SESSION VARIABLES; "
                          "SHOW LOCAL VARIABLES; SHOW VARIABLES LIKE 'autocommit'; "
                          "SHOW GLOBAL VARIABLES LIKE 'character\\_set\\_%'; "
                          "SHOW SESSION VARIABLES WHERE Variable_name = 'autocommit'; "
                          "SHOW VARIABLES WHERE Value = 'ON';",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 8U, "show variables script count");

    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT, "show variables");
    failures += expect_child_count(statement, 0U, "show variables child count");
    if (statement != NULL &&
        statement->show_variables_scope != MYLITE_SQL_AST_SHOW_VARIABLES_SESSION) {
        fprintf(stderr, "show variables: expected default session scope\n");
        failures = 1;
    }

    statement = child_at(result.root, 1U);
    if (statement != NULL &&
        statement->show_variables_scope != MYLITE_SQL_AST_SHOW_VARIABLES_GLOBAL) {
        fprintf(stderr, "show global variables: expected global scope\n");
        failures = 1;
    }

    statement = child_at(result.root, 2U);
    if (statement != NULL &&
        statement->show_variables_scope != MYLITE_SQL_AST_SHOW_VARIABLES_SESSION) {
        fprintf(stderr, "show session variables: expected session scope\n");
        failures = 1;
    }

    statement = child_at(result.root, 3U);
    if (statement != NULL &&
        statement->show_variables_scope != MYLITE_SQL_AST_SHOW_VARIABLES_SESSION) {
        fprintf(stderr, "show local variables: expected session scope\n");
        failures = 1;
    }

    statement = child_at(result.root, 4U);
    failures += expect_child_count(statement, 1U, "show variables like child count");
    failures += expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING,
                               "show variables like pattern");
    failures += expect_span_text(child_at(statement, 0U), "'autocommit'",
                                 "show variables like pattern text");

    statement = child_at(result.root, 5U);
    failures += expect_child_count(statement, 1U, "show global variables like child count");
    failures += expect_span_text(child_at(statement, 0U), "'character\\_set\\_%'",
                                 "show global variables escaped pattern text");

    statement = child_at(result.root, 6U);
    failures += expect_child_count(statement, 1U, "show variables where child count");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "show variables where clause");

    statement = child_at(result.root, 7U);
    failures += expect_child_count(statement, 1U, "show variables where value child count");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "show variables where value clause");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE variables (global INT, session INT, local INT, "
                          "variables INT);",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 1U, "show variables keyword identifiers");
    failures += expect_span_text(child_at(child_at(result.root, 0U), 0U), "variables",
                                 "variables keyword as table name");
    statement = child_at(child_at(result.root, 0U), 1U);
    failures += expect_span_text(child_at(child_at(statement, 0U), 0U), "global",
                                 "global keyword as column name");
    failures += expect_span_text(child_at(child_at(statement, 1U), 0U), "session",
                                 "session keyword as column name");
    failures += expect_span_text(child_at(child_at(statement, 2U), 0U), "local",
                                 "local keyword as column name");
    failures += expect_span_text(child_at(child_at(statement, 3U), 0U), "variables",
                                 "variables keyword as column name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW VARIABLES LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW VARIABLES LIKE 'a%' WHERE TRUE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW VARIABLES LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW GLOBAL SESSION VARIABLES;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW VARIABLES GLOBAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_show_status_syntax(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW STATUS; SHOW GLOBAL STATUS; SHOW SESSION STATUS; "
                          "SHOW LOCAL STATUS; SHOW STATUS LIKE 'Uptime%'; "
                          "SHOW GLOBAL STATUS LIKE 'Com\\_%'; "
                          "SHOW SESSION STATUS WHERE Variable_name = 'Uptime'; "
                          "SHOW STATUS WHERE Value = '0';",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 8U, "show status script count");

    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_STATUS_STATEMENT, "show status");
    failures += expect_child_count(statement, 0U, "show status child count");
    if (statement != NULL && statement->show_status_scope != MYLITE_SQL_AST_SHOW_STATUS_SESSION) {
        fprintf(stderr, "show status: expected default session scope\n");
        failures = 1;
    }

    statement = child_at(result.root, 1U);
    if (statement != NULL && statement->show_status_scope != MYLITE_SQL_AST_SHOW_STATUS_GLOBAL) {
        fprintf(stderr, "show global status: expected global scope\n");
        failures = 1;
    }

    statement = child_at(result.root, 2U);
    if (statement != NULL && statement->show_status_scope != MYLITE_SQL_AST_SHOW_STATUS_SESSION) {
        fprintf(stderr, "show session status: expected session scope\n");
        failures = 1;
    }

    statement = child_at(result.root, 3U);
    if (statement != NULL && statement->show_status_scope != MYLITE_SQL_AST_SHOW_STATUS_SESSION) {
        fprintf(stderr, "show local status: expected session scope\n");
        failures = 1;
    }

    statement = child_at(result.root, 4U);
    failures += expect_child_count(statement, 1U, "show status like child count");
    failures += expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING,
                               "show status like pattern");
    failures +=
        expect_span_text(child_at(statement, 0U), "'Uptime%'", "show status like pattern text");

    statement = child_at(result.root, 5U);
    failures += expect_child_count(statement, 1U, "show global status like child count");
    failures += expect_span_text(child_at(statement, 0U), "'Com\\_%'",
                                 "show global status escaped pattern text");

    statement = child_at(result.root, 6U);
    failures += expect_child_count(statement, 1U, "show status where child count");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "show status where clause");

    statement = child_at(result.root, 7U);
    failures += expect_child_count(statement, 1U, "show status where value child count");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "show status where value clause");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE status (global INT, session INT, local INT, status INT);",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 1U, "show status keyword identifiers");
    failures += expect_span_text(child_at(child_at(result.root, 0U), 0U), "status",
                                 "status keyword as table name");
    statement = child_at(child_at(result.root, 0U), 1U);
    failures += expect_span_text(child_at(child_at(statement, 0U), 0U), "global",
                                 "global keyword as status column name");
    failures += expect_span_text(child_at(child_at(statement, 1U), 0U), "session",
                                 "session keyword as status column name");
    failures += expect_span_text(child_at(child_at(statement, 2U), 0U), "local",
                                 "local keyword as status column name");
    failures += expect_span_text(child_at(child_at(statement, 3U), 0U), "status",
                                 "status keyword as column name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW STATUS LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW STATUS LIKE 'a%' WHERE TRUE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW STATUS LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW LOCAL GLOBAL STATUS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW STATUS GLOBAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_show_engines_syntax(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW ENGINES; SHOW STORAGE ENGINES;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 2U, "show engines script count");

    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT, "show engines");
    failures += expect_child_count(statement, 0U, "show engines child count");

    statement = child_at(result.root, 1U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT, "show storage engines");
    failures += expect_child_count(statement, 0U, "show storage engines child count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("CREATE TABLE engines (storage INT, engines INT);", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 1U, "show engines keyword identifiers");
    statement = child_at(result.root, 0U);
    failures +=
        expect_span_text(child_at(statement, 0U), "engines", "engines keyword as table name");
    statement = child_at(statement, 1U);
    failures += expect_span_text(child_at(child_at(statement, 0U), 0U), "storage",
                                 "storage keyword as column name");
    failures += expect_span_text(child_at(child_at(statement, 1U), 0U), "engines",
                                 "engines keyword as column name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ENGINES LIKE 'InnoDB';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW ENGINES WHERE Engine = 'InnoDB';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ENGINES LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW STORAGE ENGINES LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW STORAGE ENGINES LIKE 'InnoDB';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW STORAGE ENGINES WHERE Engine = 'InnoDB';",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_character_set_syntax(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW CHARACTER SET; SHOW CHARSET; SHOW CHAR SET; "
                          "SHOW CHARACTER SET LIKE 'utf8%'; "
                          "SHOW CHARSET LIKE 'binary'; "
                          "SHOW CHAR SET WHERE Charset = 'utf8mb4'; "
                          "SHOW CHARACTER SET WHERE `Default collation` = 'binary';",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 7U, "show character set script count");

    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT, "show character set");
    failures += expect_child_count(statement, 0U, "show character set child count");

    statement = child_at(result.root, 1U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT, "show charset");
    failures += expect_child_count(statement, 0U, "show charset child count");

    statement = child_at(result.root, 2U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT, "show char set");
    failures += expect_child_count(statement, 0U, "show char set child count");

    statement = child_at(result.root, 3U);
    failures += expect_child_count(statement, 1U, "show character set like child count");
    failures += expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING,
                               "show character set like pattern");
    failures += expect_span_text(child_at(statement, 0U), "'utf8%'",
                                 "show character set like pattern text");

    statement = child_at(result.root, 4U);
    failures += expect_child_count(statement, 1U, "show charset like child count");
    failures +=
        expect_span_text(child_at(statement, 0U), "'binary'", "show charset like pattern text");

    statement = child_at(result.root, 5U);
    failures += expect_child_count(statement, 1U, "show char set where child count");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "show char set where clause");

    statement = child_at(result.root, 6U);
    failures += expect_child_count(statement, 1U, "show character set where child count");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "show character set where default collation clause");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE charset (charset INT);", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 1U, "show charset keyword identifiers");
    failures += expect_span_text(child_at(child_at(result.root, 0U), 0U), "charset",
                                 "charset keyword as table name");
    statement = child_at(child_at(result.root, 0U), 1U);
    failures += expect_span_text(child_at(child_at(statement, 0U), 0U), "charset",
                                 "charset keyword as column name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CHARACTER SET LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CHARACTER SET LIKE 'a%' WHERE TRUE;", MYLITE_SQL_PARSE_SYNTAX_ERROR,
                          &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CHARACTER SET LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_show_collation_syntax(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW COLLATION; "
                          "SHOW COLLATION LIKE 'utf8mb4%'; "
                          "SHOW COLLATION WHERE Charset = 'utf8mb4'; "
                          "SHOW COLLATION WHERE `Default` = 'Yes';",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 4U, "show collation script count");

    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT, "show collation");
    failures += expect_child_count(statement, 0U, "show collation child count");

    statement = child_at(result.root, 1U);
    failures += expect_child_count(statement, 1U, "show collation like child count");
    failures += expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING,
                               "show collation like pattern");
    failures +=
        expect_span_text(child_at(statement, 0U), "'utf8mb4%'", "show collation like pattern text");

    statement = child_at(result.root, 2U);
    failures += expect_child_count(statement, 1U, "show collation where child count");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "show collation where charset clause");

    statement = child_at(result.root, 3U);
    failures += expect_child_count(statement, 1U, "show collation where default child count");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "show collation where default clause");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLLATION LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW COLLATION LIKE 'a%' WHERE TRUE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLLATION LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE show_collation_identifier (collation INT);",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_tables_syntax(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW TABLES; SHOW FULL TABLES; SHOW TABLES FROM app; "
                          "SHOW TABLES IN app LIKE 'alpha%'; "
                          "SHOW FULL TABLES FROM `select` LIKE 'beta\\_%'; "
                          "SHOW TABLES LIKE 'solo%'; SHOW EXTENDED TABLES; "
                          "SHOW EXTENDED FULL TABLES; "
                          "SHOW FULL TABLES WHERE Table_type = 'BASE TABLE';",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 9U, "show tables script count");

    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_TABLES_STATEMENT, "show tables");
    failures += expect_bool(statement->show_tables_full, false, "show tables full marker");
    failures += expect_child_count(statement, 0U, "show tables child count");

    statement = child_at(result.root, 1U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_TABLES_STATEMENT, "show full tables");
    failures += expect_bool(statement->show_tables_full, true, "show full tables full marker");
    failures += expect_child_count(statement, 0U, "show full tables child count");

    statement = child_at(result.root, 2U);
    failures +=
        expect_node(child_at(statement, 0U), MYLITE_SQL_AST_IDENTIFIER, "show tables from schema");
    failures += expect_span_text(child_at(statement, 0U), "app", "show tables from schema name");

    statement = child_at(result.root, 3U);
    failures += expect_child_count(statement, 2U, "show tables in like child count");
    failures += expect_span_text(child_at(statement, 0U), "app", "show tables in schema name");
    failures += expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING,
                               "show tables like pattern");
    failures +=
        expect_span_text(child_at(statement, 1U), "'alpha%'", "show tables like pattern text");

    statement = child_at(result.root, 4U);
    failures +=
        expect_bool(statement->show_tables_full, true, "show full tables qualified full marker");
    failures +=
        expect_span_text(child_at(statement, 0U), "`select`", "show full tables quoted schema");
    failures +=
        expect_span_text(child_at(statement, 1U), "'beta\\_%'", "show full tables escaped pattern");

    statement = child_at(result.root, 5U);
    failures += expect_child_count(statement, 1U, "show tables like-only child count");
    failures += expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING,
                               "show tables like-only pattern");

    statement = child_at(result.root, 6U);
    failures += expect_bool(statement->show_tables_extended, true, "show extended tables marker");
    failures += expect_bool(statement->show_tables_full, false, "show extended tables full marker");
    failures += expect_child_count(statement, 0U, "show extended tables child count");

    statement = child_at(result.root, 7U);
    failures +=
        expect_bool(statement->show_tables_extended, true, "show extended full tables marker");
    failures += expect_bool(statement->show_tables_full, true, "show extended full tables marker");
    failures += expect_child_count(statement, 0U, "show extended full tables child count");

    statement = child_at(result.root, 8U);
    failures += expect_bool(statement->show_tables_full, true, "show full tables where marker");
    failures += expect_child_count(statement, 1U, "show full tables where child count");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "show full tables where clause");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE tables (id INT); CREATE TABLE full (id INT); "
                          "CREATE TABLE extended (id INT);",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 3U, "show tables keyword identifiers");
    failures += expect_span_text(child_at(child_at(result.root, 0U), 0U), "tables",
                                 "tables keyword as table name");
    failures += expect_span_text(child_at(child_at(result.root, 1U), 0U), "full",
                                 "full keyword as table name");
    failures += expect_span_text(child_at(child_at(result.root, 2U), 0U), "extended",
                                 "extended keyword as table name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLES LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW TABLES LIKE 'a%' WHERE TRUE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL FULL TABLES;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_show_table_status_syntax(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW TABLE STATUS; SHOW TABLE STATUS FROM app; "
                          "SHOW TABLE STATUS IN app LIKE 'alpha%'; "
                          "SHOW TABLE STATUS FROM `select` LIKE 'beta\\_%'; "
                          "SHOW TABLE STATUS LIKE 'solo%'; "
                          "SHOW TABLE STATUS WHERE Name = 'simple';",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 6U, "show table status script count");

    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT, "show table status");
    failures += expect_child_count(statement, 0U, "show table status child count");

    statement = child_at(result.root, 1U);
    failures += expect_child_count(statement, 1U, "show table status from child count");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_IDENTIFIER,
                            "show table status from schema");
    failures +=
        expect_span_text(child_at(statement, 0U), "app", "show table status from schema name");

    statement = child_at(result.root, 2U);
    failures += expect_child_count(statement, 2U, "show table status in like child count");
    failures +=
        expect_span_text(child_at(statement, 0U), "app", "show table status in schema name");
    failures += expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING,
                               "show table status like pattern");
    failures += expect_span_text(child_at(statement, 1U), "'alpha%'",
                                 "show table status like pattern text");

    statement = child_at(result.root, 3U);
    failures +=
        expect_span_text(child_at(statement, 0U), "`select`", "show table status quoted schema");
    failures += expect_span_text(child_at(statement, 1U), "'beta\\_%'",
                                 "show table status escaped pattern");

    statement = child_at(result.root, 4U);
    failures += expect_child_count(statement, 1U, "show table status like-only child count");
    failures += expect_literal(child_at(statement, 0U), MYLITE_SQL_AST_LITERAL_STRING,
                               "show table status like-only pattern");

    statement = child_at(result.root, 5U);
    failures += expect_child_count(statement, 1U, "show table status where child count");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "show table status where clause");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLE STATUS LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TABLE STATUS LIKE 'a%' WHERE Name = 'a';",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL TABLE STATUS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW EXTENDED TABLE STATUS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW TABLE STATUS FROM app IN other;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW TABLE STATUS FROM app FROM other;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW TABLE STATUS IN app FROM other;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_show_columns_syntax(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *table_name = NULL;
    int failures = 0;

    failures += parse_sql("SHOW COLUMNS FROM t; SHOW FIELDS IN t; SHOW FULL COLUMNS FROM t; "
                          "SHOW FULL FIELDS FROM t FROM app; "
                          "SHOW EXTENDED COLUMNS FROM t; "
                          "SHOW EXTENDED FULL FIELDS IN app.t LIKE 'a%'; "
                          "SHOW COLUMNS FROM app.t; "
                          "SHOW COLUMNS FROM t WHERE Field = 'name'; "
                          "SHOW FULL COLUMNS FROM t FROM app LIKE 'id%';",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 9U, "show columns script count");

    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "show columns");
    failures += expect_bool(statement->show_columns_full, false, "show columns full marker");
    failures +=
        expect_bool(statement->show_columns_extended, false, "show columns extended marker");
    failures += expect_child_count(statement, 1U, "show columns child count");
    failures += expect_span_text(child_at(statement, 0U), "t", "show columns table name");

    statement = child_at(result.root, 1U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, "show fields");
    failures += expect_child_count(statement, 1U, "show fields child count");
    failures += expect_span_text(child_at(statement, 0U), "t", "show fields table name");

    statement = child_at(result.root, 2U);
    failures += expect_bool(statement->show_columns_full, true, "show full columns marker");
    failures +=
        expect_bool(statement->show_columns_extended, false, "show full columns extended marker");
    failures += expect_child_count(statement, 1U, "show full columns child count");

    statement = child_at(result.root, 3U);
    failures += expect_bool(statement->show_columns_full, true, "show full fields marker");
    failures += expect_child_count(statement, 2U, "show full fields child count");
    failures += expect_span_text(child_at(statement, 0U), "t", "show full fields table");
    failures += expect_span_text(child_at(statement, 1U), "app", "show full fields schema");

    statement = child_at(result.root, 4U);
    failures += expect_bool(statement->show_columns_extended, true, "show extended columns marker");
    failures +=
        expect_bool(statement->show_columns_full, false, "show extended columns full marker");

    statement = child_at(result.root, 5U);
    failures +=
        expect_bool(statement->show_columns_extended, true, "show extended full fields marker");
    failures +=
        expect_bool(statement->show_columns_full, true, "show extended full fields full marker");
    failures += expect_child_count(statement, 2U, "show extended full fields child count");
    table_name = child_at(statement, 0U);
    failures += expect_node(table_name, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "show columns qualified table");
    failures += expect_span_text(child_at(table_name, 0U), "app", "show columns qualified schema");
    failures += expect_span_text(child_at(table_name, 1U), "t", "show columns qualified table");
    failures += expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING,
                               "show columns like pattern");
    failures += expect_span_text(child_at(statement, 1U), "'a%'", "show columns like pattern text");

    statement = child_at(result.root, 6U);
    table_name = child_at(statement, 0U);
    failures += expect_node(table_name, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "show columns db table target");
    failures += expect_span_text(child_at(table_name, 0U), "app", "show columns db target schema");
    failures += expect_span_text(child_at(table_name, 1U), "t", "show columns db target table");

    statement = child_at(result.root, 7U);
    failures += expect_child_count(statement, 2U, "show columns where child count");
    failures += expect_node(child_at(statement, 1U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "show columns where clause");

    statement = child_at(result.root, 8U);
    failures += expect_bool(statement->show_columns_full, true,
                            "show columns explicit schema like full marker");
    failures += expect_child_count(statement, 3U, "show columns explicit schema like child count");
    failures +=
        expect_span_text(child_at(statement, 0U), "t", "show columns explicit schema like table");
    failures += expect_span_text(child_at(statement, 1U), "app",
                                 "show columns explicit schema like schema");
    failures += expect_literal(child_at(statement, 2U), MYLITE_SQL_AST_LITERAL_STRING,
                               "show columns explicit schema like pattern");
    failures += expect_span_text(child_at(statement, 2U), "'id%'",
                                 "show columns explicit schema like pattern text");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE columns (fields INT, columns INT, extended INT, "
                          "full INT);",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 1U, "show columns keyword identifiers");
    failures += expect_span_text(child_at(child_at(result.root, 0U), 0U), "columns",
                                 "columns keyword as table name");
    table_name = child_at(child_at(result.root, 0U), 1U);
    failures += expect_span_text(child_at(child_at(table_name, 0U), 0U), "fields",
                                 "fields keyword as column name");
    failures += expect_span_text(child_at(child_at(table_name, 1U), 0U), "columns",
                                 "columns keyword as column name");
    failures += expect_span_text(child_at(child_at(table_name, 2U), 0U), "extended",
                                 "extended keyword as column name");
    failures += expect_span_text(child_at(child_at(table_name, 3U), 0U), "full",
                                 "full keyword as column name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLUMNS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLUMNS FROM t LIKE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLUMNS FROM t LIKE 'a%' WHERE TRUE;",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL FULL COLUMNS FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW COLUMNS FROM t FROM app FROM other;", MYLITE_SQL_PARSE_SYNTAX_ERROR,
                          &result);
    mylite_sql_parse_result_deinit(&result);

    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_show_index_syntax(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *table_name = NULL;
    int failures = 0;

    failures += parse_sql("SHOW INDEX FROM t; SHOW INDEXES IN t; SHOW KEYS FROM t; "
                          "SHOW EXTENDED INDEX FROM t; "
                          "SHOW INDEX FROM app.t; "
                          "SHOW INDEX FROM t FROM app; "
                          "SHOW INDEX IN t WHERE Key_name = 'PRIMARY'; "
                          "SHOW EXTENDED KEYS FROM app.t IN other;",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 8U, "show index script count");

    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, "show index");
    failures += expect_bool(statement->show_index_extended, false, "show index extended marker");
    failures += expect_child_count(statement, 1U, "show index child count");
    failures += expect_span_text(child_at(statement, 0U), "t", "show index table name");

    statement = child_at(result.root, 1U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, "show indexes");
    failures += expect_child_count(statement, 1U, "show indexes child count");
    failures += expect_span_text(child_at(statement, 0U), "t", "show indexes table name");

    statement = child_at(result.root, 2U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, "show keys");
    failures += expect_child_count(statement, 1U, "show keys child count");
    failures += expect_span_text(child_at(statement, 0U), "t", "show keys table name");

    statement = child_at(result.root, 3U);
    failures += expect_bool(statement->show_index_extended, true, "show extended index marker");
    failures += expect_child_count(statement, 1U, "show extended index child count");

    statement = child_at(result.root, 4U);
    table_name = child_at(statement, 0U);
    failures +=
        expect_node(table_name, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "show index db table target");
    failures += expect_span_text(child_at(table_name, 0U), "app", "show index db target schema");
    failures += expect_span_text(child_at(table_name, 1U), "t", "show index db target table");

    statement = child_at(result.root, 5U);
    failures += expect_child_count(statement, 2U, "show index explicit schema child count");
    failures += expect_span_text(child_at(statement, 0U), "t", "show index explicit table");
    failures += expect_span_text(child_at(statement, 1U), "app", "show index explicit schema");

    statement = child_at(result.root, 6U);
    failures += expect_child_count(statement, 2U, "show index where child count");
    failures += expect_node(child_at(statement, 1U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "show index where clause");

    statement = child_at(result.root, 7U);
    failures += expect_bool(statement->show_index_extended, true,
                            "show extended keys explicit schema marker");
    failures += expect_child_count(statement, 2U, "show extended keys explicit schema child count");
    table_name = child_at(statement, 0U);
    failures += expect_node(table_name, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "show extended keys qualified table");
    failures +=
        expect_span_text(child_at(table_name, 0U), "app", "show extended keys qualified schema");
    failures +=
        expect_span_text(child_at(table_name, 1U), "t", "show extended keys qualified table");
    failures +=
        expect_span_text(child_at(statement, 1U), "other", "show extended keys explicit schema");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE indexes (indexes INT, KEY indexes (indexes)); "
                          "CREATE TABLE `keys` (`keys` INT, KEY `keys` (`keys`));",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 2U, "show index keyword identifiers");
    failures += expect_span_text(child_at(child_at(result.root, 0U), 0U), "indexes",
                                 "indexes keyword as table name");
    table_name = child_at(child_at(result.root, 0U), 1U);
    failures += expect_span_text(child_at(child_at(table_name, 0U), 0U), "indexes",
                                 "indexes keyword as column name");
    failures += expect_span_text(child_at(child_at(result.root, 1U), 0U), "`keys`",
                                 "quoted keys table name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW INDEX;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL INDEX FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW INDEX FROM t LIKE 'a%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW KEYS FROM keys;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE keys (id INT);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW INDEX FROM t FROM app FROM other;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_show_create_database_syntax(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SHOW CREATE DATABASE app; SHOW CREATE SCHEMA app; "
                          "SHOW CREATE DATABASE IF NOT EXISTS `My``Show``Db`; "
                          "SHOW CREATE SCHEMA IF NOT EXISTS app;",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 4U, "show create database script count");

    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_CREATE_SCHEMA_STATEMENT, "show create database");
    failures += expect_child_count(statement, 1U, "show create database child count");
    failures += expect_span_text(child_at(statement, 0U), "app", "show create database name");
    failures += expect_bool(statement->show_create_schema_if_not_exists, false,
                            "show create database no if not exists");

    statement = child_at(result.root, 1U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_CREATE_SCHEMA_STATEMENT, "show create schema");
    failures += expect_span_text(child_at(statement, 0U), "app", "show create schema name");

    statement = child_at(result.root, 2U);
    failures += expect_span_text(child_at(statement, 0U), "`My``Show``Db`",
                                 "show create database escaped name");
    failures += expect_bool(statement->show_create_schema_if_not_exists, true,
                            "show create database if not exists");

    statement = child_at(result.root, 3U);
    failures +=
        expect_span_text(child_at(statement, 0U), "app", "show create schema if not exists name");
    failures += expect_bool(statement->show_create_schema_if_not_exists, true,
                            "show create schema if not exists");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE DATABASE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE SCHEMA;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW CREATE DATABASE IF NOT EXISTS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW CREATE SCHEMA IF NOT EXISTS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW CREATE DATABASE app LIKE 'a%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW CREATE DATABASE app WHERE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW CREATE SCHEMA app LIKE 'a%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW CREATE SCHEMA app WHERE 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE VIEW v;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_show_create_table_syntax(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *table_name = NULL;
    int failures = 0;

    failures += parse_sql("SHOW CREATE TABLE t; SHOW CREATE TABLE app.t; "
                          "SHOW CREATE TABLE `weird``name`;",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 3U, "show create table script count");

    statement = child_at(result.root, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT, "show create table");
    failures += expect_child_count(statement, 1U, "show create table child count");
    failures += expect_span_text(child_at(statement, 0U), "t", "show create table name");

    statement = child_at(result.root, 1U);
    table_name = child_at(statement, 0U);
    failures += expect_node(table_name, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "show create table qualified target");
    failures +=
        expect_span_text(child_at(table_name, 0U), "app", "show create table qualified schema");
    failures +=
        expect_span_text(child_at(table_name, 1U), "t", "show create table qualified table");

    statement = child_at(result.root, 2U);
    failures += expect_span_text(child_at(statement, 0U), "`weird``name`",
                                 "show create table escaped name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE TABLE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE VIEW t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW FULL CREATE TABLE t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE TABLE t LIKE 'a%';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW CREATE TABLE t FROM app;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_show_diagnostics_syntax(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *limit = NULL;
    int failures = 0;

    failures += parse_sql("SHOW WARNINGS; SHOW ERRORS; SHOW COUNT(*) WARNINGS; "
                          "SHOW COUNT(*) ERRORS; SHOW WARNINGS LIMIT 2; "
                          "SHOW WARNINGS LIMIT 1, 2; "
                          "SHOW WARNINGS LIMIT 2 OFFSET 1; SHOW ERRORS LIMIT 3; "
                          "SHOW ERRORS LIMIT 1, 3; SHOW ERRORS LIMIT 3 OFFSET 1;",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 10U, "show diagnostics script count");

    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_DIAGNOSTICS_STATEMENT, "show warnings");
    failures += expect_child_count(statement, 0U, "show warnings child count");
    if (statement != NULL &&
        statement->show_diagnostics_kind != MYLITE_SQL_AST_SHOW_DIAGNOSTICS_WARNINGS) {
        fprintf(stderr, "show warnings: expected warnings diagnostic kind\n");
        failures = 1;
    }

    statement = child_at(result.root, 1U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_DIAGNOSTICS_STATEMENT, "show errors");
    if (statement != NULL &&
        statement->show_diagnostics_kind != MYLITE_SQL_AST_SHOW_DIAGNOSTICS_ERRORS) {
        fprintf(stderr, "show errors: expected errors diagnostic kind\n");
        failures = 1;
    }

    statement = child_at(result.root, 2U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_DIAGNOSTICS_COUNT_STATEMENT,
                            "show count warnings");
    failures += expect_child_count(statement, 0U, "show count warnings child count");
    if (statement != NULL &&
        statement->show_diagnostics_kind != MYLITE_SQL_AST_SHOW_DIAGNOSTICS_WARNINGS) {
        fprintf(stderr, "show count warnings: expected warnings diagnostic kind\n");
        failures = 1;
    }

    statement = child_at(result.root, 3U);
    failures += expect_node(statement, MYLITE_SQL_AST_SHOW_DIAGNOSTICS_COUNT_STATEMENT,
                            "show count errors");
    if (statement != NULL &&
        statement->show_diagnostics_kind != MYLITE_SQL_AST_SHOW_DIAGNOSTICS_ERRORS) {
        fprintf(stderr, "show count errors: expected errors diagnostic kind\n");
        failures = 1;
    }

    statement = child_at(result.root, 4U);
    limit = child_at(statement, 0U);
    failures += expect_node(limit, MYLITE_SQL_AST_LIMIT_CLAUSE, "show warnings row-count limit");
    failures += expect_limit_bound(child_at(limit, 0U), 0U, "show warnings default offset");
    failures += expect_limit_bound(child_at(limit, 1U), 2U, "show warnings row count");

    statement = child_at(result.root, 5U);
    limit = child_at(statement, 0U);
    failures += expect_limit_bound(child_at(limit, 0U), 1U, "show warnings comma offset");
    failures += expect_limit_bound(child_at(limit, 1U), 2U, "show warnings comma row count");

    statement = child_at(result.root, 6U);
    limit = child_at(statement, 0U);
    failures += expect_limit_bound(child_at(limit, 0U), 1U, "show warnings offset keyword");
    failures += expect_limit_bound(child_at(limit, 1U), 2U, "show warnings offset row count");

    statement = child_at(result.root, 7U);
    limit = child_at(statement, 0U);
    failures += expect_limit_bound(child_at(limit, 0U), 0U, "show errors default offset");
    failures += expect_limit_bound(child_at(limit, 1U), 3U, "show errors row count");

    statement = child_at(result.root, 8U);
    limit = child_at(statement, 0U);
    failures += expect_limit_bound(child_at(limit, 0U), 1U, "show errors comma offset");
    failures += expect_limit_bound(child_at(limit, 1U), 3U, "show errors comma row count");

    statement = child_at(result.root, 9U);
    limit = child_at(statement, 0U);
    failures += expect_limit_bound(child_at(limit, 0U), 1U, "show errors offset keyword");
    failures += expect_limit_bound(child_at(limit, 1U), 3U, "show errors offset row count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW WARNINGS LIKE 'x';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW ERRORS WHERE Code = 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SHOW COUNT(*) WARNINGS LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW WARNINGS LIMIT -1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW WARNINGS LIMIT '1';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SHOW TOTAL(*) WARNINGS;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE warnings (errors INT);", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 1U, "show diagnostics keyword identifiers");
    failures += expect_span_text(child_at(child_at(result.root, 0U), 0U), "warnings",
                                 "warnings keyword as table name");
    failures +=
        expect_span_text(child_at(child_at(child_at(child_at(result.root, 0U), 1U), 0U), 0U),
                         "errors", "errors keyword as column name");
    mylite_sql_parse_result_deinit(&result);

    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_describe_table_syntax(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *table_name = NULL;
    int failures = 0;

    failures += parse_sql("DESCRIBE t; DESC t; EXPLAIN t; DESCRIBE app.t; "
                          "DESC app.t name; DESCRIBE t `name`; "
                          "DESCRIBE t 'a%'; EXPLAIN t 'a\\_%';",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 8U, "describe table script count");

    statement = child_at(result.root, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT, "describe table");
    failures += expect_child_count(statement, 1U, "describe table child count");
    failures += expect_span_text(child_at(statement, 0U), "t", "describe table name");

    statement = child_at(result.root, 1U);
    failures += expect_node(statement, MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT, "desc table");
    failures += expect_child_count(statement, 1U, "desc table child count");

    statement = child_at(result.root, 2U);
    failures += expect_node(statement, MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT, "explain table");
    failures += expect_child_count(statement, 1U, "explain table child count");

    statement = child_at(result.root, 3U);
    table_name = child_at(statement, 0U);
    failures +=
        expect_node(table_name, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "describe qualified table");
    failures += expect_span_text(child_at(table_name, 0U), "app", "describe qualified schema");
    failures += expect_span_text(child_at(table_name, 1U), "t", "describe qualified table");

    statement = child_at(result.root, 4U);
    failures += expect_child_count(statement, 2U, "desc column child count");
    table_name = child_at(statement, 0U);
    failures += expect_node(table_name, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "desc qualified table with column");
    failures += expect_span_text(child_at(statement, 1U), "name", "desc column filter");

    statement = child_at(result.root, 5U);
    failures += expect_child_count(statement, 2U, "describe quoted column child count");
    failures += expect_span_text(child_at(statement, 1U), "`name`", "describe quoted column");

    statement = child_at(result.root, 6U);
    failures += expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING,
                               "describe literal wildcard");
    failures += expect_span_text(child_at(statement, 1U), "'a%'", "describe wildcard text");

    statement = child_at(result.root, 7U);
    failures += expect_literal(child_at(statement, 1U), MYLITE_SQL_AST_LITERAL_STRING,
                               "explain literal wildcard");
    failures += expect_span_text(child_at(statement, 1U), "'a\\_%'", "explain wildcard text");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DESCRIBE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("DESCRIBE t WHERE Field = 'name';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("EXPLAIN FORMAT=TREE SELECT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a FROM t ORDER BY a DESC; "
                          "CREATE TABLE order_desc (a INT, KEY idx_a (a DESC)); "
                          "UPDATE t SET a = 1 ORDER BY a DESC LIMIT 1; "
                          "DELETE FROM t ORDER BY a DESC LIMIT 1;",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(result.root, 4U, "desc order direction regressions");
    mylite_sql_parse_result_deinit(&result);

    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_drop_table_syntax(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *names = NULL;
    int failures = 0;

    failures += parse_sql("DROP TABLE app.one, two;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    names = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_TABLE_STATEMENT, "drop table statement");
    failures += expect_node(names, MYLITE_SQL_AST_TABLE_NAME_LIST, "drop table names");
    failures += expect_child_count(names, 2U, "drop table name count");
    failures += expect_node(child_at(names, 0U), MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "qualified drop table name");
    failures += expect_span_text(child_at(child_at(names, 0U), 0U), "app", "drop schema name");
    failures += expect_span_text(child_at(child_at(names, 0U), 1U), "one", "drop table name");
    failures += expect_span_text(child_at(names, 1U), "two", "unqualified drop table name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP TEMPORARY TABLE IF EXISTS `restrict` CASCADE;", MYLITE_SQL_PARSE_OK,
                          &result);
    statement = child_at(result.root, 0U);
    names = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DROP_TABLE_STATEMENT,
                            "drop temporary table statement");
    failures +=
        expect_node(child_at(statement, 1U), MYLITE_SQL_AST_IF_EXISTS, "drop table if exists");
    failures += expect_child_count(names, 1U, "drop temporary table name count");
    failures += expect_span_text(child_at(names, 0U), "`restrict`", "quoted reserved table name");
    if (!statement->drop_table_temporary) {
        fprintf(stderr, "DROP TEMPORARY TABLE did not record temporary mode\n");
        failures = 1;
    }
    if (!statement->drop_table_cascade) {
        fprintf(stderr, "DROP TABLE CASCADE did not record cascade mode\n");
        failures = 1;
    }
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP TABLE IF EXISTS x, y RESTRICT;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    if (!statement->drop_table_restrict) {
        fprintf(stderr, "DROP TABLE RESTRICT did not record restrict mode\n");
        failures = 1;
    }
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("DROP TABLE IF EXISTS trailing,;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP TABLE restrict;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP TABLE cascade;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP TABLE temporary;", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    names = child_at(statement, 0U);
    failures += expect_span_text(child_at(names, 0U), "temporary", "nonreserved temporary name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DROP TABLE ;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_insert_values_syntax(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *rows = NULL;
    const struct mylite_sql_ast_node *row = NULL;
    int failures = 0;

    failures +=
        parse_sql("INSERT app.t VALUES (1, 'a'), (DEFAULT, NULL);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    rows = child_at(statement, 1U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_INSERT_VALUES_STATEMENT, "insert values statement");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "insert qualified table name");
    failures += expect_child_count(statement, 2U, "insert without column list children");
    failures += expect_node(rows, MYLITE_SQL_AST_INSERT_ROW_LIST, "insert row list");
    failures += expect_child_count(rows, 2U, "insert row list count");
    row = child_at(rows, 0U);
    failures += expect_node(row, MYLITE_SQL_AST_INSERT_ROW, "insert first row");
    failures += expect_child_count(row, 2U, "insert first row value count");
    failures +=
        expect_literal(child_at(row, 0U), MYLITE_SQL_AST_LITERAL_INTEGER, "insert integer value");
    failures +=
        expect_literal(child_at(row, 1U), MYLITE_SQL_AST_LITERAL_STRING, "insert string value");
    row = child_at(rows, 1U);
    failures += expect_node(child_at(row, 0U), MYLITE_SQL_AST_DEFAULT, "insert DEFAULT value");
    failures += expect_literal(child_at(row, 1U), MYLITE_SQL_AST_LITERAL_NULL, "insert NULL value");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t (a, `B`) VALUE (DEFAULT, CURRENT_TIMESTAMP);",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    rows = child_at(statement, 2U);
    failures += expect_child_count(statement, 3U, "insert with column list children");
    failures += expect_node(columns, MYLITE_SQL_AST_INSERT_COLUMN_LIST, "insert column list");
    failures += expect_child_count(columns, 2U, "insert column count");
    failures += expect_span_text(child_at(columns, 0U), "a", "insert first column");
    failures += expect_span_text(child_at(columns, 1U), "`B`", "insert quoted column");
    row = child_at(rows, 0U);
    failures += expect_node(child_at(row, 0U), MYLITE_SQL_AST_DEFAULT, "singular VALUE default");
    failures +=
        expect_current_timestamp(child_at(row, 1U), false, 0U, "singular VALUE current timestamp");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT t VALUES ROW(1, 2), ROW(3, 4);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    rows = child_at(statement, 1U);
    failures += expect_child_count(rows, 2U, "insert ROW constructor count");
    failures += expect_child_count(child_at(rows, 0U), 2U, "first ROW constructor values");
    failures += expect_literal(child_at(child_at(rows, 1U), 1U), MYLITE_SQL_AST_LITERAL_INTEGER,
                               "second ROW constructor value");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t () VALUES ();", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    rows = child_at(statement, 2U);
    failures += expect_node(columns, MYLITE_SQL_AST_INSERT_COLUMN_LIST, "empty insert columns");
    failures += expect_child_count(columns, 0U, "empty insert column count");
    failures += expect_child_count(child_at(rows, 0U), 0U, "empty insert row count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES ();", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    rows = child_at(statement, 1U);
    failures += expect_child_count(statement, 2U, "default row without column list children");
    failures += expect_child_count(child_at(rows, 0U), 0U, "default row without column list");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES (1,);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT IGNORE INTO t VALUES (1);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    if (statement == NULL || !statement->insert_ignore) {
        fprintf(stderr, "Expected INSERT IGNORE VALUES flag\n");
        ++failures;
    }
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT IGNORE t VALUE (DEFAULT);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    if (statement == NULL || !statement->insert_ignore) {
        fprintf(stderr, "Expected INSERT IGNORE VALUE flag\n");
        ++failures;
    }
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT IGNORE INTO t VALUES ROW(1), ROW(2);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    if (statement == NULL || !statement->insert_ignore) {
        fprintf(stderr, "Expected INSERT IGNORE ROW flag\n");
        ++failures;
    }
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT INTO IGNORE t VALUES (1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT LOW_PRIORITY INTO t VALUES (1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT DELAYED INTO t VALUES (1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_insert_set_syntax(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *assignments = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    const struct mylite_sql_ast_node *value = NULL;
    int failures = 0;

    failures += parse_sql("INSERT app.t SET a = 1, t.b = DEFAULT, app.t.c = a + 1;",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignments = child_at(statement, 1U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_SET_STATEMENT, "insert set statement");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "insert set qualified table");
    failures += expect_node(assignments, MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST,
                            "insert set assignment list");
    failures += expect_child_count(assignments, 3U, "insert set assignment count");

    assignment = child_at(assignments, 0U);
    failures += expect_node(assignment, MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT,
                            "insert set first assignment");
    failures += expect_span_text(child_at(assignment, 0U), "a", "insert set first target");
    failures += expect_literal(child_at(assignment, 1U), MYLITE_SQL_AST_LITERAL_INTEGER,
                               "insert set first value");

    assignment = child_at(assignments, 1U);
    failures += expect_node(child_at(assignment, 0U), MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "insert set table-qualified target");
    failures += expect_node(child_at(assignment, 1U), MYLITE_SQL_AST_DEFAULT, "insert set DEFAULT");

    assignment = child_at(assignments, 2U);
    failures += expect_node(child_at(assignment, 0U), MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "insert set schema-qualified target");
    value = child_at(assignment, 1U);
    failures += expect_node(value, MYLITE_SQL_AST_BINARY_EXPRESSION, "insert set arithmetic value");
    failures += expect_operator(value, MYLITE_SQL_AST_OPERATOR_ADD, "insert set arithmetic op");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t SET `CamelCase` = NULL, b = CURRENT_TIMESTAMP, c = - 4;",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignments = child_at(statement, 1U);
    failures += expect_child_count(assignments, 3U, "insert set scalar assignment count");
    failures += expect_span_text(child_at(child_at(assignments, 0U), 0U), "`CamelCase`",
                                 "insert set quoted target");
    failures += expect_literal(child_at(child_at(assignments, 0U), 1U), MYLITE_SQL_AST_LITERAL_NULL,
                               "insert set NULL");
    failures += expect_current_timestamp(child_at(child_at(assignments, 1U), 1U), false, 0U,
                                         "insert set CURRENT_TIMESTAMP");
    failures += expect_operator(child_at(child_at(assignments, 2U), 1U),
                                MYLITE_SQL_AST_OPERATOR_NEGATIVE, "insert set unary value");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t SET", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t SET a", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t SET a =", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t SET a = 1,", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT IGNORE INTO t SET a = 1", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    if (statement == NULL || !statement->insert_ignore) {
        fprintf(stderr, "Expected INSERT IGNORE SET flag\n");
        ++failures;
    }
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO IGNORE t SET a = 1", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT LOW_PRIORITY INTO t SET a = 1", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT DELAYED INTO t SET a = 1", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT INTO t PARTITION (p0) SET a = 1", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t SET a = 1 AS new", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t SET a = 1 ON DUPLICATE KEY UPDATE a = 2",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_replace_syntax(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *rows = NULL;
    const struct mylite_sql_ast_node *assignments = NULL;
    int failures = 0;

    failures +=
        parse_sql("REPLACE app.t VALUES (1, 'a'), (DEFAULT, NULL);", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    rows = child_at(statement, 1U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT, "replace values statement");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "replace qualified table name");
    failures += expect_child_count(statement, 2U, "replace without column list children");
    failures += expect_node(rows, MYLITE_SQL_AST_INSERT_ROW_LIST, "replace row list");
    failures += expect_child_count(rows, 2U, "replace row list count");
    failures += expect_node(child_at(child_at(rows, 1U), 0U), MYLITE_SQL_AST_DEFAULT,
                            "replace DEFAULT value");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("REPLACE LOW_PRIORITY INTO t (a, `B`) VALUE (DEFAULT, CURRENT_TIMESTAMP);",
                  MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    rows = child_at(statement, 2U);
    failures += expect_bool(statement->replace_low_priority, true, "replace low priority flag");
    failures += expect_bool(statement->replace_delayed, false, "replace low priority no delayed");
    failures += expect_node(columns, MYLITE_SQL_AST_INSERT_COLUMN_LIST, "replace column list");
    failures += expect_child_count(columns, 2U, "replace column count");
    failures += expect_current_timestamp(child_at(child_at(rows, 0U), 1U), false, 0U,
                                         "replace VALUE current timestamp");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE DELAYED INTO t VALUES ROW(1, 2), ROW(3, 4);",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    rows = child_at(statement, 1U);
    failures += expect_bool(statement->replace_delayed, true, "replace delayed flag");
    failures += expect_child_count(rows, 2U, "replace ROW constructor count");
    failures += expect_child_count(child_at(rows, 0U), 2U, "replace ROW first values");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t () VALUES ();", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    columns = child_at(statement, 1U);
    rows = child_at(statement, 2U);
    failures += expect_child_count(columns, 0U, "replace empty column count");
    failures += expect_child_count(child_at(rows, 0U), 0U, "replace empty row count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t VALUES ();", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    rows = child_at(statement, 1U);
    failures += expect_child_count(child_at(rows, 0U), 0U, "replace all-default row");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE app.t SET a = 1, t.b = DEFAULT, app.t.c = a + 1;",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignments = child_at(statement, 1U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_REPLACE_SET_STATEMENT, "replace set statement");
    failures += expect_node(assignments, MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST,
                            "replace set assignments");
    failures += expect_child_count(assignments, 3U, "replace set assignment count");
    failures += expect_node(child_at(child_at(assignments, 1U), 1U), MYLITE_SQL_AST_DEFAULT,
                            "replace set DEFAULT");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("REPLACE IGNORE INTO t VALUES (1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE HIGH_PRIORITY INTO t VALUES (1);", MYLITE_SQL_PARSE_SYNTAX_ERROR,
                          &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE LOW_PRIORITY DELAYED INTO t VALUES (1);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE DELAYED LOW_PRIORITY INTO t VALUES (1);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t VALUES (1,);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t SET a = 1,", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t PARTITION (p0) VALUES (1)", MYLITE_SQL_PARSE_SYNTAX_ERROR,
                          &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("REPLACE INTO t VALUES (1) AS new_row", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t VALUES (1) ON DUPLICATE KEY UPDATE a = 1",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t SELECT 1", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("REPLACE INTO t TABLE src", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_insert_on_duplicate_key_update_syntax(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *rows = NULL;
    const struct mylite_sql_ast_node *duplicate_update = NULL;
    const struct mylite_sql_ast_node *assignments = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    const struct mylite_sql_ast_node *row_alias = NULL;
    const struct mylite_sql_ast_node *alias_columns = NULL;
    const struct mylite_sql_ast_node *value = NULL;
    int failures = 0;

    failures += parse_sql("INSERT INTO t VALUES (1) ON DUPLICATE KEY UPDATE a = 1;",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    rows = child_at(statement, 1U);
    duplicate_update = child_at(statement, 2U);
    assignments = child_at(duplicate_update, 0U);
    assignment = child_at(assignments, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_INSERT_VALUES_STATEMENT, "ODKU values statement");
    failures += expect_node(rows, MYLITE_SQL_AST_INSERT_ROW_LIST, "ODKU values rows");
    failures += expect_node(duplicate_update, MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE,
                            "ODKU values clause");
    failures += expect_child_count(assignments, 1U, "ODKU assignment count");
    failures += expect_span_text(child_at(assignment, 0U), "a", "ODKU assignment target");
    failures += expect_literal(child_at(assignment, 1U), MYLITE_SQL_AST_LITERAL_INTEGER,
                               "ODKU assignment value");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUE (1) ON DUPLICATE KEY UPDATE a = DEFAULT;",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    duplicate_update = child_at(statement, 2U);
    assignment = child_at(child_at(duplicate_update, 0U), 0U);
    failures += expect_node(child_at(assignment, 1U), MYLITE_SQL_AST_DEFAULT, "ODKU DEFAULT");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES ROW(1, 2), ROW(3, 4) "
                          "ON DUPLICATE KEY UPDATE a = VALUES(a), b = b + 1;",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    rows = child_at(statement, 1U);
    duplicate_update = child_at(statement, 2U);
    assignments = child_at(duplicate_update, 0U);
    failures += expect_child_count(rows, 2U, "ODKU ROW constructor rows");
    failures += expect_child_count(assignments, 2U, "ODKU ROW assignment count");
    value = child_at(child_at(assignments, 0U), 1U);
    failures += expect_function_call(value, "VALUES", 1U, "ODKU VALUES function");
    value = child_at(child_at(assignments, 1U), 1U);
    failures += expect_node(value, MYLITE_SQL_AST_BINARY_EXPRESSION, "ODKU arithmetic");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT IGNORE INTO t (a, b) VALUES (1, 2) AS new_row(x, y) "
                          "ON DUPLICATE KEY UPDATE a = y, b = new_row.x;",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    row_alias = child_at(statement, 3U);
    duplicate_update = child_at(statement, 4U);
    alias_columns = child_at(row_alias, 1U);
    assignments = child_at(duplicate_update, 0U);
    if (statement == NULL || !statement->insert_ignore) {
        fprintf(stderr, "Expected ODKU INSERT IGNORE flag\n");
        ++failures;
    }
    failures +=
        expect_node(child_at(statement, 1U), MYLITE_SQL_AST_INSERT_COLUMN_LIST, "ODKU column list");
    failures += expect_span_text(child_at(row_alias, 0U), "new_row", "ODKU row alias");
    failures += expect_child_count(alias_columns, 2U, "ODKU column alias count");
    failures +=
        expect_span_text(child_at(child_at(assignments, 0U), 1U), "y", "ODKU column alias value");
    failures += expect_node(child_at(child_at(assignments, 1U), 1U),
                            MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "ODKU row alias qualified value");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t SET a = 1, b = 2 AS n(c1, c2) "
                          "ON DUPLICATE KEY UPDATE a = c1, a = c2;",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    row_alias = child_at(statement, 2U);
    duplicate_update = child_at(statement, 3U);
    assignments = child_at(duplicate_update, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_INSERT_SET_STATEMENT, "ODKU SET statement");
    failures += expect_span_text(child_at(row_alias, 0U), "n", "ODKU SET row alias");
    failures += expect_child_count(assignments, 2U, "ODKU repeated targets");
    failures += expect_span_text(child_at(child_at(assignments, 0U), 0U), "a",
                                 "ODKU repeated first target");
    failures += expect_span_text(child_at(child_at(assignments, 1U), 0U), "a",
                                 "ODKU repeated second target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES (1) ON DUPLICATE KEY UPDATE",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES (1) ON DUPLICATE UPDATE a = 1",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES (1) ON DUPLICATE KEY a = 1",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES (1) AS n() ON DUPLICATE KEY UPDATE a = 1",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES (1) ON DUPLICATE KEY UPDATE a",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES (1) ON DUPLICATE KEY UPDATE a =",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES (1) ON DUPLICATE KEY UPDATE a = 1,",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_update_single_table_syntax(void)
{
    enum { full_update_child_count = 5U };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *target = NULL;
    const struct mylite_sql_ast_node *assignments = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *order_by = NULL;
    const struct mylite_sql_ast_node *order_items = NULL;
    const struct mylite_sql_ast_node *limit = NULL;
    const struct mylite_sql_ast_node *value = NULL;
    int failures = 0;

    failures += parse_sql("UPDATE t SET a = 1", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    target = child_at(statement, 0U);
    assignments = child_at(statement, 1U);
    failures += expect_node(statement, MYLITE_SQL_AST_UPDATE_STATEMENT, "update statement");
    failures += expect_child_count(statement, 2U, "update base child count");
    failures += expect_node(target, MYLITE_SQL_AST_UPDATE_TARGET, "update target");
    failures += expect_span_text(child_at(target, 0U), "t", "update target table");
    failures +=
        expect_node(assignments, MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST, "update assignment list");
    failures += expect_child_count(assignments, 1U, "update assignment count");
    assignment = child_at(assignments, 0U);
    failures += expect_node(assignment, MYLITE_SQL_AST_UPDATE_ASSIGNMENT, "update assignment");
    failures += expect_span_text(child_at(assignment, 0U), "a", "update assignment target");
    failures +=
        expect_literal(child_at(assignment, 1U), MYLITE_SQL_AST_LITERAL_INTEGER, "update value");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE app.t AS tt SET tt.a = tt.a + 1, app.t.b = DEFAULT "
                          "WHERE tt.id = 1 ORDER BY tt.b DESC, tt.id ASC LIMIT 2",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    target = child_at(statement, 0U);
    assignments = child_at(statement, 1U);
    where_clause = child_at(statement, 2U);
    order_by = child_at(statement, 3U);
    limit = child_at(statement, 4U);
    failures += expect_child_count(statement, full_update_child_count, "full update child count");
    failures += expect_node(child_at(target, 0U), MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "schema-qualified update target");
    failures += expect_span_text(child_at(target, 1U), "tt", "update AS alias");
    failures += expect_child_count(assignments, 2U, "qualified update assignment count");
    assignment = child_at(assignments, 0U);
    failures += expect_node(child_at(assignment, 0U), MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "alias-qualified assignment target");
    failures += expect_operator(child_at(assignment, 1U), MYLITE_SQL_AST_OPERATOR_ADD,
                                "assignment arithmetic value");
    assignment = child_at(assignments, 1U);
    failures += expect_node(child_at(assignment, 1U), MYLITE_SQL_AST_DEFAULT, "update DEFAULT");
    failures += expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "update where clause");
    failures += expect_operator(child_at(where_clause, 0U), MYLITE_SQL_AST_OPERATOR_EQUAL,
                                "update where predicate");
    failures += expect_node(order_by, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "update order clause");
    order_items = child_at(order_by, 0U);
    failures += expect_child_count(order_items, 2U, "update order item count");
    failures += expect_order_item_direction(
        child_at(order_items, 0U), MYLITE_SQL_AST_KEY_PART_ORDER_DESC, "update desc order");
    failures += expect_order_item_direction(child_at(order_items, 1U),
                                            MYLITE_SQL_AST_KEY_PART_ORDER_ASC, "update asc order");
    failures += expect_node(limit, MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE, "update limit clause");
    failures += expect_limit_bound(child_at(limit, 0U), 2U, "update limit row count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE app.t tt SET tt.a = 100, tt.a = tt.a + 1 WHERE tt.id IN (1,2)",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    assignments = child_at(statement, 1U);
    failures += expect_span_text(child_at(child_at(statement, 0U), 1U), "tt", "update bare alias");
    failures += expect_child_count(assignments, 2U, "duplicate update assignment count");
    failures +=
        expect_span_text(child_at(child_at(assignments, 0U), 0U), "tt.a", "first duplicate target");
    failures += expect_span_text(child_at(child_at(assignments, 1U), 0U), "tt.a",
                                 "second duplicate target");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE t SET a = 1 LIMIT 18446744073709551615", MYLITE_SQL_PARSE_OK, &result);
    limit = child_at(child_at(result.root, 0U), 2U);
    failures += expect_limit_bound(child_at(limit, 0U), UINT64_MAX, "update max limit");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE t SET a = 1 LIMIT 1 OFFSET 1", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET a = 1 LIMIT 1,1", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET a = 1 LIMIT -1", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET a = 1 LIMIT '1'", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET a = 1 LIMIT NULL", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET a = 1 LIMIT 18446744073709551616",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("UPDATE LOW_PRIORITY t SET a = 1", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE IGNORE t SET a = 1", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t JOIN u ON t.id = u.id SET t.a = u.a",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET a = LAST_INSERT_ID(a + 1)", MYLITE_SQL_PARSE_OK, &result);
    assignment = child_at(child_at(child_at(result.root, 0U), 1U), 0U);
    value = child_at(assignment, 1U);
    failures += expect_function_call(value, "LAST_INSERT_ID", 1U, "update LAST_INSERT_ID call");
    failures += expect_operator(child_at(child_at(value, 1U), 0U), MYLITE_SQL_AST_OPERATOR_ADD,
                                "update LAST_INSERT_ID argument");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("UPDATE t SET a = 1 WHERE id = 1 LIMIT 1 ORDER BY id",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_delete_single_table_syntax(void)
{
    enum { full_delete_child_count = 4U };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *target = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *order_by = NULL;
    const struct mylite_sql_ast_node *order_items = NULL;
    const struct mylite_sql_ast_node *limit = NULL;
    int failures = 0;

    failures += parse_sql("DELETE FROM t", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    target = child_at(statement, 0U);
    failures += expect_node(statement, MYLITE_SQL_AST_DELETE_STATEMENT, "delete statement");
    failures += expect_child_count(statement, 1U, "delete base child count");
    failures += expect_node(target, MYLITE_SQL_AST_DELETE_TARGET, "delete target");
    failures += expect_span_text(child_at(target, 0U), "t", "delete target table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM app.t WHERE app.t.id = 1", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    target = child_at(statement, 0U);
    where_clause = child_at(statement, 1U);
    failures += expect_node(child_at(target, 0U), MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "schema-qualified delete target");
    failures += expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "delete where clause");
    failures += expect_operator(child_at(where_clause, 0U), MYLITE_SQL_AST_OPERATOR_EQUAL,
                                "delete schema-qualified predicate");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t AS tt WHERE tt.id = 1", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    target = child_at(statement, 0U);
    failures += expect_span_text(child_at(target, 1U), "tt", "delete AS alias");
    failures += expect_operator(child_at(child_at(statement, 1U), 0U),
                                MYLITE_SQL_AST_OPERATOR_EQUAL, "delete alias predicate");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t tt WHERE tt.id = 1", MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    failures += expect_span_text(child_at(child_at(statement, 0U), 1U), "tt", "delete bare alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t WHERE category = 1 ORDER BY v DESC, id ASC LIMIT 2",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    where_clause = child_at(statement, 1U);
    order_by = child_at(statement, 2U);
    limit = child_at(statement, 3U);
    failures += expect_child_count(statement, full_delete_child_count, "full delete child count");
    failures += expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "full delete where");
    failures += expect_node(order_by, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "full delete order");
    order_items = child_at(order_by, 0U);
    failures += expect_child_count(order_items, 2U, "delete order item count");
    failures += expect_order_item_direction(
        child_at(order_items, 0U), MYLITE_SQL_AST_KEY_PART_ORDER_DESC, "delete desc order");
    failures += expect_order_item_direction(child_at(order_items, 1U),
                                            MYLITE_SQL_AST_KEY_PART_ORDER_ASC, "delete asc order");
    failures += expect_node(limit, MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE, "delete limit clause");
    failures += expect_limit_bound(child_at(limit, 0U), 2U, "delete limit row count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t LIMIT 0", MYLITE_SQL_PARSE_OK, &result);
    limit = child_at(child_at(result.root, 0U), 1U);
    failures += expect_limit_bound(child_at(limit, 0U), 0U, "delete zero limit");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t LIMIT 18446744073709551615", MYLITE_SQL_PARSE_OK, &result);
    limit = child_at(child_at(result.root, 0U), 1U);
    failures += expect_limit_bound(child_at(limit, 0U), UINT64_MAX, "delete max limit");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t LIMIT 1 OFFSET 1", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t LIMIT 1,1", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t LIMIT -1", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t LIMIT '1'", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t LIMIT 18446744073709551616", MYLITE_SQL_PARSE_SYNTAX_ERROR,
                          &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE LOW_PRIORITY FROM t", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE QUICK FROM t", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE IGNORE FROM t", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("DELETE FROM t PARTITION (p0)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("WITH c AS (SELECT 1) DELETE FROM t", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("DELETE t FROM t JOIN u ON t.id = u.id", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_transaction_statement_syntax(void)
{
    enum {
        rollback_work_statement_index = 5,
        rollback_chain_statement_index = 5,
        rollback_no_chain_statement_index = 6,
        rollback_release_statement_index = 2,
        rollback_no_release_statement_index = 3,
        rollback_no_chain_release_statement_index = 4,
        rollback_work_chain_no_release_statement_index = 5,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *characteristics = NULL;
    const struct mylite_sql_ast_node *completion = NULL;
    int failures = 0;

    failures += parse_sql("START TRANSACTION READ WRITE, WITH CONSISTENT SNAPSHOT",
                          MYLITE_SQL_PARSE_OK, &result);
    statement = child_at(result.root, 0U);
    characteristics = child_at(statement, 0U);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_START_TRANSACTION_STATEMENT, "start transaction");
    failures += expect_child_count(characteristics, 2U, "start characteristics");
    failures += expect_transaction_access_mode(child_at(characteristics, 0U),
                                               MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_WRITE,
                                               "read write characteristic");
    failures +=
        expect_node(child_at(characteristics, 1U), MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC,
                    "consistent snapshot characteristic");
    if (child_at(characteristics, 1U) != NULL &&
        !child_at(characteristics, 1U)->transaction_consistent_snapshot) {
        fprintf(stderr, "consistent snapshot characteristic flag was not set\n");
        failures = 1;
    }
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("START TRANSACTION READ ONLY, READ ONLY", MYLITE_SQL_PARSE_OK, &result);
    characteristics = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(characteristics, 2U, "duplicate read only characteristics");
    failures += expect_transaction_access_mode(child_at(characteristics, 0U),
                                               MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY,
                                               "first read only characteristic");
    failures += expect_transaction_access_mode(child_at(characteristics, 1U),
                                               MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY,
                                               "second read only characteristic");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("CREATE TABLE transaction_keyword_names ("
                          "begin INT, chain INT, commit INT, consistent INT, no INT, "
                          "rollback INT, snapshot INT, start INT, transaction INT, work INT);",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("BEGIN; BEGIN WORK; COMMIT; COMMIT WORK; ROLLBACK; ROLLBACK WORK",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(child_at(result.root, 0U), MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT,
                            "begin transaction");
    failures += expect_node(child_at(result.root, 1U), MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT,
                            "begin work transaction");
    failures +=
        expect_node(child_at(result.root, 2U), MYLITE_SQL_AST_COMMIT_STATEMENT, "commit statement");
    failures += expect_node(child_at(result.root, 3U), MYLITE_SQL_AST_COMMIT_STATEMENT,
                            "commit work statement");
    failures += expect_node(child_at(result.root, 4U), MYLITE_SQL_AST_ROLLBACK_STATEMENT,
                            "rollback statement");
    failures += expect_node(child_at(result.root, rollback_work_statement_index),
                            MYLITE_SQL_AST_ROLLBACK_STATEMENT, "rollback work statement");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("COMMIT AND CHAIN; COMMIT AND CHAIN NO RELEASE; "
                          "COMMIT AND NO CHAIN RELEASE; COMMIT RELEASE; COMMIT NO RELEASE; "
                          "ROLLBACK AND CHAIN; ROLLBACK AND NO CHAIN NO RELEASE",
                          MYLITE_SQL_PARSE_OK, &result);
    completion = child_at(child_at(result.root, 0U), 0U);
    failures += expect_transaction_completion(completion, MYLITE_SQL_AST_TRANSACTION_CHAIN_YES,
                                              MYLITE_SQL_AST_TRANSACTION_RELEASE_DEFAULT,
                                              "commit chain completion");
    completion = child_at(child_at(result.root, 1U), 0U);
    failures += expect_transaction_completion(completion, MYLITE_SQL_AST_TRANSACTION_CHAIN_YES,
                                              MYLITE_SQL_AST_TRANSACTION_RELEASE_NO,
                                              "commit chain no release completion");
    completion = child_at(child_at(result.root, 2U), 0U);
    failures += expect_transaction_completion(completion, MYLITE_SQL_AST_TRANSACTION_CHAIN_NO,
                                              MYLITE_SQL_AST_TRANSACTION_RELEASE_YES,
                                              "commit no chain release completion");
    completion = child_at(child_at(result.root, 3U), 0U);
    failures += expect_transaction_completion(completion, MYLITE_SQL_AST_TRANSACTION_CHAIN_DEFAULT,
                                              MYLITE_SQL_AST_TRANSACTION_RELEASE_YES,
                                              "commit release completion");
    completion = child_at(child_at(result.root, 4U), 0U);
    failures += expect_transaction_completion(completion, MYLITE_SQL_AST_TRANSACTION_CHAIN_DEFAULT,
                                              MYLITE_SQL_AST_TRANSACTION_RELEASE_NO,
                                              "commit no release completion");
    completion = child_at(child_at(result.root, rollback_chain_statement_index), 0U);
    failures += expect_transaction_completion(completion, MYLITE_SQL_AST_TRANSACTION_CHAIN_YES,
                                              MYLITE_SQL_AST_TRANSACTION_RELEASE_DEFAULT,
                                              "rollback chain completion");
    completion = child_at(child_at(result.root, rollback_no_chain_statement_index), 0U);
    failures += expect_transaction_completion(completion, MYLITE_SQL_AST_TRANSACTION_CHAIN_NO,
                                              MYLITE_SQL_AST_TRANSACTION_RELEASE_NO,
                                              "rollback no chain no release completion");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("COMMIT AND NO CHAIN; COMMIT AND NO CHAIN NO RELEASE; "
                          "ROLLBACK RELEASE; ROLLBACK NO RELEASE; "
                          "ROLLBACK AND NO CHAIN RELEASE; ROLLBACK WORK AND CHAIN NO RELEASE",
                          MYLITE_SQL_PARSE_OK, &result);
    completion = child_at(child_at(result.root, 0U), 0U);
    failures += expect_transaction_completion(completion, MYLITE_SQL_AST_TRANSACTION_CHAIN_NO,
                                              MYLITE_SQL_AST_TRANSACTION_RELEASE_DEFAULT,
                                              "commit no chain completion");
    completion = child_at(child_at(result.root, 1U), 0U);
    failures += expect_transaction_completion(completion, MYLITE_SQL_AST_TRANSACTION_CHAIN_NO,
                                              MYLITE_SQL_AST_TRANSACTION_RELEASE_NO,
                                              "commit no chain no release completion");
    completion = child_at(child_at(result.root, rollback_release_statement_index), 0U);
    failures += expect_transaction_completion(completion, MYLITE_SQL_AST_TRANSACTION_CHAIN_DEFAULT,
                                              MYLITE_SQL_AST_TRANSACTION_RELEASE_YES,
                                              "rollback release completion");
    completion = child_at(child_at(result.root, rollback_no_release_statement_index), 0U);
    failures += expect_transaction_completion(completion, MYLITE_SQL_AST_TRANSACTION_CHAIN_DEFAULT,
                                              MYLITE_SQL_AST_TRANSACTION_RELEASE_NO,
                                              "rollback no release completion");
    completion = child_at(child_at(result.root, rollback_no_chain_release_statement_index), 0U);
    failures += expect_transaction_completion(completion, MYLITE_SQL_AST_TRANSACTION_CHAIN_NO,
                                              MYLITE_SQL_AST_TRANSACTION_RELEASE_YES,
                                              "rollback no chain release completion");
    completion =
        child_at(child_at(result.root, rollback_work_chain_no_release_statement_index), 0U);
    failures += expect_transaction_completion(completion, MYLITE_SQL_AST_TRANSACTION_CHAIN_YES,
                                              MYLITE_SQL_AST_TRANSACTION_RELEASE_NO,
                                              "rollback work chain no release completion");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("START TRANSACTION READ WRITE, READ ONLY", MYLITE_SQL_PARSE_SYNTAX_ERROR,
                          &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("START TRANSACTION READ ONLY READ WRITE", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("START TRANSACTION WITH CONSISTENT SNAPSHOT READ ONLY",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("BEGIN READ ONLY", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("BEGIN WORK READ ONLY", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("COMMIT CHAIN", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("COMMIT AND", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("COMMIT AND NO", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("COMMIT AND CHAIN RELEASE", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("COMMIT AND CHAIN AND RELEASE", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("ROLLBACK AND CHAIN RELEASE", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_savepoint_statement_syntax(void)
{
    enum {
        savepoint_statement_index = 0,
        savepoint_quoted_reserved_statement_index = 1,
        savepoint_quoted_dot_statement_index = 2,
        rollback_to_statement_index = 3,
        rollback_work_to_statement_index = 4,
        rollback_to_savepoint_statement_index = 5,
        rollback_work_to_savepoint_statement_index = 6,
        release_savepoint_statement_index = 7,
        rollback_quoted_reserved_statement_index = 8,
        release_quoted_reserved_statement_index = 9,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parse_sql("SAVEPOINT s; SAVEPOINT `select`; SAVEPOINT `db.sp`; "
                          "ROLLBACK TO s; ROLLBACK WORK TO s; ROLLBACK TO SAVEPOINT s; "
                          "ROLLBACK WORK TO SAVEPOINT s; RELEASE SAVEPOINT s; "
                          "ROLLBACK TO `select`; RELEASE SAVEPOINT `select`",
                          MYLITE_SQL_PARSE_OK, &result);

    statement = child_at(result.root, savepoint_statement_index);
    failures += expect_node(statement, MYLITE_SQL_AST_SAVEPOINT_STATEMENT, "savepoint statement");
    failures += expect_span_text(statement, "SAVEPOINT s", "savepoint statement span");
    failures += expect_child_count(statement, 1U, "savepoint child count");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_IDENTIFIER, "savepoint name");
    failures += expect_span_text(child_at(statement, 0U), "s", "savepoint name span");

    statement = child_at(result.root, savepoint_quoted_reserved_statement_index);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SAVEPOINT_STATEMENT, "quoted reserved savepoint");
    failures +=
        expect_span_text(child_at(statement, 0U), "`select`", "quoted reserved savepoint name");

    statement = child_at(result.root, savepoint_quoted_dot_statement_index);
    failures +=
        expect_node(statement, MYLITE_SQL_AST_SAVEPOINT_STATEMENT, "quoted dotted savepoint");
    failures +=
        expect_span_text(child_at(statement, 0U), "`db.sp`", "quoted dotted savepoint name");

    statement = child_at(result.root, rollback_to_statement_index);
    failures += expect_node(statement, MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT,
                            "rollback to statement");
    failures += expect_span_text(statement, "ROLLBACK TO s", "rollback to statement span");
    failures += expect_child_count(statement, 1U, "rollback to child count");
    failures += expect_node(child_at(statement, 0U), MYLITE_SQL_AST_IDENTIFIER, "rollback to name");
    failures += expect_span_text(child_at(statement, 0U), "s", "rollback to name span");

    statement = child_at(result.root, rollback_work_to_statement_index);
    failures += expect_node(statement, MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT,
                            "rollback work to statement");
    failures += expect_span_text(statement, "ROLLBACK WORK TO s", "rollback work to span");

    statement = child_at(result.root, rollback_to_savepoint_statement_index);
    failures += expect_node(statement, MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT,
                            "rollback to savepoint statement");
    failures +=
        expect_span_text(statement, "ROLLBACK TO SAVEPOINT s", "rollback to savepoint span");

    statement = child_at(result.root, rollback_work_to_savepoint_statement_index);
    failures += expect_node(statement, MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT,
                            "rollback work to savepoint statement");
    failures += expect_span_text(statement, "ROLLBACK WORK TO SAVEPOINT s",
                                 "rollback work to savepoint span");

    statement = child_at(result.root, release_savepoint_statement_index);
    failures += expect_node(statement, MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT,
                            "release savepoint statement");
    failures += expect_span_text(statement, "RELEASE SAVEPOINT s", "release savepoint span");
    failures += expect_child_count(statement, 1U, "release savepoint child count");
    failures +=
        expect_node(child_at(statement, 0U), MYLITE_SQL_AST_IDENTIFIER, "release savepoint name");
    failures += expect_span_text(child_at(statement, 0U), "s", "release savepoint name span");

    statement = child_at(result.root, rollback_quoted_reserved_statement_index);
    failures += expect_node(statement, MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT,
                            "rollback quoted reserved");
    failures +=
        expect_span_text(child_at(statement, 0U), "`select`", "rollback quoted reserved name");

    statement = child_at(result.root, release_quoted_reserved_statement_index);
    failures += expect_node(statement, MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT,
                            "release quoted reserved");
    failures +=
        expect_span_text(child_at(statement, 0U), "`select`", "release quoted reserved name");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SAVEPOINT", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SAVEPOINT select", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SAVEPOINT db.sp", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("ROLLBACK TO", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("ROLLBACK TO SAVEPOINT", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("ROLLBACK TO s AND CHAIN", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("ROLLBACK TO s RELEASE", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("ROLLBACK SAVEPOINT s", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("ROLLBACK TO select", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("RELEASE s", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("RELEASE SAVEPOINT", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("RELEASE SAVEPOINT select", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_expression_list(void)
{
    enum { expected_select_item_count = 5 };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *first_item = NULL;
    const struct mylite_sql_ast_node *add = NULL;
    const struct mylite_sql_ast_node *multiply = NULL;
    int failures = 0;

    failures += parse_sql("SELECT 1 + 2 * 3, 'text', TRUE, FALSE, NULL FROM DUAL;",
                          MYLITE_SQL_PARSE_OK, &result);

    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    first_item = child_at(select_list, 0U);
    add = child_at(first_item, 0U);
    multiply = child_at(add, 1U);

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

    failures += expect_literal(child_at(child_at(select_list, 1U), 0U),
                               MYLITE_SQL_AST_LITERAL_STRING, "string literal");
    failures += expect_literal(child_at(child_at(select_list, 2U), 0U), MYLITE_SQL_AST_LITERAL_TRUE,
                               "true literal");
    failures += expect_literal(child_at(child_at(select_list, 3U), 0U),
                               MYLITE_SQL_AST_LITERAL_FALSE, "false literal");
    failures += expect_literal(child_at(child_at(select_list, 4U), 0U), MYLITE_SQL_AST_LITERAL_NULL,
                               "null literal");

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_expression_operator_foundation_syntax(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    int failures = 0;

    failures += parse_sql("SELECT 1 = 1, 1 <=> NULL, 1 <> 2, 1 != 2, 1 < 2, 1 <= 2, "
                          "2 > 1, 2 >= 1",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    failures += expect_operator(child_at(child_at(select_list, 0U), 0U),
                                MYLITE_SQL_AST_OPERATOR_EQUAL, "equal operator");
    failures +=
        expect_operator(child_at(child_at(select_list, 1U), 0U),
                        MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL, "null safe equal operator");
    failures += expect_operator(child_at(child_at(select_list, 2U), 0U),
                                MYLITE_SQL_AST_OPERATOR_NOT_EQUAL, "angle not equal operator");
    failures += expect_operator(child_at(child_at(select_list, 3U), 0U),
                                MYLITE_SQL_AST_OPERATOR_NOT_EQUAL, "bang not equal operator");
    failures += expect_operator(child_at(child_at(select_list, 4U), 0U),
                                MYLITE_SQL_AST_OPERATOR_LESS, "less operator");
    failures += expect_operator(child_at(child_at(select_list, 5U), 0U),
                                MYLITE_SQL_AST_OPERATOR_LESS_EQUAL, "less equal operator");
    failures += expect_operator(child_at(child_at(select_list, 6U), 0U),
                                MYLITE_SQL_AST_OPERATOR_GREATER, "greater operator");
    failures += expect_operator(child_at(child_at(select_list, 7U), 0U),
                                MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL, "greater equal operator");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT !0, NOT 0, 1 AND 1, 1 && 1, 1 XOR 0, 1 OR 0, 1 || 0, "
                          "~0, 1 & 3, 1 | 2, 1 ^ 3, 1 << 2, 4 >> 1",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_operator(child_at(child_at(select_list, 0U), 0U),
                                MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT, "bang not");
    failures += expect_operator(child_at(child_at(select_list, 2U), 0U),
                                MYLITE_SQL_AST_OPERATOR_LOGICAL_AND, "and");
    failures += expect_operator(child_at(child_at(select_list, 4U), 0U),
                                MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR, "xor");
    failures += expect_operator(child_at(child_at(select_list, 5U), 0U),
                                MYLITE_SQL_AST_OPERATOR_LOGICAL_OR, "or");
    failures += expect_operator(child_at(child_at(select_list, 7U), 0U),
                                MYLITE_SQL_AST_OPERATOR_BITWISE_NOT, "bit not");
    failures += expect_operator(child_at(child_at(select_list, 8U), 0U),
                                MYLITE_SQL_AST_OPERATOR_BITWISE_AND, "bit and");
    failures += expect_operator(child_at(child_at(select_list, 9U), 0U),
                                MYLITE_SQL_AST_OPERATOR_BITWISE_OR, "bit or");
    failures += expect_operator(child_at(child_at(select_list, 10U), 0U),
                                MYLITE_SQL_AST_OPERATOR_BITWISE_XOR, "bit xor");
    failures += expect_operator(child_at(child_at(select_list, 11U), 0U),
                                MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT, "shift left");
    failures += expect_operator(child_at(child_at(select_list, 12U), 0U),
                                MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT, "shift right");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1 IS NULL, 1 IS NOT TRUE, 2 BETWEEN 1 AND 3, "
                          "2 NOT BETWEEN 3 AND 1, 'a' LIKE 'a' ESCAPE '!', "
                          "1 NOT IN (2,3), 5 DIV 2, 5 % 2, 5 MOD 2",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_operator(child_at(child_at(select_list, 0U), 0U),
                                MYLITE_SQL_AST_OPERATOR_IS_NULL, "is null");
    failures += expect_operator(child_at(child_at(select_list, 1U), 0U),
                                MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE, "is not true");
    expression = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_TERNARY_EXPRESSION, "between node");
    failures += expect_operator(expression, MYLITE_SQL_AST_OPERATOR_BETWEEN, "between op");
    expression = child_at(child_at(select_list, 4U), 0U);
    failures += expect_node(expression, MYLITE_SQL_AST_TERNARY_EXPRESSION, "like escape node");
    failures += expect_operator(child_at(child_at(select_list, 5U), 0U),
                                MYLITE_SQL_AST_OPERATOR_NOT_IN, "not in");
    failures += expect_node(child_at(child_at(child_at(select_list, 5U), 0U), 1U),
                            MYLITE_SQL_AST_EXPRESSION_LIST, "in expression list");
    failures += expect_operator(child_at(child_at(select_list, 6U), 0U),
                                MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE, "div");
    failures += expect_operator(child_at(child_at(select_list, 7U), 0U),
                                MYLITE_SQL_AST_OPERATOR_MODULO, "percent");
    failures += expect_operator(child_at(child_at(select_list, 8U), 0U),
                                MYLITE_SQL_AST_OPERATOR_MODULO, "mod");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1 IN ()", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_scalar_function_call_syntax(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    enum {
        expected_select_item_count = 40,
        string_function_item_count = 17,
        padding_function_item_count = 6,
        quote_function_item_count = 2,
        list_function_item_count = 19,
        coalesce_nested_arg_index = 2,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *item = NULL;
    const struct mylite_sql_ast_node *call = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = 0;

    failures += parse_sql("SELECT CONCAT('a','b') AS joined, PI() pi_value, "
                          "COALESCE(NULL, 1, CONCAT('x','y')), DATABASE(), SCHEMA(), VERSION(), "
                          "LAST_INSERT_ID(), LAST_INSERT_ID(5), ROW_COUNT(), CONNECTION_ID(), "
                          "USER(), SESSION_USER(), SYSTEM_USER(), CURRENT_USER(), CURRENT_USER, "
                          "IF(1, 2, 3), "
                          "LEFT('abc', 1), RIGHT('abc', 1), REPLACE('a','a','b'), "
                          "ROUND(123.456, 2), Exp(2), Ln(2), LOG(2), Log(10, 100), "
                          "LOG2(8), LOG10(1000), POW(2, 10), Power(2, -2), Sqrt(9), "
                          "Sin(1), COS(1), Tan(1), AtAn(1), ATAN(1, 2), ATAN2(1), "
                          "AtAn2(1, 2), Greatest(1, 2), least(1, 2, 3), "
                          "StRcMp('a', 'b'), FORMAT(1234.56, 2, 'de_DE') "
                          "FROM DUAL;",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, expected_select_item_count, "function select list");

    item = child_at(select_list, 0U);
    call = child_at(item, 0U);
    failures += expect_function_call(call, "CONCAT", 2U, "CONCAT call");
    failures += expect_span_text(call, "CONCAT('a','b')", "CONCAT call span");
    failures += expect_span_text(child_at(item, 1U), "joined", "CONCAT alias");

    item = child_at(select_list, 1U);
    call = child_at(item, 0U);
    failures += expect_function_call(call, "PI", 0U, "PI call");
    failures += expect_span_text(child_at(item, 1U), "pi_value", "PI bare alias");

    call = child_at(child_at(select_list, 2U), 0U);
    failures += expect_function_call(call, "COALESCE", 3U, "COALESCE call");
    arguments = child_at(call, 1U);
    failures += expect_function_call(child_at(arguments, coalesce_nested_arg_index), "CONCAT", 2U,
                                     "nested CONCAT call");

    failures += expect_function_call(child_at(child_at(select_list, 3U), 0U), "DATABASE", 0U,
                                     "DATABASE call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 4U), 0U), "SCHEMA", 0U, "SCHEMA call");
    failures += expect_function_call(child_at(child_at(select_list, 5U), 0U), "VERSION", 0U,
                                     "VERSION call");
    failures += expect_function_call(child_at(child_at(select_list, 6U), 0U), "LAST_INSERT_ID", 0U,
                                     "LAST_INSERT_ID no-arg call");
    failures += expect_function_call(child_at(child_at(select_list, 7U), 0U), "LAST_INSERT_ID", 1U,
                                     "LAST_INSERT_ID one-arg call");
    failures += expect_function_call(child_at(child_at(select_list, 8U), 0U), "ROW_COUNT", 0U,
                                     "ROW_COUNT call");
    failures += expect_function_call(child_at(child_at(select_list, 9U), 0U), "CONNECTION_ID", 0U,
                                     "CONNECTION_ID call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 10U), 0U), "USER", 0U, "USER call");
    failures += expect_function_call(child_at(child_at(select_list, 11U), 0U), "SESSION_USER", 0U,
                                     "SESSION_USER call");
    failures += expect_function_call(child_at(child_at(select_list, 12U), 0U), "SYSTEM_USER", 0U,
                                     "SYSTEM_USER call");
    failures += expect_function_call(child_at(child_at(select_list, 13U), 0U), "CURRENT_USER", 0U,
                                     "CURRENT_USER call");
    failures += expect_function_call(child_at(child_at(select_list, 14U), 0U), "CURRENT_USER", 0U,
                                     "bare CURRENT_USER call");
    failures += expect_function_call(child_at(child_at(select_list, 15U), 0U), "IF", 3U, "IF call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 16U), 0U), "LEFT", 2U, "LEFT call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 17U), 0U), "RIGHT", 2U, "RIGHT call");
    failures += expect_function_call(child_at(child_at(select_list, 18U), 0U), "REPLACE", 3U,
                                     "REPLACE call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 19U), 0U), "ROUND", 2U, "ROUND call");
    failures += expect_function_call(child_at(child_at(select_list, 20U), 0U), "Exp", 1U,
                                     "EXP case-insensitive call");
    failures += expect_function_call(child_at(child_at(select_list, 21U), 0U), "Ln", 1U, "LN call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 22U), 0U), "LOG", 1U, "LOG call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 23U), 0U), "Log", 2U, "LOG base call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 24U), 0U), "LOG2", 1U, "LOG2 call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 25U), 0U), "LOG10", 1U, "LOG10 call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 26U), 0U), "POW", 2U, "POW call");
    failures += expect_function_call(child_at(child_at(select_list, 27U), 0U), "Power", 2U,
                                     "POWER case-insensitive call");
    failures += expect_function_call(child_at(child_at(select_list, 28U), 0U), "Sqrt", 1U,
                                     "SQRT case-insensitive call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 29U), 0U), "Sin", 1U, "SIN call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 30U), 0U), "COS", 1U, "COS call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 31U), 0U), "Tan", 1U, "TAN call");
    failures += expect_function_call(child_at(child_at(select_list, 32U), 0U), "AtAn", 1U,
                                     "ATAN one-argument call");
    failures += expect_function_call(child_at(child_at(select_list, 33U), 0U), "ATAN", 2U,
                                     "ATAN two-argument call");
    failures += expect_function_call(child_at(child_at(select_list, 34U), 0U), "ATAN2", 1U,
                                     "ATAN2 one-argument call");
    failures += expect_function_call(child_at(child_at(select_list, 35U), 0U), "AtAn2", 2U,
                                     "ATAN2 two-argument call");
    failures += expect_function_call(child_at(child_at(select_list, 36U), 0U), "Greatest", 2U,
                                     "GREATEST call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 37U), 0U), "least", 3U, "LEAST call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 38U), 0U), "StRcMp", 2U, "STRCMP call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 39U), 0U), "FORMAT", 3U, "FORMAT call");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CONCAT_WS(',', 'a', 'b'), "
                          "SUBSTRING('abcdef', 2, 3), "
                          "SUBSTRING('abcdef' FROM 2 FOR 3), "
                          "SUBSTRING('abcdef' FROM -2), "
                          "SUBSTR('abcdef', 2, 3), SUBSTR('abcdef' FROM 2 FOR 3), "
                          "MID('abcdef', 2, 3), MID('abcdef' FROM -2), "
                          "TRIM('  hi  '), TRIM(LEADING 'x' FROM 'xx'), "
                          "TRIM(TRAILING 'x' FROM 'xx'), TRIM('x' FROM 'xx'), "
                          "TRIM(BOTH FROM '  hi  '), LTRIM('  hi  '), RTRIM('  hi  '), "
                          "SUBSTRING_INDEX('www.mysql.com', '.', 2), "
                          "substring_index('www.mysql.com', '.', -2);",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures +=
        expect_child_count(select_list, string_function_item_count, "string function select list");
    failures += expect_function_call(child_at(child_at(select_list, 0U), 0U), "CONCAT_WS", 3U,
                                     "CONCAT_WS call");
    failures += expect_function_call(child_at(child_at(select_list, 1U), 0U), "SUBSTRING", 3U,
                                     "SUBSTRING comma call");
    failures += expect_function_call(child_at(child_at(select_list, 2U), 0U), "SUBSTRING", 3U,
                                     "SUBSTRING FROM FOR call");
    failures += expect_function_call(child_at(child_at(select_list, 3U), 0U), "SUBSTRING", 2U,
                                     "SUBSTRING FROM call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 4U), 0U), "SUBSTR", 3U, "SUBSTR call");
    failures += expect_function_call(child_at(child_at(select_list, 5U), 0U), "SUBSTR", 3U,
                                     "SUBSTR FROM FOR call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 6U), 0U), "MID", 3U, "MID call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 7U), 0U), "MID", 2U, "MID FROM call");

    call = child_at(child_at(select_list, 8U), 0U);
    failures += expect_function_call(call, "TRIM", 1U, "ordinary TRIM call");
    arguments = child_at(call, 1U);
    {
        bool has_trim_spec = false;

        if (arguments != NULL) {
            has_trim_spec = arguments->trim_spec;
        }
        failures += expect_bool(has_trim_spec, false, "ordinary TRIM has no trim spec");
    }

    call = child_at(child_at(select_list, 9U), 0U);
    failures += expect_function_call(call, "TRIM", 2U, "directed TRIM call");
    arguments = child_at(call, 1U);
    {
        bool has_trim_spec = false;

        if (arguments != NULL) {
            has_trim_spec = arguments->trim_spec;
        }
        failures += expect_bool(has_trim_spec, true, "directed TRIM has trim spec");
    }
    if (arguments != NULL && arguments->trim_direction != MYLITE_SQL_AST_TRIM_DIRECTION_LEADING) {
        fprintf(stderr, "directed TRIM direction was not LEADING\n");
        failures = 1;
    }
    failures += expect_span_text(child_at(arguments, 0U), "'xx'", "directed TRIM source");
    failures += expect_span_text(child_at(arguments, 1U), "'x'", "directed TRIM remove string");

    call = child_at(child_at(select_list, 10U), 0U);
    failures += expect_function_call(call, "TRIM", 2U, "trailing TRIM call");
    arguments = child_at(call, 1U);
    if (arguments != NULL && arguments->trim_direction != MYLITE_SQL_AST_TRIM_DIRECTION_TRAILING) {
        fprintf(stderr, "trailing TRIM direction was not TRAILING\n");
        failures = 1;
    }

    call = child_at(child_at(select_list, 11U), 0U);
    failures += expect_function_call(call, "TRIM", 2U, "remstr TRIM call");
    arguments = child_at(call, 1U);
    if (arguments != NULL && arguments->trim_direction != MYLITE_SQL_AST_TRIM_DIRECTION_BOTH) {
        fprintf(stderr, "remstr TRIM direction was not BOTH\n");
        failures = 1;
    }

    call = child_at(child_at(select_list, 12U), 0U);
    failures += expect_function_call(call, "TRIM", 1U, "defaulted directed TRIM call");
    arguments = child_at(call, 1U);
    {
        bool has_trim_spec = false;

        if (arguments != NULL) {
            has_trim_spec = arguments->trim_spec;
        }
        failures += expect_bool(has_trim_spec, true, "defaulted directed TRIM has trim spec");
    }
    if (arguments != NULL && arguments->trim_direction != MYLITE_SQL_AST_TRIM_DIRECTION_BOTH) {
        fprintf(stderr, "defaulted directed TRIM direction was not BOTH\n");
        failures = 1;
    }
    failures +=
        expect_function_call(child_at(child_at(select_list, 13U), 0U), "LTRIM", 1U, "LTRIM call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 14U), 0U), "RTRIM", 1U, "RTRIM call");
    failures += expect_function_call(child_at(child_at(select_list, 15U), 0U), "SUBSTRING_INDEX",
                                     3U, "SUBSTRING_INDEX call");
    failures += expect_function_call(child_at(child_at(select_list, 16U), 0U), "substring_index",
                                     3U, "lowercase SUBSTRING_INDEX call");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ASCII('A'), ORD('\xE6\xB5\xB7'), LOCATE('a','alpha'), "
                          "LOCATE('a','alpha',2), POSITION('ph' IN 'alpha'), "
                          "INSTR('alpha','ph');",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, 6U, "search/code function select list");
    failures +=
        expect_function_call(child_at(child_at(select_list, 0U), 0U), "ASCII", 1U, "ASCII call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 1U), 0U), "ORD", 1U, "ORD call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 2U), 0U), "LOCATE", 2U, "LOCATE call");
    failures += expect_function_call(child_at(child_at(select_list, 3U), 0U), "LOCATE", 3U,
                                     "LOCATE start call");
    call = child_at(child_at(select_list, 4U), 0U);
    failures += expect_function_call(call, "POSITION", 2U, "POSITION special call");
    arguments = child_at(call, 1U);
    failures += expect_span_text(child_at(arguments, 0U), "'ph'", "POSITION substring");
    failures += expect_span_text(child_at(arguments, 1U), "'alpha'", "POSITION source");
    failures +=
        expect_function_call(child_at(child_at(select_list, 5U), 0U), "INSTR", 2U, "INSTR call");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT QUOTE('Don''t'), QUOTE(NULL);", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, quote_function_item_count, "quote function list");
    failures +=
        expect_function_call(child_at(child_at(select_list, 0U), 0U), "QUOTE", 1U, "QUOTE call");
    failures += expect_function_call(child_at(child_at(select_list, 1U), 0U), "QUOTE", 1U,
                                     "QUOTE NULL call");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT REPEAT('ab', 3), SPACE(3), REVERSE('abc'), "
                          "LPAD('hi', 5, '.'), RPAD('hi', 5, '.'), "
                          "INSERT('Quadratic', 3, 4, 'What');",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, padding_function_item_count,
                                   "padding function select list");
    failures +=
        expect_function_call(child_at(child_at(select_list, 0U), 0U), "REPEAT", 2U, "REPEAT call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 1U), 0U), "SPACE", 1U, "SPACE call");
    failures += expect_function_call(child_at(child_at(select_list, 2U), 0U), "REVERSE", 1U,
                                     "REVERSE call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 3U), 0U), "LPAD", 3U, "LPAD call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 4U), 0U), "RPAD", 3U, "RPAD call");
    failures += expect_function_call(child_at(child_at(select_list, 5U), 0U), "INSERT", 4U,
                                     "INSERT string function call");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ELT(2, 'a', 'b'), FIELD('b', 'a', 'b'), "
                          "FIND_IN_SET('b', 'a,b'), MAKE_SET(3, 'a', 'b'), "
                          "HEX('Az'), UNHEX('417a'), TO_BASE64('Az'), from_base64('QXo='), "
                          "BIN(12), OCT(12), CONV('a',16,2), "
                          "BIT_COUNT(7), BIT_LENGTH('abc'), cRc32('MySQL'), "
                          "INET_ATON('127.0.0.1'), "
                          "inet_ntoa(2130706433), "
                          "IS_UUID('6ccd780c-baba-1026-9564-5b8c656024db'), "
                          "Uuid_To_Bin('6ccd780c-baba-1026-9564-5b8c656024db', 1), "
                          "bin_to_uuid(UNHEX('6CCD780CBABA102695645B8C656024DB'));",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures +=
        expect_child_count(select_list, list_function_item_count, "list function select list");
    failures +=
        expect_function_call(child_at(child_at(select_list, 0U), 0U), "ELT", 3U, "ELT call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 1U), 0U), "FIELD", 3U, "FIELD call");
    failures += expect_function_call(child_at(child_at(select_list, 2U), 0U), "FIND_IN_SET", 2U,
                                     "FIND_IN_SET call");
    failures += expect_function_call(child_at(child_at(select_list, 3U), 0U), "MAKE_SET", 3U,
                                     "MAKE_SET call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 4U), 0U), "HEX", 1U, "HEX call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 5U), 0U), "UNHEX", 1U, "UNHEX call");
    failures += expect_function_call(child_at(child_at(select_list, 6U), 0U), "TO_BASE64", 1U,
                                     "TO_BASE64 call");
    failures += expect_function_call(child_at(child_at(select_list, 7U), 0U), "from_base64", 1U,
                                     "FROM_BASE64 case-insensitive call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 8U), 0U), "BIN", 1U, "BIN call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 9U), 0U), "OCT", 1U, "OCT call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 10U), 0U), "CONV", 3U, "CONV call");
    failures += expect_function_call(child_at(child_at(select_list, 11U), 0U), "BIT_COUNT", 1U,
                                     "BIT_COUNT call");
    failures += expect_function_call(child_at(child_at(select_list, 12U), 0U), "BIT_LENGTH", 1U,
                                     "BIT_LENGTH call");
    failures += expect_function_call(child_at(child_at(select_list, 13U), 0U), "cRc32", 1U,
                                     "CRC32 case-insensitive call");
    failures += expect_function_call(child_at(child_at(select_list, 14U), 0U), "INET_ATON", 1U,
                                     "INET_ATON call");
    failures += expect_function_call(child_at(child_at(select_list, 15U), 0U), "inet_ntoa", 1U,
                                     "INET_NTOA case-insensitive call");
    failures += expect_function_call(child_at(child_at(select_list, 16U), 0U), "IS_UUID", 1U,
                                     "IS_UUID call");
    failures += expect_function_call(child_at(child_at(select_list, 17U), 0U), "Uuid_To_Bin", 2U,
                                     "UUID_TO_BIN case-insensitive call");
    failures += expect_function_call(child_at(child_at(select_list, 18U), 0U), "bin_to_uuid", 1U,
                                     "BIN_TO_UUID case-insensitive call");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CHAR(65), CHAR(65,66 USING utf8mb4), "
                          "CHAR(65 USING 'latin1'), CHAR(65 USING binary);",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, 4U, "CHAR function select list");
    failures +=
        expect_function_call(child_at(child_at(select_list, 0U), 0U), "CHAR", 1U, "CHAR call");
    call = child_at(child_at(select_list, 1U), 0U);
    failures += expect_node(call, MYLITE_SQL_AST_FUNCTION_CALL, "CHAR utf8mb4 call");
    failures += expect_span_text(child_at(call, 0U), "CHAR", "CHAR utf8mb4 name");
    failures += expect_child_count(child_at(call, 1U), 2U, "CHAR utf8mb4 arguments");
    failures += expect_span_text(child_at(call, 2U), "utf8mb4", "CHAR utf8mb4 charset");
    call = child_at(child_at(select_list, 2U), 0U);
    failures += expect_node(call, MYLITE_SQL_AST_FUNCTION_CALL, "CHAR quoted latin1 call");
    failures += expect_span_text(child_at(call, 0U), "CHAR", "CHAR quoted latin1 name");
    failures += expect_child_count(child_at(call, 1U), 1U, "CHAR quoted latin1 arguments");
    failures += expect_span_text(child_at(call, 2U), "'latin1'", "CHAR quoted latin1 charset");
    call = child_at(child_at(select_list, 3U), 0U);
    failures += expect_node(call, MYLITE_SQL_AST_FUNCTION_CALL, "CHAR binary call");
    failures += expect_span_text(child_at(call, 0U), "CHAR", "CHAR binary name");
    failures += expect_child_count(child_at(call, 1U), 1U, "CHAR binary arguments");
    failures += expect_span_text(child_at(call, 2U), "binary", "CHAR binary charset");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CHAR();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CHAR(USING utf8mb4);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CHARSET('a'), COLLATION('a'), COERCIBILITY('a');",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, 3U, "charset introspection function select list");
    failures += expect_function_call(child_at(child_at(select_list, 0U), 0U), "CHARSET", 1U,
                                     "CHARSET call");
    failures += expect_function_call(child_at(child_at(select_list, 1U), 0U), "COLLATION", 1U,
                                     "COLLATION call");
    failures += expect_function_call(child_at(child_at(select_list, 2U), 0U), "COERCIBILITY", 1U,
                                     "COERCIBILITY call");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT POSITION('a' IN ('abc'));", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    call = child_at(child_at(select_list, 0U), 0U);
    failures += expect_function_call(call, "POSITION", 2U, "POSITION parenthesized source");
    arguments = child_at(call, 1U);
    failures +=
        expect_span_text(child_at(arguments, 0U), "'a'", "POSITION parenthesized substring");
    failures +=
        expect_span_text(child_at(arguments, 1U), "('abc')", "POSITION parenthesized source");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT ABS(1 IN (1)), LOCATE('a' IN ('abc'));", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, 2U, "IN predicate function argument select list");
    failures += expect_function_call(child_at(child_at(select_list, 0U), 0U), "ABS", 1U,
                                     "function IN predicate argument");
    failures += expect_function_call(child_at(child_at(select_list, 1U), 0U), "LOCATE", 1U,
                                     "LOCATE boolean argument call");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CONCAT ('a','b') AS spaced, LEFT('abc', 1) 'left alias';",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    item = child_at(select_list, 0U);
    failures += expect_function_call(child_at(item, 0U), "CONCAT", 2U, "spaced CONCAT call");
    failures += expect_span_text(child_at(item, 0U), "CONCAT ('a','b')", "spaced CONCAT span");
    item = child_at(select_list, 1U);
    failures += expect_function_call(child_at(item, 0U), "LEFT", 2U, "string aliased LEFT call");
    failures += expect_span_text(child_at(item, 1U), "'left alias'", "string function alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CURRENT_USER(), CURRENT_DATE(), CURRENT_TIME(), LOCALTIME(), "
                          "LOCALTIMESTAMP(), UTC_DATE(), UTC_TIME(), UTC_TIMESTAMP(), MOD(7,3);",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_function_call(child_at(child_at(select_list, 0U), 0U), "CURRENT_USER", 0U,
                                     "CURRENT_USER call");
    failures += expect_function_call(child_at(child_at(select_list, 1U), 0U), "CURRENT_DATE", 0U,
                                     "CURRENT_DATE call");
    failures += expect_function_call(child_at(child_at(select_list, 2U), 0U), "CURRENT_TIME", 0U,
                                     "CURRENT_TIME call");
    failures += expect_function_call(child_at(child_at(select_list, 3U), 0U), "LOCALTIME", 0U,
                                     "LOCALTIME call");
    failures += expect_function_call(child_at(child_at(select_list, 4U), 0U), "LOCALTIMESTAMP", 0U,
                                     "LOCALTIMESTAMP call");
    failures += expect_function_call(child_at(child_at(select_list, 5U), 0U), "UTC_DATE", 0U,
                                     "UTC_DATE call");
    failures += expect_function_call(child_at(child_at(select_list, 6U), 0U), "UTC_TIME", 0U,
                                     "UTC_TIME call");
    failures += expect_function_call(child_at(child_at(select_list, 7U), 0U), "UTC_TIMESTAMP", 0U,
                                     "UTC_TIMESTAMP call");
    failures +=
        expect_function_call(child_at(child_at(select_list, 8U), 0U), "MOD", 2U, "MOD call");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CURRENT_DATE, CURRENT_TIME, LOCALTIME, LOCALTIMESTAMP, "
                          "UTC_DATE, UTC_TIME, UTC_TIMESTAMP;",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_function_call(child_at(child_at(select_list, 0U), 0U), "CURRENT_DATE", 0U,
                                     "bare CURRENT_DATE call");
    failures += expect_function_call(child_at(child_at(select_list, 1U), 0U), "CURRENT_TIME", 0U,
                                     "bare CURRENT_TIME call");
    failures += expect_function_call(child_at(child_at(select_list, 2U), 0U), "LOCALTIME", 0U,
                                     "bare LOCALTIME call");
    failures += expect_function_call(child_at(child_at(select_list, 3U), 0U), "LOCALTIMESTAMP", 0U,
                                     "bare LOCALTIMESTAMP call");
    failures += expect_function_call(child_at(child_at(select_list, 4U), 0U), "UTC_DATE", 0U,
                                     "bare UTC_DATE call");
    failures += expect_function_call(child_at(child_at(select_list, 5U), 0U), "UTC_TIME", 0U,
                                     "bare UTC_TIME call");
    failures += expect_function_call(child_at(child_at(select_list, 6U), 0U), "UTC_TIMESTAMP", 0U,
                                     "bare UTC_TIMESTAMP call");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT NOW(), NOW(0), NOW(6), CURDATE(), CURTIME(), CURTIME(6), "
                          "CURRENT_TIME(6), LOCALTIME(6), LOCALTIMESTAMP(6), "
                          "CURRENT_TIMESTAMP, CURRENT_TIMESTAMP(), CURRENT_TIMESTAMP(6), "
                          "CURRENT_DATE, CURRENT_DATE(), CURRENT_TIME, LOCALTIME, "
                          "LOCALTIMESTAMP;",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures +=
        expect_function_call(child_at(child_at(select_list, 0U), 0U), "NOW", 0U, "NOW call");
    failures += expect_function_call(child_at(child_at(select_list, 1U), 0U), "NOW", 1U,
                                     "NOW fsp zero call");
    failures += expect_function_call(child_at(child_at(select_list, 2U), 0U), "NOW", 1U,
                                     "NOW fsp six call");
    failures += expect_function_call(child_at(child_at(select_list, 3U), 0U), "CURDATE", 0U,
                                     "CURDATE call");
    failures += expect_function_call(child_at(child_at(select_list, 4U), 0U), "CURTIME", 0U,
                                     "CURTIME call");
    failures += expect_function_call(child_at(child_at(select_list, 5U), 0U), "CURTIME", 1U,
                                     "CURTIME fsp call");
    failures += expect_function_call(child_at(child_at(select_list, 6U), 0U), "CURRENT_TIME", 1U,
                                     "CURRENT_TIME fsp call");
    failures += expect_function_call(child_at(child_at(select_list, 7U), 0U), "LOCALTIME", 1U,
                                     "LOCALTIME fsp call");
    failures += expect_function_call(child_at(child_at(select_list, 8U), 0U), "LOCALTIMESTAMP", 1U,
                                     "LOCALTIMESTAMP fsp call");
    failures += expect_current_timestamp(child_at(child_at(select_list, 9U), 0U), false, 0U,
                                         "bare CURRENT_TIMESTAMP");
    failures += expect_current_timestamp(child_at(child_at(select_list, 10U), 0U), false, 0U,
                                         "CURRENT_TIMESTAMP empty parens");
    failures += expect_current_timestamp(child_at(child_at(select_list, 11U), 0U), true, 6U,
                                         "CURRENT_TIMESTAMP fsp");
    failures += expect_function_call(child_at(child_at(select_list, 12U), 0U), "CURRENT_DATE", 0U,
                                     "bare CURRENT_DATE current temporal");
    failures += expect_function_call(child_at(child_at(select_list, 13U), 0U), "CURRENT_DATE", 0U,
                                     "CURRENT_DATE call current temporal");
    failures += expect_function_call(child_at(child_at(select_list, 14U), 0U), "CURRENT_TIME", 0U,
                                     "bare CURRENT_TIME current temporal");
    failures += expect_function_call(child_at(child_at(select_list, 15U), 0U), "LOCALTIME", 0U,
                                     "bare LOCALTIME current temporal");
    failures += expect_function_call(child_at(child_at(select_list, 16U), 0U), "LOCALTIMESTAMP", 0U,
                                     "bare LOCALTIMESTAMP current temporal");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT NOW, CURDATE, CURTIME;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_node(child_at(child_at(select_list, 0U), 0U), MYLITE_SQL_AST_IDENTIFIER,
                            "bare NOW identifier");
    failures += expect_node(child_at(child_at(select_list, 1U), 0U), MYLITE_SQL_AST_IDENTIFIER,
                            "bare CURDATE identifier");
    failures += expect_node(child_at(child_at(select_list, 2U), 0U), MYLITE_SQL_AST_IDENTIFIER,
                            "bare CURTIME identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT NOW(7)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT NOW(-1)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT NOW('3')", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT NOW(NULL)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT NOW(1 + 1)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT NOW(1, 2)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CURTIME(7)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CURRENT_TIME(7)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT LOCALTIME(7)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT LOCALTIMESTAMP(7)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CURDATE(0)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CURRENT_DATE(0)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT IF(1,2)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ASCII()", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ASCII('a','b')", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT REPEAT('a')", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT REPEAT('a',2,3)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT REVERSE()", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT REVERSE('a','b')", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT INSERT()", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT INSERT('a',1,1)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT INSERT('a',1,1,'x','y')", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT POSITION('a')", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT POSITION('a','b')", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT POSITION('a' IN 'abc' IN 'x')", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT POSITION(1 IN (1) IN 'abc')", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT LOCATE('a' IN 'abc')", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SUBSTRING('abc')", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SUBSTRING('abc',1,2,3)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SUBSTRING('abc' FROM 1 FOR 2 FOR 3)",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT TRIM()", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT TRIM('x','abc')", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT TRIM(BOTH 'x')", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT FOO('a' FROM 1)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_case_expression_syntax(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *case_expression = NULL;
    const struct mylite_sql_ast_node *when_list = NULL;
    const struct mylite_sql_ast_node *case_when = NULL;
    int failures = 0;

    failures += parse_sql("SELECT CASE WHEN 1 THEN 2 END;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    case_expression = child_at(child_at(select_list, 0U), 0U);
    when_list = child_at(case_expression, 0U);
    case_when = child_at(when_list, 0U);
    failures +=
        expect_node(case_expression, MYLITE_SQL_AST_CASE_EXPRESSION, "searched CASE expression");
    failures += expect_child_count(case_expression, 1U, "searched CASE children");
    failures += expect_bool(case_expression->case_expression_simple, false, "searched CASE mode");
    failures += expect_node(when_list, MYLITE_SQL_AST_CASE_WHEN_LIST, "searched CASE when list");
    failures += expect_child_count(when_list, 1U, "searched CASE when count");
    failures += expect_node(case_when, MYLITE_SQL_AST_CASE_WHEN, "searched CASE when");
    failures += expect_span_text(child_at(case_when, 0U), "1", "searched CASE condition");
    failures += expect_span_text(child_at(case_when, 1U), "2", "searched CASE result");
    failures += expect_span_text(case_expression, "CASE WHEN 1 THEN 2 END", "searched CASE span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CASE 1 WHEN 1 THEN 'one' ELSE 'other' END;", MYLITE_SQL_PARSE_OK,
                          &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    case_expression = child_at(child_at(select_list, 0U), 0U);
    when_list = child_at(case_expression, 1U);
    case_when = child_at(when_list, 0U);
    failures +=
        expect_node(case_expression, MYLITE_SQL_AST_CASE_EXPRESSION, "simple CASE expression");
    failures += expect_child_count(case_expression, 3U, "simple CASE children");
    failures += expect_bool(case_expression->case_expression_simple, true, "simple CASE mode");
    failures += expect_span_text(child_at(case_expression, 0U), "1", "simple CASE base");
    failures += expect_node(when_list, MYLITE_SQL_AST_CASE_WHEN_LIST, "simple CASE when list");
    failures += expect_span_text(child_at(case_when, 0U), "1", "simple CASE compare");
    failures += expect_span_text(child_at(case_when, 1U), "'one'", "simple CASE result");
    failures += expect_span_text(child_at(case_expression, 2U), "'other'", "simple CASE else");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CASE WHEN 0 THEN 1 WHEN 1 THEN CASE WHEN 1 THEN 2 ELSE 3 END "
                          "ELSE 4 END;",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    case_expression = child_at(child_at(select_list, 0U), 0U);
    when_list = child_at(case_expression, 0U);
    failures += expect_child_count(when_list, 2U, "multi WHEN CASE count");
    case_when = child_at(when_list, 1U);
    failures +=
        expect_node(child_at(case_when, 1U), MYLITE_SQL_AST_CASE_EXPRESSION, "nested CASE result");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CASE ELSE 1 END", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CASE WHEN 1 END", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_cast_expression_syntax(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *cast_expression = NULL;
    const struct mylite_sql_ast_node *target = NULL;
    const struct mylite_sql_ast_node *case_expression = NULL;
    const struct mylite_sql_ast_node *when_list = NULL;
    bool signed_flag = false;
    bool unsigned_flag = false;
    int failures = 0;

    failures += parse_sql("SELECT CAST('123' AS SIGNED INTEGER);", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    cast_expression = child_at(child_at(select_list, 0U), 0U);
    target = child_at(cast_expression, 1U);
    failures += expect_node(cast_expression, MYLITE_SQL_AST_CAST_EXPRESSION, "signed CAST node");
    failures += expect_child_count(cast_expression, 2U, "signed CAST children");
    failures += expect_literal(child_at(cast_expression, 0U), MYLITE_SQL_AST_LITERAL_STRING,
                               "signed CAST source");
    failures += expect_column_type(target, MYLITE_SQL_AST_COLUMN_TYPE_BIGINT, "signed CAST target");
    if (target != NULL) {
        signed_flag = target->column_type_signed;
        unsigned_flag = target->column_type_unsigned;
    }
    failures += expect_bool(signed_flag, true, "signed CAST signed flag");
    failures += expect_bool(unsigned_flag, false, "signed CAST unsigned flag");
    failures += expect_span_text(target, "SIGNED", "signed CAST target span");
    failures +=
        expect_span_text(cast_expression, "CAST('123' AS SIGNED INTEGER)", "signed CAST span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CAST('12.34' AS DECIMAL(6,2)), CAST('12' AS DEC(5));",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    cast_expression = child_at(child_at(select_list, 0U), 0U);
    target = child_at(cast_expression, 1U);
    failures +=
        expect_column_type(target, MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL, "decimal CAST target");
    if (target != NULL && (!target->has_column_precision || target->column_precision != 6U ||
                           !target->has_column_scale || target->column_scale != 2U)) {
        fprintf(stderr, "decimal CAST did not record precision 6 and scale 2\n");
        failures = 1;
    }
    cast_expression = child_at(child_at(select_list, 1U), 0U);
    target = child_at(cast_expression, 1U);
    failures += expect_column_type(target, MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL, "DEC CAST target");
    if (target != NULL && (!target->has_column_precision || target->column_precision != 5U ||
                           target->has_column_scale)) {
        fprintf(stderr, "DEC CAST did not record precision-only target\n");
        failures = 1;
    }
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CAST('abcdef' AS CHAR(3) CHARACTER SET 'utf8mb4'), "
                          "CAST('abc' AS CHAR CHARSET binary), "
                          "CAST('abc' AS NCHAR(4)), CAST('abc' AS BINARY);",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    cast_expression = child_at(child_at(select_list, 0U), 0U);
    target = child_at(cast_expression, 1U);
    failures += expect_column_type(target, MYLITE_SQL_AST_COLUMN_TYPE_CHAR, "CHAR CAST target");
    if (target != NULL && (!target->has_column_length || target->column_length != 3U ||
                           !target->has_column_character_set)) {
        fprintf(stderr, "CHAR CAST did not record length and charset\n");
        failures = 1;
    }
    failures +=
        expect_span_text(target, "CHAR(3) CHARACTER SET 'utf8mb4'", "CHAR CAST target span");
    cast_expression = child_at(child_at(select_list, 1U), 0U);
    target = child_at(cast_expression, 1U);
    failures += expect_column_type(target, MYLITE_SQL_AST_COLUMN_TYPE_CHAR, "CHARSET CAST target");
    if (target != NULL && !target->has_column_character_set) {
        fprintf(stderr, "CHARSET CAST did not record shorthand charset\n");
        failures = 1;
    }
    failures += expect_span_text(target, "CHAR CHARSET binary", "CHARSET CAST target span");
    cast_expression = child_at(child_at(select_list, 2U), 0U);
    target = child_at(cast_expression, 1U);
    failures += expect_column_type(target, MYLITE_SQL_AST_COLUMN_TYPE_CHAR, "NCHAR CAST target");
    if (target != NULL && (!target->column_national_attribute || !target->has_column_length ||
                           target->column_length != 4U)) {
        fprintf(stderr, "NCHAR CAST did not record national length\n");
        failures = 1;
    }
    cast_expression = child_at(child_at(select_list, 3U), 0U);
    failures += expect_column_type(child_at(cast_expression, 1U), MYLITE_SQL_AST_COLUMN_TYPE_BINARY,
                                   "BINARY CAST target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CASE WHEN 1 THEN CAST(1 AS UNSIGNED) "
                          "ELSE CAST(0 AS SIGNED) END;",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    case_expression = child_at(child_at(select_list, 0U), 0U);
    when_list = child_at(case_expression, 0U);
    failures += expect_node(child_at(child_at(when_list, 0U), 1U), MYLITE_SQL_AST_CAST_EXPRESSION,
                            "CASE CAST result");
    failures += expect_node(child_at(case_expression, 1U), MYLITE_SQL_AST_CAST_EXPRESSION,
                            "CASE CAST else");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CAST('1' AS INT)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CAST('1' AS INTEGER)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT CAST('1' AS NUMERIC(5,2))", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CAST('1' SIGNED)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT CAST('1' AS DECIMAL(66))", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parse_sql("SELECT CAST('1' AS DECIMAL(5,6))", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CAST('x' AS CHAR CHARACTER SET utf8mb4 COLLATE utf8mb4_bin)",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CAST('x' AS BINARY(3))", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_convert_expression_syntax(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *cast_expression = NULL;
    const struct mylite_sql_ast_node *target = NULL;
    const struct mylite_sql_ast_node *case_expression = NULL;
    const struct mylite_sql_ast_node *when_list = NULL;
    bool signed_flag = false;
    bool unsigned_flag = false;
    int failures = 0;

    failures += parse_sql("SELECT CONVERT('123', SIGNED INTEGER), "
                          "CONVERT('12.34', DECIMAL(6,2)), "
                          "CONVERT('abc', CHAR CHARACTER SET latin1), "
                          "CONVERT('abc', NCHAR(4)), "
                          "CONVERT('abc', BINARY);",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    cast_expression = child_at(child_at(select_list, 0U), 0U);
    target = child_at(cast_expression, 1U);
    failures += expect_node(cast_expression, MYLITE_SQL_AST_CAST_EXPRESSION, "signed CONVERT node");
    failures +=
        expect_column_type(target, MYLITE_SQL_AST_COLUMN_TYPE_BIGINT, "signed CONVERT target");
    if (target != NULL) {
        signed_flag = target->column_type_signed;
        unsigned_flag = target->column_type_unsigned;
    }
    failures += expect_bool(signed_flag, true, "signed CONVERT signed flag");
    failures += expect_bool(unsigned_flag, false, "signed CONVERT unsigned flag");
    failures +=
        expect_span_text(cast_expression, "CONVERT('123', SIGNED INTEGER)", "signed CONVERT span");
    cast_expression = child_at(child_at(select_list, 1U), 0U);
    target = child_at(cast_expression, 1U);
    failures +=
        expect_column_type(target, MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL, "decimal CONVERT target");
    if (target != NULL && (!target->has_column_precision || target->column_precision != 6U ||
                           !target->has_column_scale || target->column_scale != 2U)) {
        fprintf(stderr, "decimal CONVERT did not record precision 6 and scale 2\n");
        failures = 1;
    }
    cast_expression = child_at(child_at(select_list, 2U), 0U);
    target = child_at(cast_expression, 1U);
    failures += expect_column_type(target, MYLITE_SQL_AST_COLUMN_TYPE_CHAR, "CHAR CONVERT target");
    if (target != NULL && !target->has_column_character_set) {
        fprintf(stderr, "CHAR CONVERT did not record charset\n");
        failures = 1;
    }
    cast_expression = child_at(child_at(select_list, 3U), 0U);
    target = child_at(cast_expression, 1U);
    failures += expect_column_type(target, MYLITE_SQL_AST_COLUMN_TYPE_CHAR, "NCHAR CONVERT target");
    if (target != NULL && (!target->column_national_attribute || !target->has_column_length ||
                           target->column_length != 4U)) {
        fprintf(stderr, "NCHAR CONVERT did not record national length\n");
        failures = 1;
    }
    cast_expression = child_at(child_at(select_list, 4U), 0U);
    failures += expect_column_type(child_at(cast_expression, 1U), MYLITE_SQL_AST_COLUMN_TYPE_BINARY,
                                   "BINARY CONVERT target");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CONVERT('abc' USING latin1), "
                          "CONVERT('abc' USING utf8mb4), "
                          "CONVERT('abc' USING 'utf8mb3'), "
                          "CONVERT('abc' USING binary);",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    cast_expression = child_at(child_at(select_list, 0U), 0U);
    target = child_at(cast_expression, 1U);
    failures += expect_node(cast_expression, MYLITE_SQL_AST_CAST_EXPRESSION, "USING CONVERT node");
    failures += expect_column_type(target, MYLITE_SQL_AST_COLUMN_TYPE_CHAR, "USING CONVERT target");
    if (target != NULL && !target->has_column_character_set) {
        fprintf(stderr, "USING CONVERT did not record charset\n");
        failures = 1;
    }
    failures +=
        expect_span_text(cast_expression, "CONVERT('abc' USING latin1)", "USING CONVERT span");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CASE WHEN 1 THEN CONVERT(1, UNSIGNED) "
                          "ELSE CONVERT(0 USING latin1) END;",
                          MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    case_expression = child_at(child_at(select_list, 0U), 0U);
    when_list = child_at(case_expression, 0U);
    failures += expect_node(child_at(child_at(when_list, 0U), 1U), MYLITE_SQL_AST_CAST_EXPRESSION,
                            "CASE CONVERT result");
    failures += expect_node(child_at(case_expression, 1U), MYLITE_SQL_AST_CAST_EXPRESSION,
                            "CASE CONVERT else");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CONVERT('1', INT)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CONVERT()", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CONVERT(1)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CONVERT(1, SIGNED, 2)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CONVERT('abc' USING)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parse_sql("SELECT CONVERT('x', CHAR CHARACTER SET utf8mb4 COLLATE utf8mb4_bin)",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_information_schema_select(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *from_table = NULL;
    const struct mylite_sql_ast_node *qualified = NULL;
    int failures = 0;

    failures +=
        parse_sql("SELECT * FROM INFORMATION_SCHEMA.ENGINES;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_SELECT_STATEMENT, "information schema select");
    failures += expect_node(from_table, MYLITE_SQL_AST_FROM_TABLE, "information schema from");
    failures += expect_node(qualified, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "information schema table name");
    failures += expect_span_text(child_at(qualified, 0U), "INFORMATION_SCHEMA",
                                 "information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "ENGINES", "information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "INFORMATION_SCHEMA",
                                 "character sets information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "CHARACTER_SETS",
                                 "character sets information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM INFORMATION_SCHEMA.COLLATIONS;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "INFORMATION_SCHEMA",
                                 "collations information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "COLLATIONS",
                                 "collations information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "INFORMATION_SCHEMA",
                                 "collation charset applicability information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "COLLATION_CHARACTER_SET_APPLICABILITY",
                                 "collation charset applicability information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM INFORMATION_SCHEMA.KEYWORDS;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "INFORMATION_SCHEMA",
                                 "keywords information schema qualifier");
    failures +=
        expect_span_text(child_at(qualified, 1U), "KEYWORDS", "keywords information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "INFORMATION_SCHEMA",
                                 "check constraints information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "CHECK_CONSTRAINTS",
                                 "check constraints information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE;", MYLITE_SQL_PARSE_OK,
                          &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "INFORMATION_SCHEMA",
                                 "key column usage information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "KEY_COLUMN_USAGE",
                                 "key column usage information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "INFORMATION_SCHEMA",
                                 "table constraints information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "TABLE_CONSTRAINTS",
                                 "table constraints information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "INFORMATION_SCHEMA",
                                 "referential constraints information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "REFERENTIAL_CONSTRAINTS",
                                 "referential constraints information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM information_schema.engines;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "information_schema",
                                 "lower information schema qualifier");
    failures +=
        expect_span_text(child_at(qualified, 1U), "engines", "lower information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM information_schema.character_sets;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "information_schema",
                                 "lower character sets information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "character_sets",
                                 "lower character sets information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM information_schema.collations;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "information_schema",
                                 "lower collations information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "collations",
                                 "lower collations information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM information_schema.collation_character_set_applicability;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "information_schema",
                                 "lower collation charset applicability qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "collation_character_set_applicability",
                                 "lower collation charset applicability table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM information_schema.keywords;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "information_schema",
                                 "lower keywords information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "keywords", "lower keywords table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM information_schema.check_constraints;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "information_schema",
                                 "lower check constraints information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "check_constraints",
                                 "lower check constraints table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM information_schema.key_column_usage;", MYLITE_SQL_PARSE_OK,
                          &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "information_schema",
                                 "lower key column usage information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "key_column_usage",
                                 "lower key column usage table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM information_schema.table_constraints;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "information_schema",
                                 "lower table constraints information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "table_constraints",
                                 "lower table constraints table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM information_schema.referential_constraints;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "information_schema",
                                 "lower referential constraints information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "referential_constraints",
                                 "lower referential constraints table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM Information_Schema.EnGiNeS;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "Information_Schema",
                                 "mixed information schema qualifier");
    failures +=
        expect_span_text(child_at(qualified, 1U), "EnGiNeS", "mixed information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM Information_Schema.Character_Sets;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "Information_Schema",
                                 "mixed character sets information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "Character_Sets",
                                 "mixed character sets information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM Information_Schema.Collations;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "Information_Schema",
                                 "mixed collations information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "Collations",
                                 "mixed collations information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM Information_Schema.Collation_Character_Set_Applicability;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "Information_Schema",
                                 "mixed collation charset applicability qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "Collation_Character_Set_Applicability",
                                 "mixed collation charset applicability table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM Information_Schema.Keywords;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures +=
        expect_span_text(child_at(qualified, 0U), "Information_Schema", "mixed keywords qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "Keywords", "mixed keywords table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM Information_Schema.Check_Constraints;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "Information_Schema",
                                 "mixed check constraints qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "Check_Constraints",
                                 "mixed check constraints table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM Information_Schema.Key_Column_Usage;", MYLITE_SQL_PARSE_OK,
                          &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "Information_Schema",
                                 "mixed key column usage qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "Key_Column_Usage",
                                 "mixed key column usage table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM Information_Schema.Table_Constraints;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "Information_Schema",
                                 "mixed table constraints qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "Table_Constraints",
                                 "mixed table constraints table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM Information_Schema.Referential_Constraints;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "Information_Schema",
                                 "mixed referential constraints qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "Referential_Constraints",
                                 "mixed referential constraints table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM `information_schema`.`ENGINES`;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "`information_schema`",
                                 "quoted information schema qualifier");
    failures +=
        expect_span_text(child_at(qualified, 1U), "`ENGINES`", "quoted information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM `information_schema`.`CHARACTER_SETS`;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "`information_schema`",
                                 "quoted character sets information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "`CHARACTER_SETS`",
                                 "quoted character sets information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM `information_schema`.`COLLATIONS`;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "`information_schema`",
                                 "quoted collations information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "`COLLATIONS`",
                                 "quoted collations information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM `information_schema`.`COLLATION_CHARACTER_SET_APPLICABILITY`;",
                  MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "`information_schema`",
                                 "quoted collation charset applicability qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "`COLLATION_CHARACTER_SET_APPLICABILITY`",
                                 "quoted collation charset applicability table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM `information_schema`.`KEYWORDS`;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "`information_schema`",
                                 "quoted keywords qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "`KEYWORDS`", "quoted keywords table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM `information_schema`.`CHECK_CONSTRAINTS`;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "`information_schema`",
                                 "quoted check constraints qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "`CHECK_CONSTRAINTS`",
                                 "quoted check constraints table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM `information_schema`.`KEY_COLUMN_USAGE`;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "`information_schema`",
                                 "quoted key column usage qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "`KEY_COLUMN_USAGE`",
                                 "quoted key column usage table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM `information_schema`.`TABLE_CONSTRAINTS`;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "`information_schema`",
                                 "quoted table constraints qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "`TABLE_CONSTRAINTS`",
                                 "quoted table constraints table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM `information_schema`.`REFERENTIAL_CONSTRAINTS`;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "`information_schema`",
                                 "quoted referential constraints qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "`REFERENTIAL_CONSTRAINTS`",
                                 "quoted referential constraints table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM INFORMATION_SCHEMA.SCHEMATA;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM `information_schema`.`STATISTICS`;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA;",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM INFORMATION_SCHEMA.SCHEMATA WHERE TRUE;",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(child_at(child_at(result.root, 0U), 2U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "information schema where clause");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT ENGINE FROM INFORMATION_SCHEMA.ENGINES;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM INFORMATION_SCHEMA.ENGINES WHERE ENGINE = 'InnoDB';",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(child_at(child_at(result.root, 0U), 2U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "information schema engines where clause");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CHARACTER_SET_NAME FROM INFORMATION_SCHEMA.CHARACTER_SETS;",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS WHERE CHARACTER_SET_NAME = "
                  "'utf8mb4';",
                  MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(child_at(child_at(result.root, 0U), 2U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "information schema character sets where clause");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT COLLATION_NAME FROM INFORMATION_SCHEMA.COLLATIONS;",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM INFORMATION_SCHEMA.COLLATIONS WHERE COLLATION_NAME = "
                          "'utf8mb4_bin';",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(child_at(child_at(result.root, 0U), 2U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "information schema collations where clause");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql(
        "SELECT COLLATION_NAME FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY;",
        MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY WHERE "
                  "CHARACTER_SET_NAME = 'utf8mb4';",
                  MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(child_at(child_at(result.root, 0U), 2U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "information schema collation charset applicability where clause");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT WORD FROM INFORMATION_SCHEMA.KEYWORDS;", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM INFORMATION_SCHEMA.KEYWORDS WHERE RESERVED = 1;",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(child_at(child_at(result.root, 0U), 2U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "information schema keywords where clause");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CONSTRAINT_NAME FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS;",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS WHERE "
                          "CONSTRAINT_NAME = 'chk';",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(child_at(child_at(result.root, 0U), 2U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "information schema check constraints where clause");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CONSTRAINT_NAME FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE;",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE "
                          "CONSTRAINT_NAME = 'PRIMARY';",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(child_at(child_at(result.root, 0U), 2U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "information schema key column usage where clause");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CONSTRAINT_NAME FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS;",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE "
                          "CONSTRAINT_TYPE = 'UNIQUE';",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(child_at(child_at(result.root, 0U), 2U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "information schema table constraints where clause");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT CONSTRAINT_NAME FROM "
                          "INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS;",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS WHERE "
                          "CONSTRAINT_NAME = 'fk_parent';",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(child_at(child_at(result.root, 0U), 2U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "information schema referential constraints where clause");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_table_core_syntax(void)
{
    static const size_t aliased_select_item_count = 5U;
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *from_table = NULL;
    const struct mylite_sql_ast_node *qualified = NULL;
    const struct mylite_sql_ast_node *item = NULL;
    const struct mylite_sql_ast_node *wildcard = NULL;
    int failures = 0;

    failures += parse_sql("SELECT a AS x, b y, c AS `quoted alias`, d AS 'single alias', "
                          "e 'bare string' FROM app.t AS alias;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_SELECT_STATEMENT, "table select statement");
    failures +=
        expect_child_count(select_list, aliased_select_item_count, "aliased select item count");
    failures +=
        expect_node(qualified, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "schema-qualified table");
    failures += expect_span_text(child_at(qualified, 0U), "app", "select table schema");
    failures += expect_span_text(child_at(qualified, 1U), "t", "select table name");
    failures += expect_span_text(child_at(from_table, 1U), "alias", "select table alias");
    failures += expect_span_text(child_at(child_at(select_list, 0U), 1U), "x", "AS alias");
    failures += expect_span_text(child_at(child_at(select_list, 1U), 1U), "y", "bare alias");
    failures +=
        expect_span_text(child_at(child_at(select_list, 2U), 1U), "`quoted alias`", "quoted alias");
    failures += expect_literal(child_at(child_at(select_list, 3U), 1U),
                               MYLITE_SQL_AST_LITERAL_STRING, "single quoted alias");
    failures += expect_literal(child_at(child_at(select_list, 4U), 1U),
                               MYLITE_SQL_AST_LITERAL_STRING, "bare string alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT `quoted alias`.* FROM t AS `quoted alias`;", MYLITE_SQL_PARSE_OK,
                          &result);
    select = child_at(result.root, 0U);
    wildcard = child_at(child_at(child_at(select, 0U), 0U), 0U);
    failures += expect_node(wildcard, MYLITE_SQL_AST_WILDCARD, "quoted alias wildcard");
    failures +=
        expect_span_text(child_at(wildcard, 0U), "`quoted alias`", "quoted wildcard qualifier");
    failures += expect_span_text(child_at(child_at(select, 1U), 1U), "`quoted alias`",
                                 "quoted table alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a, t.*, app.t.* FROM app.t alias;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    failures += expect_child_count(select_list, 3U, "mixed qualified wildcard count");
    item = child_at(select_list, 1U);
    wildcard = child_at(item, 0U);
    failures += expect_node(wildcard, MYLITE_SQL_AST_WILDCARD, "table wildcard");
    failures += expect_span_text(child_at(wildcard, 0U), "t", "table wildcard qualifier");
    item = child_at(select_list, 2U);
    wildcard = child_at(item, 0U);
    failures += expect_span_text(child_at(wildcard, 0U), "app", "schema wildcard qualifier");
    failures += expect_span_text(child_at(wildcard, 1U), "t", "schema wildcard table");
    failures += expect_span_text(child_at(child_at(select, 1U), 1U), "alias", "bare table alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a, * FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a INTO @x FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_inner_join_syntax(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *from_clause = NULL;
    const struct mylite_sql_ast_node *references = NULL;
    const struct mylite_sql_ast_node *join = NULL;
    const struct mylite_sql_ast_node *condition = NULL;
    const struct mylite_sql_ast_node *using_columns = NULL;
    const struct mylite_sql_ast_node *table = NULL;
    const struct mylite_sql_ast_node *qualified = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    int failures = 0;

    failures +=
        parse_sql("SELECT id FROM app.t AS alias WHERE id = 1;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_clause = child_at(select, 1U);
    qualified = child_at(from_clause, 0U);
    failures +=
        expect_node(from_clause, MYLITE_SQL_AST_FROM_TABLE, "single table keeps from table node");
    failures +=
        expect_node(qualified, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "single table qualified name");
    failures += expect_span_text(child_at(qualified, 0U), "app", "single table schema");
    failures += expect_span_text(child_at(qualified, 1U), "t", "single table name");
    failures += expect_span_text(child_at(from_clause, 1U), "alias", "single table alias");
    failures +=
        expect_node(child_at(select, 2U), MYLITE_SQL_AST_WHERE_CLAUSE, "single table where child");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM l JOIN r;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_clause = child_at(select, 1U);
    references = child_at(from_clause, 0U);
    join = child_at(references, 0U);
    failures +=
        expect_node(from_clause, MYLITE_SQL_AST_FROM_TABLE_REFERENCES, "join from references");
    failures +=
        expect_node(references, MYLITE_SQL_AST_TABLE_REFERENCE_LIST, "join table reference list");
    failures += expect_child_count(references, 1U, "single explicit join list count");
    failures += expect_node(join, MYLITE_SQL_AST_JOIN_EXPRESSION, "bare join expression");
    failures += expect_join_type(join, MYLITE_SQL_AST_JOIN_INNER, "bare JOIN type");
    failures += expect_child_count(join, 2U, "join without condition child count");
    failures += expect_span_text(child_at(child_at(join, 0U), 0U), "l", "join left table");
    failures += expect_span_text(child_at(child_at(join, 1U), 0U), "r", "join right table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a FROM l, r, p;", MYLITE_SQL_PARSE_OK, &result);
    references = child_at(child_at(child_at(result.root, 0U), 1U), 0U);
    failures += expect_child_count(references, 3U, "comma join table count");
    failures += expect_span_text(child_at(child_at(references, 0U), 0U), "l", "comma first");
    failures += expect_span_text(child_at(child_at(references, 1U), 0U), "r", "comma second");
    failures += expect_span_text(child_at(child_at(references, 2U), 0U), "p", "comma third");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM l INNER JOIN r;", MYLITE_SQL_PARSE_OK, &result);
    join = child_at(child_at(child_at(child_at(result.root, 0U), 1U), 0U), 0U);
    failures += expect_join_type(join, MYLITE_SQL_AST_JOIN_INNER, "INNER JOIN type");
    failures += expect_child_count(join, 2U, "INNER JOIN without condition child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM l CROSS JOIN r;", MYLITE_SQL_PARSE_OK, &result);
    join = child_at(child_at(child_at(child_at(result.root, 0U), 1U), 0U), 0U);
    failures += expect_join_type(join, MYLITE_SQL_AST_JOIN_CROSS, "CROSS JOIN type");
    failures += expect_child_count(join, 2U, "CROSS JOIN without condition child count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM l CROSS JOIN r ON l.id = r.id;", MYLITE_SQL_PARSE_OK, &result);
    join = child_at(child_at(child_at(child_at(result.root, 0U), 1U), 0U), 0U);
    condition = child_at(join, 2U);
    predicate = child_at(condition, 0U);
    failures += expect_join_type(join, MYLITE_SQL_AST_JOIN_CROSS, "CROSS JOIN type");
    failures += expect_join_condition_type(condition, MYLITE_SQL_AST_JOIN_CONDITION_ON,
                                           "CROSS JOIN ON condition type");
    failures += expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_EQUAL, "CROSS JOIN predicate");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM l JOIN r ON l.id = r.l_id;", MYLITE_SQL_PARSE_OK, &result);
    join = child_at(child_at(child_at(child_at(result.root, 0U), 1U), 0U), 0U);
    condition = child_at(join, 2U);
    predicate = child_at(condition, 0U);
    failures += expect_join_condition_type(condition, MYLITE_SQL_AST_JOIN_CONDITION_ON,
                                           "JOIN ON condition type");
    failures += expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_EQUAL, "JOIN ON predicate");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM l JOIN r USING (id, shared);", MYLITE_SQL_PARSE_OK, &result);
    join = child_at(child_at(child_at(child_at(result.root, 0U), 1U), 0U), 0U);
    condition = child_at(join, 2U);
    using_columns = child_at(condition, 0U);
    failures += expect_join_condition_type(condition, MYLITE_SQL_AST_JOIN_CONDITION_USING,
                                           "JOIN USING condition type");
    failures +=
        expect_node(using_columns, MYLITE_SQL_AST_USING_COLUMN_LIST, "JOIN USING column list");
    failures += expect_child_count(using_columns, 2U, "JOIN USING column count");
    failures +=
        expect_node(child_at(using_columns, 0U), MYLITE_SQL_AST_USING_COLUMN, "first USING item");
    failures +=
        expect_span_text(child_at(child_at(using_columns, 0U), 0U), "id", "first USING column");
    failures += expect_span_text(child_at(child_at(using_columns, 1U), 0U), "shared",
                                 "second USING column");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM l, r JOIN p ON r.id = p.l_id;", MYLITE_SQL_PARSE_OK, &result);
    references = child_at(child_at(child_at(result.root, 0U), 1U), 0U);
    join = child_at(references, 1U);
    failures += expect_child_count(references, 2U, "comma explicit precedence list count");
    failures += expect_node(child_at(references, 0U), MYLITE_SQL_AST_FROM_TABLE,
                            "comma explicit left table");
    failures +=
        expect_node(join, MYLITE_SQL_AST_JOIN_EXPRESSION, "comma explicit right join subtree");
    failures += expect_span_text(child_at(child_at(join, 0U), 0U), "r", "comma explicit join left");
    failures +=
        expect_span_text(child_at(child_at(join, 1U), 0U), "p", "comma explicit join right");
    failures += expect_join_condition_type(child_at(join, 2U), MYLITE_SQL_AST_JOIN_CONDITION_ON,
                                           "comma explicit ON belongs to explicit join");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM app.l AS lefty JOIN r righty "
                          "ON lefty.id = righty.l_id;",
                          MYLITE_SQL_PARSE_OK, &result);
    join = child_at(child_at(child_at(child_at(result.root, 0U), 1U), 0U), 0U);
    table = child_at(join, 0U);
    qualified = child_at(table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "app", "aliased join schema");
    failures += expect_span_text(child_at(qualified, 1U), "l", "aliased join table");
    failures += expect_span_text(child_at(table, 1U), "lefty", "AS join alias");
    failures += expect_span_text(child_at(child_at(join, 1U), 1U), "righty", "bare join alias");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM l, r ON l.id = r.id;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_outer_join_syntax(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *references = NULL;
    const struct mylite_sql_ast_node *join = NULL;
    const struct mylite_sql_ast_node *condition = NULL;
    const struct mylite_sql_ast_node *using_columns = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    int failures = 0;

    failures +=
        parse_sql("SELECT * FROM l LEFT JOIN r ON l.id = r.id;", MYLITE_SQL_PARSE_OK, &result);
    join = child_at(child_at(child_at(child_at(result.root, 0U), 1U), 0U), 0U);
    condition = child_at(join, 2U);
    predicate = child_at(condition, 0U);
    failures += expect_join_type(join, MYLITE_SQL_AST_JOIN_LEFT, "LEFT JOIN type");
    failures += expect_child_count(join, 3U, "LEFT JOIN condition child count");
    failures += expect_join_condition_type(condition, MYLITE_SQL_AST_JOIN_CONDITION_ON,
                                           "LEFT JOIN ON condition type");
    failures += expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_EQUAL, "LEFT JOIN predicate");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM l LEFT OUTER JOIN r USING (id, shared);",
                          MYLITE_SQL_PARSE_OK, &result);
    join = child_at(child_at(child_at(child_at(result.root, 0U), 1U), 0U), 0U);
    condition = child_at(join, 2U);
    using_columns = child_at(condition, 0U);
    failures += expect_join_type(join, MYLITE_SQL_AST_JOIN_LEFT, "LEFT OUTER JOIN type");
    failures += expect_join_condition_type(condition, MYLITE_SQL_AST_JOIN_CONDITION_USING,
                                           "LEFT OUTER JOIN USING condition type");
    failures +=
        expect_node(using_columns, MYLITE_SQL_AST_USING_COLUMN_LIST, "LEFT OUTER USING list");
    failures += expect_child_count(using_columns, 2U, "LEFT OUTER USING column count");
    failures += expect_span_text(child_at(child_at(using_columns, 0U), 0U), "id",
                                 "LEFT OUTER first USING column");
    failures += expect_span_text(child_at(child_at(using_columns, 1U), 0U), "shared",
                                 "LEFT OUTER second USING column");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM l RIGHT JOIN r ON l.id = r.id;", MYLITE_SQL_PARSE_OK, &result);
    join = child_at(child_at(child_at(child_at(result.root, 0U), 1U), 0U), 0U);
    condition = child_at(join, 2U);
    predicate = child_at(condition, 0U);
    failures += expect_join_type(join, MYLITE_SQL_AST_JOIN_RIGHT, "RIGHT JOIN type");
    failures += expect_join_condition_type(condition, MYLITE_SQL_AST_JOIN_CONDITION_ON,
                                           "RIGHT JOIN ON condition type");
    failures += expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_EQUAL, "RIGHT JOIN predicate");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM l RIGHT OUTER JOIN r USING (id);", MYLITE_SQL_PARSE_OK, &result);
    join = child_at(child_at(child_at(child_at(result.root, 0U), 1U), 0U), 0U);
    condition = child_at(join, 2U);
    using_columns = child_at(condition, 0U);
    failures += expect_join_type(join, MYLITE_SQL_AST_JOIN_RIGHT, "RIGHT OUTER JOIN type");
    failures += expect_join_condition_type(condition, MYLITE_SQL_AST_JOIN_CONDITION_USING,
                                           "RIGHT OUTER JOIN USING condition type");
    failures += expect_child_count(using_columns, 1U, "RIGHT OUTER USING column count");
    failures += expect_span_text(child_at(child_at(using_columns, 0U), 0U), "id",
                                 "RIGHT OUTER first USING column");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM l, r LEFT JOIN p USING (id);", MYLITE_SQL_PARSE_OK, &result);
    references = child_at(child_at(child_at(result.root, 0U), 1U), 0U);
    join = child_at(references, 1U);
    failures += expect_child_count(references, 2U, "comma outer precedence list count");
    failures +=
        expect_node(child_at(references, 0U), MYLITE_SQL_AST_FROM_TABLE, "comma outer left table");
    failures += expect_node(join, MYLITE_SQL_AST_JOIN_EXPRESSION, "comma outer join subtree");
    failures += expect_join_type(join, MYLITE_SQL_AST_JOIN_LEFT, "comma outer join type");
    failures += expect_span_text(child_at(child_at(join, 0U), 0U), "r", "comma outer join left");
    failures += expect_span_text(child_at(child_at(join, 1U), 0U), "p", "comma outer join right");
    failures += expect_join_condition_type(child_at(join, 2U), MYLITE_SQL_AST_JOIN_CONDITION_USING,
                                           "comma outer USING belongs to explicit join");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM l LEFT JOIN r;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM l LEFT OUTER JOIN r;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM l RIGHT JOIN r;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM l RIGHT OUTER JOIN r;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_where_clause_syntax(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    int failures = 0;

    failures += parse_sql("SELECT id FROM t WHERE 1;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    where_clause = child_at(select, 2U);
    predicate = child_at(where_clause, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_SELECT_STATEMENT, "where select statement");
    failures += expect_child_count(select, 3U, "where select child count");
    failures +=
        expect_node(child_at(select, 0U), MYLITE_SQL_AST_SELECT_LIST, "where select list child");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "where from child");
    failures += expect_node(where_clause, MYLITE_SQL_AST_WHERE_CLAUSE, "where clause child");
    failures += expect_span_text(where_clause, "WHERE 1", "where clause span");
    failures += expect_literal(predicate, MYLITE_SQL_AST_LITERAL_INTEGER, "where literal");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM t WHERE n;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures +=
        expect_node(child_at(select, 2U), MYLITE_SQL_AST_WHERE_CLAUSE, "wildcard where clause");
    failures += expect_span_text(child_at(child_at(select, 2U), 0U), "n", "wildcard predicate");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT t.* FROM t WHERE t.n = 1;", MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    failures +=
        expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_EQUAL, "qualified where equality");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM t AS tt WHERE tt.n = 1;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_span_text(child_at(child_at(child_at(result.root, 0U), 1U), 1U), "tt",
                                 "AS where alias");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM t tt WHERE tt.n = 1;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_span_text(child_at(child_at(child_at(result.root, 0U), 1U), 1U), "tt",
                                 "bare where alias");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM t WHERE n BETWEEN 1 AND 2;", MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    failures += expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_BETWEEN, "where between");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM t WHERE s LIKE 'a%' ESCAPE '!';", MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    failures += expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_LIKE, "where like escape");
    failures += expect_child_count(predicate, 3U, "where like escape child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM t WHERE s LIKE 'a%' ESCAPE '!' OR 1;",
                          MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    failures += expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_LOGICAL_OR,
                                "where like escape before or");
    failures += expect_operator(child_at(predicate, 0U), MYLITE_SQL_AST_OPERATOR_LIKE,
                                "where like escape left of or");
    failures +=
        expect_child_count(child_at(predicate, 0U), 3U, "where like escape left child count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM t WHERE n IN (1, 2, NULL);", MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    failures += expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_IN, "where in");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM t WHERE n IN ();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM t JOIN t AS u WHERE t.id = u.id;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(child_at(child_at(result.root, 0U), 2U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "join where clause");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_order_limit_offset_syntax(void)
{
    enum { where_order_limit_child_count = 5U };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *order_by = NULL;
    const struct mylite_sql_ast_node *order_items = NULL;
    const struct mylite_sql_ast_node *limit = NULL;
    int failures = 0;

    failures += parse_sql("SELECT a FROM t ORDER BY a;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    order_by = child_at(select, 2U);
    order_items = child_at(order_by, 0U);
    failures += expect_child_count(select, 3U, "order select child count");
    failures += expect_node(order_by, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "order clause");
    failures += expect_span_text(order_by, "ORDER BY a", "order clause span");
    failures += expect_node(order_items, MYLITE_SQL_AST_ORDER_ITEM_LIST, "order item list");
    failures += expect_child_count(order_items, 1U, "order item count");
    failures += expect_span_text(child_at(child_at(order_items, 0U), 0U), "a", "order expression");
    failures += expect_order_item_direction(
        child_at(order_items, 0U), MYLITE_SQL_AST_KEY_PART_ORDER_ASC, "default order direction");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a FROM t WHERE n = 1 ORDER BY n DESC, a ASC LIMIT 2 OFFSET 1;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    order_by = child_at(select, 3U);
    order_items = child_at(order_by, 0U);
    limit = child_at(select, 4U);
    failures +=
        expect_child_count(select, where_order_limit_child_count, "where order limit child count");
    failures +=
        expect_node(child_at(select, 2U), MYLITE_SQL_AST_WHERE_CLAUSE, "where before order");
    failures += expect_node(order_by, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "order after where");
    failures += expect_child_count(order_items, 2U, "multiple order item count");
    failures += expect_order_item_direction(
        child_at(order_items, 0U), MYLITE_SQL_AST_KEY_PART_ORDER_DESC, "desc order direction");
    failures += expect_order_item_direction(
        child_at(order_items, 1U), MYLITE_SQL_AST_KEY_PART_ORDER_ASC, "asc order direction");
    failures += expect_node(limit, MYLITE_SQL_AST_LIMIT_CLAUSE, "offset keyword limit clause");
    failures += expect_limit_bound(child_at(limit, 0U), 1U, "offset keyword normalized offset");
    failures += expect_limit_bound(child_at(limit, 1U), 2U, "offset keyword normalized row count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT a FROM t LIMIT 1, 18446744073709551615;", MYLITE_SQL_PARSE_OK, &result);
    limit = child_at(child_at(result.root, 0U), 2U);
    failures += expect_node(limit, MYLITE_SQL_AST_LIMIT_CLAUSE, "comma limit clause");
    failures += expect_limit_bound(child_at(limit, 0U), 1U, "comma limit normalized offset");
    failures +=
        expect_limit_bound(child_at(limit, 1U), UINT64_MAX, "comma limit normalized row count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a FROM t LIMIT 3;", MYLITE_SQL_PARSE_OK, &result);
    limit = child_at(child_at(result.root, 0U), 2U);
    failures += expect_limit_bound(child_at(limit, 0U), 0U, "single limit normalized offset");
    failures += expect_limit_bound(child_at(limit, 1U), 3U, "single limit normalized row count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a FROM t ORDER BY -1 LIMIT 2;", MYLITE_SQL_PARSE_OK, &result);
    order_items = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    failures += expect_operator(child_at(child_at(order_items, 0U), 0U),
                                MYLITE_SQL_AST_OPERATOR_NEGATIVE, "negative order constant");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT a FROM t ORDER BY a NULLS LAST;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a FROM t ORDER BY a WHERE n = 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR,
                          &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a FROM t LIMIT -1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a FROM t LIMIT 1.5;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a FROM t LIMIT '2';", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a FROM t LIMIT NULL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a FROM t LIMIT 1 + 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a FROM t LIMIT 1, -2;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT a FROM t LIMIT 2 OFFSET -1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a FROM t LIMIT 18446744073709551616;",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a FROM t LIMIT ?;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1 ORDER BY 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1 LIMIT 1;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_select_distinct_syntax(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    int failures = 0;

    failures += parse_sql("SELECT 1;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_select_duplicate_mode(select, MYLITE_SQL_AST_SELECT_DUPLICATES_IMPLICIT_ALL,
                                             false, false, 0U, "implicit duplicate mode");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ALL a FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_select_duplicate_mode(select, MYLITE_SQL_AST_SELECT_DUPLICATES_ALL, true,
                                             false, 1U, "all duplicate mode");
    failures +=
        expect_node(child_at(select, 0U), MYLITE_SQL_AST_SELECT_LIST, "all select list child");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT DISTINCT a FROM t ORDER BY a LIMIT 1;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_select_duplicate_mode(select, MYLITE_SQL_AST_SELECT_DUPLICATES_DISTINCT,
                                             true, false, 1U, "distinct duplicate mode");
    failures += expect_node(child_at(select, 1U), MYLITE_SQL_AST_FROM_TABLE, "distinct from child");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DISTINCTROW * FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_select_duplicate_mode(select, MYLITE_SQL_AST_SELECT_DUPLICATES_DISTINCT,
                                             true, false, 1U, "distinctrow duplicate mode");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DISTINCT DISTINCTROW a FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_select_duplicate_mode(select, MYLITE_SQL_AST_SELECT_DUPLICATES_DISTINCT,
                                             true, false, 2U, "repeated distinct duplicate mode");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ALL ALL a FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_select_duplicate_mode(select, MYLITE_SQL_AST_SELECT_DUPLICATES_ALL, true,
                                             false, 2U, "repeated all duplicate mode");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ALL DISTINCT a FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_select_duplicate_mode(select, MYLITE_SQL_AST_SELECT_DUPLICATES_ALL, true,
                                             true, 2U, "mixed duplicate mode");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT DISTINCT ALL a FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_select_duplicate_mode(select, MYLITE_SQL_AST_SELECT_DUPLICATES_DISTINCT,
                                             true, true, 2U, "reverse mixed duplicate mode");
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_union_query_expression_syntax(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *query = NULL;
    const struct mylite_sql_ast_node *top_union = NULL;
    const struct mylite_sql_ast_node *left_union = NULL;
    const struct mylite_sql_ast_node *left_primary = NULL;
    const struct mylite_sql_ast_node *local_select = NULL;
    int failures = 0;

    failures += parse_sql("SELECT 1 UNION SELECT 2;", MYLITE_SQL_PARSE_OK, &result);
    query = child_at(result.root, 0U);
    top_union = child_at(query, 0U);
    failures += expect_node(query, MYLITE_SQL_AST_QUERY_EXPRESSION, "simple union query");
    failures += expect_child_count(query, 1U, "simple union query child count");
    failures += expect_union_duplicate_mode(top_union, MYLITE_SQL_AST_SET_DUPLICATES_DISTINCT,
                                            "default union duplicate mode");
    failures +=
        expect_node(child_at(top_union, 0U), MYLITE_SQL_AST_SELECT_STATEMENT, "left union select");
    failures +=
        expect_node(child_at(top_union, 1U), MYLITE_SQL_AST_SELECT_STATEMENT, "right union select");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1 UNION ALL SELECT 2 UNION DISTINCT SELECT 3 "
                          "ORDER BY 1 LIMIT 2 OFFSET 1;",
                          MYLITE_SQL_PARSE_OK, &result);
    query = child_at(result.root, 0U);
    top_union = child_at(query, 0U);
    left_union = child_at(top_union, 0U);
    failures += expect_node(query, MYLITE_SQL_AST_QUERY_EXPRESSION, "chained union query");
    failures += expect_child_count(query, 3U, "chained union global clauses");
    failures += expect_node(child_at(query, 1U), MYLITE_SQL_AST_ORDER_BY_CLAUSE,
                            "chained union global order");
    failures +=
        expect_node(child_at(query, 2U), MYLITE_SQL_AST_LIMIT_CLAUSE, "chained union global limit");
    failures += expect_union_duplicate_mode(top_union, MYLITE_SQL_AST_SET_DUPLICATES_DISTINCT,
                                            "top union distinct mode");
    failures += expect_union_duplicate_mode(left_union, MYLITE_SQL_AST_SET_DUPLICATES_ALL,
                                            "left union all mode");
    failures +=
        expect_node(child_at(top_union, 1U), MYLITE_SQL_AST_SELECT_STATEMENT, "third union select");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("(SELECT n FROM t ORDER BY n DESC LIMIT 1) UNION ALL SELECT n FROM t;",
                          MYLITE_SQL_PARSE_OK, &result);
    query = child_at(result.root, 0U);
    top_union = child_at(query, 0U);
    left_primary = child_at(top_union, 0U);
    local_select = child_at(left_primary, 0U);
    failures += expect_union_duplicate_mode(top_union, MYLITE_SQL_AST_SET_DUPLICATES_ALL,
                                            "parenthesized union all mode");
    failures +=
        expect_node(left_primary, MYLITE_SQL_AST_QUERY_PRIMARY, "parenthesized union operand");
    failures +=
        expect_node(local_select, MYLITE_SQL_AST_SELECT_STATEMENT, "parenthesized operand select");
    failures += expect_node(child_at(local_select, 2U), MYLITE_SQL_AST_ORDER_BY_CLAUSE,
                            "parenthesized operand local order");
    failures += expect_node(child_at(local_select, 3U), MYLITE_SQL_AST_LIMIT_CLAUSE,
                            "parenthesized operand local limit");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("((SELECT 1 AS v)) UNION ((SELECT 2)) ORDER BY v;", MYLITE_SQL_PARSE_OK, &result);
    query = child_at(result.root, 0U);
    top_union = child_at(query, 0U);
    left_primary = child_at(child_at(top_union, 0U), 0U);
    local_select = child_at(left_primary, 0U);
    failures += expect_node(child_at(top_union, 0U), MYLITE_SQL_AST_QUERY_PRIMARY,
                            "nested parenthesized outer operand");
    failures += expect_node(left_primary, MYLITE_SQL_AST_QUERY_PRIMARY,
                            "nested parenthesized inner operand");
    failures +=
        expect_node(local_select, MYLITE_SQL_AST_SELECT_STATEMENT, "nested parenthesized select");
    failures += expect_node(child_at(query, 1U), MYLITE_SQL_AST_ORDER_BY_CLAUSE,
                            "nested parenthesized global order");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1 UNION (SELECT 2 LIMIT 1);", MYLITE_SQL_PARSE_OK, &result);
    query = child_at(result.root, 0U);
    top_union = child_at(query, 0U);
    left_primary = child_at(top_union, 1U);
    local_select = child_at(left_primary, 0U);
    failures +=
        expect_node(left_primary, MYLITE_SQL_AST_QUERY_PRIMARY, "right parenthesized operand");
    failures += expect_node(child_at(local_select, 1U), MYLITE_SQL_AST_LIMIT_CLAUSE,
                            "right parenthesized scalar local limit");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT n FROM t ORDER BY n LIMIT 1 UNION ALL SELECT n FROM t;",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT 1 UNION ALL DISTINCT SELECT 2;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1 UNION SELECT 2 ORDER BY 1 OFFSET 1;",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_aggregate_grouping_syntax(void)
{
    enum {
        aggregate_select_item_count = 6U,
        grouped_select_child_count = 7U,
        aggregate_where_child = 2U,
        aggregate_group_by_child = 3U,
        aggregate_having_child = 4U,
        aggregate_order_by_child = 5U,
        aggregate_limit_child = 6U,
        aggregate_max_item = 5U,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *group_by = NULL;
    const struct mylite_sql_ast_node *group_items = NULL;
    const struct mylite_sql_ast_node *having = NULL;
    const struct mylite_sql_ast_node *order_by = NULL;
    const struct mylite_sql_ast_node *limit = NULL;
    int failures = 0;

    failures += parse_sql("SELECT grp, COUNT(*), SUM(n) AS total, AVG(n), MIN(txt), MAX(txt) "
                          "FROM t WHERE n IS NOT NULL GROUP BY grp ASC, 1 DESC "
                          "HAVING total > 10 ORDER BY total DESC LIMIT 2;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    group_by = child_at(select, aggregate_group_by_child);
    group_items = child_at(group_by, 0U);
    having = child_at(select, aggregate_having_child);
    order_by = child_at(select, aggregate_order_by_child);
    limit = child_at(select, aggregate_limit_child);

    failures +=
        expect_child_count(select, grouped_select_child_count, "grouped select child count");
    failures +=
        expect_child_count(select_list, aggregate_select_item_count, "aggregate select item count");
    failures += expect_node(child_at(select, aggregate_where_child), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "grouping where clause");
    failures += expect_node(group_by, MYLITE_SQL_AST_GROUP_BY_CLAUSE, "group by clause");
    failures += expect_span_text(group_by, "GROUP BY grp ASC, 1 DESC", "group by span");
    failures += expect_node(group_items, MYLITE_SQL_AST_GROUP_ITEM_LIST, "group item list");
    failures += expect_child_count(group_items, 2U, "group item count");
    failures +=
        expect_span_text(child_at(child_at(group_items, 0U), 0U), "grp", "first group expression");
    failures += expect_group_item_direction(
        child_at(group_items, 0U), MYLITE_SQL_AST_KEY_PART_ORDER_ASC, "first group direction");
    failures += expect_literal(child_at(child_at(group_items, 1U), 0U),
                               MYLITE_SQL_AST_LITERAL_INTEGER, "ordinal group expression");
    failures += expect_group_item_direction(
        child_at(group_items, 1U), MYLITE_SQL_AST_KEY_PART_ORDER_DESC, "second group direction");
    failures += expect_node(having, MYLITE_SQL_AST_HAVING_CLAUSE, "having clause");
    failures +=
        expect_operator(child_at(having, 0U), MYLITE_SQL_AST_OPERATOR_GREATER, "having predicate");
    failures += expect_node(order_by, MYLITE_SQL_AST_ORDER_BY_CLAUSE, "order after having");
    failures += expect_node(limit, MYLITE_SQL_AST_LIMIT_CLAUSE, "limit after grouping");

    failures += expect_aggregate_call(
        child_at(child_at(select_list, 1U), 0U), MYLITE_SQL_AST_AGGREGATE_COUNT,
        MYLITE_SQL_AST_AGGREGATE_ARGUMENT_STAR, "COUNT", "COUNT star aggregate");
    failures +=
        expect_aggregate_call(child_at(child_at(select_list, 2U), 0U), MYLITE_SQL_AST_AGGREGATE_SUM,
                              MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION, "SUM", "SUM aggregate");
    failures += expect_span_text(child_at(child_at(child_at(select_list, 2U), 0U), 1U), "n",
                                 "SUM argument");
    failures +=
        expect_aggregate_call(child_at(child_at(select_list, 3U), 0U), MYLITE_SQL_AST_AGGREGATE_AVG,
                              MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION, "AVG", "AVG aggregate");
    failures +=
        expect_aggregate_call(child_at(child_at(select_list, 4U), 0U), MYLITE_SQL_AST_AGGREGATE_MIN,
                              MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION, "MIN", "MIN aggregate");
    failures += expect_aggregate_call(
        child_at(child_at(select_list, aggregate_max_item), 0U), MYLITE_SQL_AST_AGGREGATE_MAX,
        MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION, "MAX", "MAX aggregate");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT COUNT(n) FROM t HAVING COUNT(*) > 0 ORDER BY 1 LIMIT 1;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    having = child_at(select, 2U);
    failures += expect_aggregate_call(
        child_at(child_at(child_at(select, 0U), 0U), 0U), MYLITE_SQL_AST_AGGREGATE_COUNT,
        MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION, "COUNT", "COUNT expression aggregate");
    failures += expect_node(having, MYLITE_SQL_AST_HAVING_CLAUSE, "having without group");
    failures +=
        expect_aggregate_call(child_at(child_at(having, 0U), 0U), MYLITE_SQL_AST_AGGREGATE_COUNT,
                              MYLITE_SQL_AST_AGGREGATE_ARGUMENT_STAR, "COUNT", "having COUNT star");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT COUNT(DISTINCT n, nullable + 1) AS c FROM t;",
                          MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    failures += expect_aggregate_call(child_at(child_at(select_list, 0U), 0U),
                                      MYLITE_SQL_AST_AGGREGATE_COUNT,
                                      MYLITE_SQL_AST_AGGREGATE_ARGUMENT_DISTINCT_EXPRESSION_LIST,
                                      "COUNT", "COUNT distinct aggregate");
    failures += expect_node(child_at(child_at(child_at(select_list, 0U), 0U), 1U),
                            MYLITE_SQL_AST_EXPRESSION_LIST, "COUNT distinct arguments");
    failures += expect_child_count(child_at(child_at(child_at(select_list, 0U), 0U), 1U), 2U,
                                   "COUNT distinct argument count");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT COUNT(*);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    failures += expect_aggregate_call(
        child_at(child_at(child_at(select, 0U), 0U), 0U), MYLITE_SQL_AST_AGGREGATE_COUNT,
        MYLITE_SQL_AST_AGGREGATE_ARGUMENT_STAR, "COUNT", "no-table COUNT star");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT COUNT() FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SUM() FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT COUNT(*, n) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT COUNT(DISTINCT) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT COUNT(DISTINCT *) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SUM(DISTINCT n) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SUM(*) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a FROM t GROUP BY;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a FROM t HAVING;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT a FROM t ORDER BY a GROUP BY a;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a FROM t HAVING a > 0 GROUP BY a;", MYLITE_SQL_PARSE_SYNTAX_ERROR,
                          &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_subquery_expression_syntax(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *predicate = NULL;
    const struct mylite_sql_ast_node *row = NULL;
    const struct mylite_sql_ast_node *join = NULL;
    const struct mylite_sql_ast_node *condition = NULL;
    const struct mylite_sql_ast_node *order_items = NULL;
    int failures = 0;

    failures += parse_sql("SELECT (SELECT 1);", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    select_list = child_at(select, 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures +=
        expect_node(expression, MYLITE_SQL_AST_SUBQUERY_EXPRESSION, "projection scalar subquery");
    failures += expect_child_count(expression, 1U, "projection scalar subquery child count");
    failures += expect_node(child_at(expression, 0U), MYLITE_SQL_AST_SELECT_STATEMENT,
                            "projection scalar subquery select");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE val = "
                          "(SELECT val FROM inner_t WHERE id = 101);",
                          MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    expression = child_at(predicate, 1U);
    failures +=
        expect_node(predicate, MYLITE_SQL_AST_BINARY_EXPRESSION, "scalar comparison predicate");
    failures +=
        expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_EQUAL, "scalar comparison operator");
    failures += expect_node(expression, MYLITE_SQL_AST_SUBQUERY_EXPRESSION,
                            "scalar comparison right subquery");
    failures += expect_node(child_at(expression, 0U), MYLITE_SQL_AST_SELECT_STATEMENT,
                            "scalar comparison inner select");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE EXISTS (SELECT 1 FROM inner_t);",
                          MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    failures += expect_node(predicate, MYLITE_SQL_AST_EXISTS_EXPRESSION, "exists predicate");
    failures += expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_NONE, "exists operator");
    failures += expect_node(child_at(predicate, 0U), MYLITE_SQL_AST_SELECT_STATEMENT,
                            "exists inner select");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE NOT EXISTS (SELECT 1 FROM inner_t);",
                          MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    failures += expect_node(predicate, MYLITE_SQL_AST_EXISTS_EXPRESSION, "not exists predicate");
    failures +=
        expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT, "not exists operator");
    failures += expect_node(child_at(predicate, 0U), MYLITE_SQL_AST_SELECT_STATEMENT,
                            "not exists inner select");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE val IN (SELECT val FROM inner_t);",
                          MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    failures += expect_node(predicate, MYLITE_SQL_AST_BINARY_EXPRESSION, "in subquery predicate");
    failures += expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_IN, "in subquery operator");
    failures += expect_node(child_at(predicate, 1U), MYLITE_SQL_AST_SELECT_STATEMENT,
                            "in subquery inner select");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE val NOT IN (SELECT val FROM inner_t);",
                          MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    failures +=
        expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_NOT_IN, "not in subquery operator");
    failures += expect_node(child_at(predicate, 1U), MYLITE_SQL_AST_SELECT_STATEMENT,
                            "not in subquery inner select");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE val > ANY (SELECT val FROM inner_t);",
                          MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    failures +=
        expect_node(predicate, MYLITE_SQL_AST_QUANTIFIED_COMPARISON, "any quantified predicate");
    failures +=
        expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_GREATER, "any quantified operator");
    failures += expect_subquery_quantifier(predicate, MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ANY,
                                           "any quantified quantifier");
    failures += expect_node(child_at(predicate, 1U), MYLITE_SQL_AST_SELECT_STATEMENT,
                            "any quantified inner select");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE val <> SOME (SELECT val FROM inner_t);",
                          MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    failures +=
        expect_node(predicate, MYLITE_SQL_AST_QUANTIFIED_COMPARISON, "some quantified predicate");
    failures +=
        expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_NOT_EQUAL, "some quantified operator");
    failures += expect_subquery_quantifier(predicate, MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_SOME,
                                           "some quantified quantifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE val >= ALL (SELECT val FROM inner_t);",
                          MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    failures +=
        expect_node(predicate, MYLITE_SQL_AST_QUANTIFIED_COMPARISON, "all quantified predicate");
    failures += expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL,
                                "all quantified operator");
    failures += expect_subquery_quantifier(predicate, MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ALL,
                                           "all quantified quantifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE (val, grp) = ANY "
                          "(SELECT a, b FROM pair_t);",
                          MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    row = child_at(predicate, 0U);
    failures += expect_node(predicate, MYLITE_SQL_AST_QUANTIFIED_COMPARISON,
                            "row any quantified predicate");
    failures +=
        expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_EQUAL, "row any quantified operator");
    failures += expect_subquery_quantifier(predicate, MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ANY,
                                           "row any quantified quantifier");
    failures += expect_node(row, MYLITE_SQL_AST_ROW_CONSTRUCTOR, "row any constructor");
    failures += expect_child_count(row, 2U, "row any constructor arity");
    failures += expect_node(child_at(predicate, 1U), MYLITE_SQL_AST_SELECT_STATEMENT,
                            "row any inner select");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE (val, grp) = SOME "
                          "(SELECT a, b FROM pair_t);",
                          MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    failures += expect_node(predicate, MYLITE_SQL_AST_QUANTIFIED_COMPARISON,
                            "row some quantified predicate");
    failures +=
        expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_EQUAL, "row some quantified operator");
    failures += expect_subquery_quantifier(predicate, MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_SOME,
                                           "row some quantified quantifier");
    failures += expect_node(child_at(predicate, 0U), MYLITE_SQL_AST_ROW_CONSTRUCTOR,
                            "row some constructor");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE (val, grp) <> ALL "
                          "(SELECT a, b FROM pair_t);",
                          MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    failures += expect_node(predicate, MYLITE_SQL_AST_QUANTIFIED_COMPARISON,
                            "row not equal all quantified predicate");
    failures += expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_NOT_EQUAL,
                                "row not equal all quantified operator");
    failures += expect_subquery_quantifier(predicate, MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ALL,
                                           "row not equal all quantified quantifier");
    failures += expect_node(child_at(predicate, 0U), MYLITE_SQL_AST_ROW_CONSTRUCTOR,
                            "row not equal all constructor");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE (val, grp) != ALL "
                          "(SELECT a, b FROM pair_t);",
                          MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    failures += expect_node(predicate, MYLITE_SQL_AST_QUANTIFIED_COMPARISON,
                            "row bang not equal all quantified predicate");
    failures += expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_NOT_EQUAL,
                                "row bang not equal all quantified operator");
    failures += expect_subquery_quantifier(predicate, MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ALL,
                                           "row bang not equal all quantified quantifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE ROW(val, grp) = ANY "
                          "(SELECT a, b FROM pair_t);",
                          MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    row = child_at(predicate, 0U);
    failures += expect_node(row, MYLITE_SQL_AST_ROW_CONSTRUCTOR, "ROW any constructor");
    failures += expect_child_count(row, 2U, "ROW any constructor arity");
    failures += expect_subquery_quantifier(predicate, MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ANY,
                                           "ROW any quantified quantifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE ROW(val) = ANY (SELECT a FROM pair_t);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE (val, grp) <=> ANY "
                          "(SELECT a, b FROM pair_t);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE val <=> ANY (SELECT val FROM inner_t);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE (val, grp) IN (SELECT a, b FROM pair_t);",
                          MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    row = child_at(predicate, 0U);
    failures += expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_IN, "row in subquery operator");
    failures += expect_node(row, MYLITE_SQL_AST_ROW_CONSTRUCTOR, "parenthesized row constructor");
    failures += expect_child_count(row, 2U, "parenthesized row constructor arity");
    failures += expect_node(child_at(predicate, 1U), MYLITE_SQL_AST_SELECT_STATEMENT,
                            "row in subquery inner select");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT id FROM outer_t WHERE (val, grp) NOT IN (SELECT a, b FROM pair_t);",
                  MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    row = child_at(predicate, 0U);
    failures +=
        expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_NOT_IN, "row not in subquery operator");
    failures += expect_node(row, MYLITE_SQL_AST_ROW_CONSTRUCTOR, "row not in constructor");
    failures += expect_child_count(row, 2U, "row not in constructor arity");
    failures += expect_node(child_at(predicate, 1U), MYLITE_SQL_AST_SELECT_STATEMENT,
                            "row not in subquery inner select");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE ROW(val, grp) = "
                          "(SELECT a, b FROM pair_t WHERE a = 10);",
                          MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    row = child_at(predicate, 0U);
    expression = child_at(predicate, 1U);
    failures +=
        expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_EQUAL, "row scalar comparison operator");
    failures += expect_node(row, MYLITE_SQL_AST_ROW_CONSTRUCTOR, "ROW row constructor");
    failures += expect_child_count(row, 2U, "ROW row constructor arity");
    failures += expect_node(expression, MYLITE_SQL_AST_SUBQUERY_EXPRESSION,
                            "row scalar comparison subquery");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE (val, grp) < "
                          "(SELECT a, b FROM pair_t WHERE a = 10);",
                          MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    row = child_at(predicate, 0U);
    expression = child_at(predicate, 1U);
    failures +=
        expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_LESS, "row scalar less operator");
    failures += expect_node(row, MYLITE_SQL_AST_ROW_CONSTRUCTOR, "row less constructor");
    failures += expect_child_count(row, 2U, "row less constructor arity");
    failures += expect_node(expression, MYLITE_SQL_AST_SUBQUERY_EXPRESSION, "row less subquery");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t JOIN inner_t i ON EXISTS (SELECT 1);",
                          MYLITE_SQL_PARSE_OK, &result);
    join = child_at(child_at(child_at(child_at(result.root, 0U), 1U), 0U), 0U);
    condition = child_at(join, 2U);
    failures += expect_node(condition, MYLITE_SQL_AST_JOIN_CONDITION, "subquery join condition");
    failures += expect_node(child_at(condition, 0U), MYLITE_SQL_AST_EXISTS_EXPRESSION,
                            "subquery join predicate");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT grp, COUNT(*) FROM outer_t GROUP BY grp "
                          "HAVING COUNT(*) > (SELECT 1);",
                          MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 3U), 0U);
    failures += expect_operator(predicate, MYLITE_SQL_AST_OPERATOR_GREATER,
                                "having scalar subquery comparison");
    failures += expect_node(child_at(predicate, 1U), MYLITE_SQL_AST_SUBQUERY_EXPRESSION,
                            "having scalar subquery");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t ORDER BY "
                          "(SELECT val FROM inner_t WHERE id = 101);",
                          MYLITE_SQL_PARSE_OK, &result);
    order_items = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    failures += expect_node(child_at(child_at(order_items, 0U), 0U),
                            MYLITE_SQL_AST_SUBQUERY_EXPRESSION, "order scalar subquery");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT any, some FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures +=
        expect_span_text(child_at(child_at(select_list, 0U), 0U), "any", "ANY identifier fallback");
    failures += expect_span_text(child_at(child_at(select_list, 1U), 0U), "some",
                                 "SOME identifier fallback");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1 = any, 2 <> some FROM t;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    expression = child_at(child_at(select_list, 0U), 0U);
    failures += expect_operator(expression, MYLITE_SQL_AST_OPERATOR_EQUAL,
                                "ANY comparison identifier operator");
    failures += expect_span_text(child_at(expression, 1U), "any", "ANY comparison identifier");
    expression = child_at(child_at(select_list, 1U), 0U);
    failures += expect_operator(expression, MYLITE_SQL_AST_OPERATOR_NOT_EQUAL,
                                "SOME comparison identifier operator");
    failures += expect_span_text(child_at(expression, 1U), "some", "SOME comparison identifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE val > any /* comment */ "
                          "(SELECT val FROM inner_t);",
                          MYLITE_SQL_PARSE_OK, &result);
    predicate = child_at(child_at(child_at(result.root, 0U), 2U), 0U);
    failures += expect_node(predicate, MYLITE_SQL_AST_QUANTIFIED_COMPARISON,
                            "lowercase any quantified predicate");
    failures += expect_subquery_quantifier(predicate, MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ANY,
                                           "lowercase any quantified quantifier");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT ROW(1) = (SELECT val FROM inner_t);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE val IN ();", MYLITE_SQL_PARSE_SYNTAX_ERROR,
                          &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE EXISTS SELECT 1;",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT id FROM outer_t WHERE val > ANY SELECT val FROM inner_t;",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT (TABLE outer_t);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT (VALUES ROW(1));", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM (SELECT 1) AS dt;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    // NOLINTEND(readability-magic-numbers)
    return failures;
}

static int test_unary_and_parenthesized_expression(void)
{
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
    failures += expect_node(parenthesized, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
                            "parenthesized expression");
    failures += expect_span_text(parenthesized, "(1 + 2)", "parenthesized expression");
    failures += expect_node(add, MYLITE_SQL_AST_BINARY_EXPRESSION, "parenthesized add");
    failures += expect_operator(add, MYLITE_SQL_AST_OPERATOR_ADD, "parenthesized add");
    failures += expect_span_text(child_at(add, 0U), "1", "parenthesized add left");
    failures += expect_span_text(child_at(add, 1U), "2", "parenthesized add right");

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_literal_categories(void)
{
    enum { expected_literal_item_count = 5 };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    int failures = 0;

    failures += parse_sql("SELECT 0xabc, b'10', .25, 1e+3, N'a';", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, expected_literal_item_count, "literal select list");
    failures += expect_literal(child_at(child_at(select_list, 0U), 0U), MYLITE_SQL_AST_LITERAL_HEX,
                               "hex literal");
    failures += expect_literal(child_at(child_at(select_list, 1U), 0U), MYLITE_SQL_AST_LITERAL_BIT,
                               "bit literal");
    failures += expect_literal(child_at(child_at(select_list, 2U), 0U),
                               MYLITE_SQL_AST_LITERAL_DECIMAL, "decimal literal");
    failures += expect_literal(child_at(child_at(select_list, 3U), 0U),
                               MYLITE_SQL_AST_LITERAL_FLOAT, "float literal");
    failures += expect_literal(child_at(child_at(select_list, 4U), 0U),
                               MYLITE_SQL_AST_LITERAL_NATIONAL_STRING, "national literal");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM DUAL;", MYLITE_SQL_PARSE_OK, &result);
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, 1U, "wildcard select list");
    failures += expect_node(child_at(child_at(select_list, 0U), 0U), MYLITE_SQL_AST_WILDCARD,
                            "wildcard item");

    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT *;", MYLITE_SQL_PARSE_OK, &result);
    failures += expect_child_count(child_at(result.root, 0U), 1U, "bare wildcard select");
    select_list = child_at(child_at(result.root, 0U), 0U);
    failures += expect_child_count(select_list, 1U, "bare wildcard select list");
    failures += expect_node(child_at(child_at(select_list, 0U), 0U), MYLITE_SQL_AST_WILDCARD,
                            "bare wildcard item");

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_qualified_identifier_keyword_part(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *qualified = NULL;
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

    failures += expect_node(qualified, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "commented qualified identifier");
    failures += expect_span_text(child_at(qualified, 0U), "mydb", "commented qualified left");
    failures += expect_span_text(child_at(qualified, 1U), "select", "commented qualified right");

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int test_comments_are_skipped(void)
{
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

static int test_syntax_errors(void)
{
    struct mylite_sql_parse_result result;
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

    failures += parse_sql("SELECT 1 WHERE TRUE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT 1, * FROM DUAL;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_lexer_errors(void)
{
    struct mylite_sql_parse_result result;
    int failures = 0;

    failures += parse_sql("SELECT 'unterminated", MYLITE_SQL_PARSE_LEXER_ERROR, &result);
    if (result.error_token.error != MYLITE_SQL_LEXER_ERROR_UNTERMINATED_STRING) {
        fprintf(stderr, "expected unterminated string lexer error, got %s\n",
                mylite_sql_lexer_error_name(result.error_token.error));
        failures = 1;
    }

    mylite_sql_parse_result_deinit(&result);
    return failures;
}

static int parse_sql(const char *sql, enum mylite_sql_parse_status expected_status,
                     struct mylite_sql_parse_result *out_result)
{
    enum mylite_sql_parse_status actual = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = strlen(sql),
            .modes = 0U,
        },
        out_result);

    if (actual != expected_status) {
        fprintf(stderr, "parse '%s': expected %s, got %s\n", sql,
                mylite_sql_parse_status_name(expected_status),
                mylite_sql_parse_status_name(actual));
        return 1;
    }

    return 0;
}

static const struct mylite_sql_ast_node *child_at(const struct mylite_sql_ast_node *node,
                                                  size_t index)
{
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

static int expect_node(const struct mylite_sql_ast_node *node,
                       enum mylite_sql_ast_node_kind expected_kind, const char *context)
{
    if (node == NULL) {
        fprintf(stderr, "%s: expected %s, got null\n", context,
                mylite_sql_ast_node_kind_name(expected_kind));
        return 1;
    }

    if (node->kind != expected_kind) {
        fprintf(stderr, "%s: expected %s, got %s\n", context,
                mylite_sql_ast_node_kind_name(expected_kind),
                mylite_sql_ast_node_kind_name(node->kind));
        return 1;
    }

    return 0;
}

static int expect_child_count(const struct mylite_sql_ast_node *node, size_t expected,
                              const char *context)
{
    size_t actual = mylite_sql_ast_node_child_count(node);

    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu children, got %zu\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_bool(bool actual, bool expected, const char *context)
{
    const char *actual_text = "false";
    const char *expected_text = "false";

    if (actual) {
        actual_text = "true";
    }
    if (expected) {
        expected_text = "true";
    }
    if (actual != expected) {
        fprintf(stderr, "%s: expected %s, got %s\n", context, expected_text, actual_text);
        return 1;
    }

    return 0;
}

static int expect_span_text(const struct mylite_sql_ast_node *node, const char *expected,
                            const char *context)
{
    size_t expected_length = strlen(expected);

    if (node == NULL) {
        fprintf(stderr, "%s: expected span '%s', got null node\n", context, expected);
        return 1;
    }

    if (node->span.length != expected_length ||
        (expected_length > 0U && memcmp(node->span.text, expected, expected_length) != 0)) {
        fprintf(stderr, "%s: expected span '%s', got '%.*s'\n", context, expected,
                (int)node->span.length, node->span.text == NULL ? "" : node->span.text);
        return 1;
    }

    return 0;
}

static int expect_function_call(const struct mylite_sql_ast_node *node, const char *expected_name,
                                size_t expected_arg_count, const char *context)
{
    const struct mylite_sql_ast_node *name = NULL;
    const struct mylite_sql_ast_node *arguments = NULL;
    int failures = expect_node(node, MYLITE_SQL_AST_FUNCTION_CALL, context);

    if (node == NULL) {
        return failures;
    }

    failures += expect_child_count(node, 2U, context);
    name = child_at(node, 0U);
    arguments = child_at(node, 1U);
    failures += expect_node(name, MYLITE_SQL_AST_IDENTIFIER, context);
    failures += expect_span_text(name, expected_name, context);
    failures += expect_node(arguments, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, context);
    failures += expect_child_count(arguments, expected_arg_count, context);
    return failures;
}

static int expect_aggregate_call(const struct mylite_sql_ast_node *node,
                                 enum mylite_sql_ast_aggregate_kind expected_kind,
                                 enum mylite_sql_ast_aggregate_argument expected_argument,
                                 const char *expected_name, const char *context)
{
    const struct mylite_sql_ast_node *name = NULL;
    int failures = expect_node(node, MYLITE_SQL_AST_AGGREGATE_CALL, context);

    if (node == NULL) {
        return failures;
    }

    failures += expect_child_count(node, 2U, context);
    name = child_at(node, 0U);
    failures += expect_node(name, MYLITE_SQL_AST_IDENTIFIER, context);
    failures += expect_span_text(name, expected_name, context);
    if (node->aggregate_kind != expected_kind) {
        fprintf(stderr, "%s: expected aggregate kind %s, got %s\n", context,
                mylite_sql_ast_aggregate_kind_name(expected_kind),
                mylite_sql_ast_aggregate_kind_name(node->aggregate_kind));
        failures = 1;
    }
    if (node->aggregate_argument != expected_argument) {
        fprintf(stderr, "%s: expected aggregate argument %s, got %s\n", context,
                mylite_sql_ast_aggregate_argument_name(expected_argument),
                mylite_sql_ast_aggregate_argument_name(node->aggregate_argument));
        failures = 1;
    }
    return failures;
}

static int expect_join_type(const struct mylite_sql_ast_node *node,
                            enum mylite_sql_ast_join_type expected, const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_JOIN_EXPRESSION, context);

    if (node != NULL && node->join_type != expected) {
        fprintf(stderr, "%s: expected join type %s, got %s\n", context,
                mylite_sql_ast_join_type_name(expected),
                mylite_sql_ast_join_type_name(node->join_type));
        failures = 1;
    }

    return failures;
}

static int expect_join_condition_type(const struct mylite_sql_ast_node *node,
                                      enum mylite_sql_ast_join_condition_type expected,
                                      const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_JOIN_CONDITION, context);

    if (node != NULL && node->join_condition_type != expected) {
        fprintf(stderr, "%s: expected join condition %s, got %s\n", context,
                mylite_sql_ast_join_condition_type_name(expected),
                mylite_sql_ast_join_condition_type_name(node->join_condition_type));
        failures = 1;
    }

    return failures;
}

static int expect_select_duplicate_mode(const struct mylite_sql_ast_node *node,
                                        enum mylite_sql_ast_select_duplicate_mode expected,
                                        bool explicit_mode, bool conflict, size_t modifier_count,
                                        const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_SELECT_STATEMENT, context);

    if (node == NULL) {
        return failures;
    }
    if (node->select_duplicate_mode != expected) {
        fprintf(stderr, "%s: expected duplicate mode %s, got %s\n", context,
                mylite_sql_ast_select_duplicate_mode_name(expected),
                mylite_sql_ast_select_duplicate_mode_name(node->select_duplicate_mode));
        failures = 1;
    }
    failures += expect_bool(node->select_duplicate_mode_explicit, explicit_mode, context);
    failures += expect_bool(node->select_duplicate_mode_conflict, conflict, context);
    if (node->select_duplicate_modifier_count != modifier_count) {
        fprintf(stderr, "%s: expected %zu duplicate modifiers, got %zu\n", context, modifier_count,
                node->select_duplicate_modifier_count);
        failures = 1;
    }
    return failures;
}

static int expect_union_duplicate_mode(const struct mylite_sql_ast_node *node,
                                       enum mylite_sql_ast_set_duplicate_mode expected,
                                       const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_UNION_EXPRESSION, context);

    if (node != NULL && node->set_duplicate_mode != expected) {
        fprintf(stderr, "%s: expected union duplicate mode %s, got %s\n", context,
                mylite_sql_ast_set_duplicate_mode_name(expected),
                mylite_sql_ast_set_duplicate_mode_name(node->set_duplicate_mode));
        failures = 1;
    }
    return failures;
}

static int expect_subquery_quantifier(const struct mylite_sql_ast_node *node,
                                      enum mylite_sql_ast_subquery_quantifier expected,
                                      const char *context)
{
    if (node == NULL) {
        fprintf(stderr, "%s: expected subquery quantifier %s, got null node\n", context,
                mylite_sql_ast_subquery_quantifier_name(expected));
        return 1;
    }

    if (node->subquery_quantifier != expected) {
        fprintf(stderr, "%s: expected subquery quantifier %s, got %s\n", context,
                mylite_sql_ast_subquery_quantifier_name(expected),
                mylite_sql_ast_subquery_quantifier_name(node->subquery_quantifier));
        return 1;
    }

    return 0;
}

static int expect_literal(const struct mylite_sql_ast_node *node,
                          enum mylite_sql_ast_literal_kind expected, const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_LITERAL, context);

    if (node != NULL && node->literal_kind != expected) {
        fprintf(stderr, "%s: expected literal %s, got %s\n", context,
                mylite_sql_ast_literal_kind_name(expected),
                mylite_sql_ast_literal_kind_name(node->literal_kind));
        failures = 1;
    }

    return failures;
}

static int expect_operator(const struct mylite_sql_ast_node *node,
                           enum mylite_sql_ast_operator expected, const char *context)
{
    if (node == NULL) {
        fprintf(stderr, "%s: expected operator %s, got null node\n", context,
                mylite_sql_ast_operator_name(expected));
        return 1;
    }

    if (node->operator_kind != expected) {
        fprintf(stderr, "%s: expected operator %s, got %s\n", context,
                mylite_sql_ast_operator_name(expected),
                mylite_sql_ast_operator_name(node->operator_kind));
        return 1;
    }

    return 0;
}

static int expect_schema_option(const struct mylite_sql_ast_node *node,
                                enum mylite_sql_ast_schema_option expected, const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_SCHEMA_OPTION, context);

    if (node != NULL && node->schema_option != expected) {
        fprintf(stderr, "%s: expected schema option %s, got %s\n", context,
                mylite_sql_ast_schema_option_name(expected),
                mylite_sql_ast_schema_option_name(node->schema_option));
        failures = 1;
    }

    return failures;
}

static int expect_column_type(const struct mylite_sql_ast_node *node,
                              enum mylite_sql_ast_column_type expected, const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_COLUMN_TYPE, context);

    if (node != NULL && node->column_type != expected) {
        fprintf(stderr, "%s: expected column type %s, got %s\n", context,
                mylite_sql_ast_column_type_name(expected),
                mylite_sql_ast_column_type_name(node->column_type));
        failures = 1;
    }

    return failures;
}

static int expect_column_attribute(const struct mylite_sql_ast_node *node,
                                   enum mylite_sql_ast_column_attribute expected,
                                   const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_COLUMN_ATTRIBUTE, context);

    if (node != NULL && node->column_attribute != expected) {
        fprintf(stderr, "%s: expected column attribute %s, got %s\n", context,
                mylite_sql_ast_column_attribute_name(expected),
                mylite_sql_ast_column_attribute_name(node->column_attribute));
        failures = 1;
    }

    return failures;
}

static int expect_column_format(const struct mylite_sql_ast_node *node,
                                enum mylite_sql_ast_column_format expected, const char *context)
{
    int failures =
        expect_column_attribute(node, MYLITE_SQL_AST_COLUMN_ATTRIBUTE_COLUMN_FORMAT, context);

    if (node != NULL && node->column_format != expected) {
        fprintf(stderr, "%s: expected column format %s, got %s\n", context,
                mylite_sql_ast_column_format_name(expected),
                mylite_sql_ast_column_format_name(node->column_format));
        failures = 1;
    }

    return failures;
}

static int expect_column_storage(const struct mylite_sql_ast_node *node,
                                 enum mylite_sql_ast_column_storage expected, const char *context)
{
    int failures = expect_column_attribute(node, MYLITE_SQL_AST_COLUMN_ATTRIBUTE_STORAGE, context);

    if (node != NULL && node->column_storage != expected) {
        fprintf(stderr, "%s: expected column storage %s, got %s\n", context,
                mylite_sql_ast_column_storage_name(expected),
                mylite_sql_ast_column_storage_name(node->column_storage));
        failures = 1;
    }

    return failures;
}

static int expect_key_part_order(const struct mylite_sql_ast_node *node,
                                 enum mylite_sql_ast_key_part_order expected, const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_KEY_PART, context);

    if (node != NULL && node->key_part_order != expected) {
        fprintf(stderr, "%s: expected key part order %s, got %s\n", context,
                mylite_sql_ast_key_part_order_name(expected),
                mylite_sql_ast_key_part_order_name(node->key_part_order));
        failures = 1;
    }

    return failures;
}

static int expect_order_item_direction(const struct mylite_sql_ast_node *node,
                                       enum mylite_sql_ast_key_part_order expected,
                                       const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_ORDER_ITEM, context);

    if (node != NULL && node->key_part_order != expected) {
        fprintf(stderr, "%s: expected order direction %s, got %s\n", context,
                mylite_sql_ast_key_part_order_name(expected),
                mylite_sql_ast_key_part_order_name(node->key_part_order));
        failures = 1;
    }

    return failures;
}

static int expect_group_item_direction(const struct mylite_sql_ast_node *node,
                                       enum mylite_sql_ast_key_part_order expected,
                                       const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_GROUP_ITEM, context);

    if (node != NULL && node->key_part_order != expected) {
        fprintf(stderr, "%s: expected group direction %s, got %s\n", context,
                mylite_sql_ast_key_part_order_name(expected),
                mylite_sql_ast_key_part_order_name(node->key_part_order));
        failures = 1;
    }

    return failures;
}

static int expect_limit_bound(const struct mylite_sql_ast_node *node, uint64_t expected,
                              const char *context)
{
    uint64_t actual = 0U;
    int failures = expect_node(node, MYLITE_SQL_AST_LIMIT_BOUND, context);

    if (node != NULL && node->has_limit_bound_value) {
        actual = node->limit_bound_value;
    }
    if (node != NULL && (!node->has_limit_bound_value || actual != expected)) {
        fprintf(stderr, "%s: expected limit bound %llu, got %llu\n", context,
                (unsigned long long)expected, (unsigned long long)actual);
        failures = 1;
    }

    return failures;
}

static int expect_index_algorithm(const struct mylite_sql_ast_node *node,
                                  enum mylite_sql_ast_index_algorithm expected, const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_INDEX_TYPE, context);

    if (node != NULL && node->index_algorithm != expected) {
        fprintf(stderr, "%s: expected index algorithm %s, got %s\n", context,
                mylite_sql_ast_index_algorithm_name(expected),
                mylite_sql_ast_index_algorithm_name(node->index_algorithm));
        failures = 1;
    }

    return failures;
}

static int expect_index_option(const struct mylite_sql_ast_node *node,
                               enum mylite_sql_ast_index_option expected, const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_INDEX_OPTION, context);

    if (node != NULL && node->index_option != expected) {
        fprintf(stderr, "%s: expected index option %s, got %s\n", context,
                mylite_sql_ast_index_option_name(expected),
                mylite_sql_ast_index_option_name(node->index_option));
        failures = 1;
    }

    return failures;
}

static int expect_index_class(const struct mylite_sql_ast_node *node,
                              enum mylite_sql_ast_index_class expected, const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_CREATE_INDEX_STATEMENT, context);

    if (node != NULL && node->index_class != expected) {
        fprintf(stderr, "%s: expected index class %s, got %s\n", context,
                mylite_sql_ast_index_class_name(expected),
                mylite_sql_ast_index_class_name(node->index_class));
        failures = 1;
    }

    return failures;
}

static int expect_ddl_table_option(const struct mylite_sql_ast_node *node,
                                   enum mylite_sql_ast_ddl_table_option expected,
                                   const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_DDL_TABLE_OPTION, context);

    if (node != NULL && node->ddl_table_option != expected) {
        fprintf(stderr, "%s: expected DDL table option %s, got %s\n", context,
                mylite_sql_ast_ddl_table_option_name(expected),
                mylite_sql_ast_ddl_table_option_name(node->ddl_table_option));
        failures = 1;
    }

    return failures;
}

static int expect_alter_table_action(const struct mylite_sql_ast_node *node,
                                     enum mylite_sql_ast_alter_table_action expected,
                                     bool column_keyword, const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_ALTER_TABLE_ACTION, context);

    if (node != NULL && node->alter_table_action != expected) {
        fprintf(stderr, "%s: expected ALTER TABLE action %s, got %s\n", context,
                mylite_sql_ast_alter_table_action_name(expected),
                mylite_sql_ast_alter_table_action_name(node->alter_table_action));
        failures = 1;
    }
    if (node != NULL && node->alter_table_action_column_keyword != column_keyword) {
        const char *actual_keyword_text = "false";
        const char *expected_keyword_text = "false";

        if (node->alter_table_action_column_keyword) {
            actual_keyword_text = "true";
        }
        if (column_keyword) {
            expected_keyword_text = "true";
        }
        fprintf(stderr, "%s: expected ALTER TABLE COLUMN keyword %s, got %s\n", context,
                expected_keyword_text, actual_keyword_text);
        failures = 1;
    }

    return failures;
}

static int
expect_alter_table_column_position(const struct mylite_sql_ast_node *node,
                                   enum mylite_sql_ast_alter_table_column_position expected,
                                   const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION, context);

    if (node != NULL && node->alter_table_column_position != expected) {
        fprintf(stderr, "%s: expected ALTER TABLE column position %s, got %s\n", context,
                mylite_sql_ast_alter_table_column_position_name(expected),
                mylite_sql_ast_alter_table_column_position_name(node->alter_table_column_position));
        failures = 1;
    }

    return failures;
}

static int expect_table_option(const struct mylite_sql_ast_node *node,
                               enum mylite_sql_ast_table_option expected, const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_TABLE_OPTION, context);

    if (node != NULL && node->table_option != expected) {
        fprintf(stderr, "%s: expected table option %d, got %d\n", context, (int)expected,
                (int)node->table_option);
        failures = 1;
    }

    return failures;
}

static int expect_transaction_access_mode(const struct mylite_sql_ast_node *node,
                                          enum mylite_sql_ast_transaction_access_mode expected,
                                          const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC, context);

    if (node != NULL && node->transaction_access_mode != expected) {
        fprintf(stderr, "%s: expected transaction access mode %d, got %d\n", context, (int)expected,
                (int)node->transaction_access_mode);
        failures = 1;
    }

    return failures;
}

static int expect_transaction_completion(const struct mylite_sql_ast_node *node,
                                         enum mylite_sql_ast_transaction_chain expected_chain,
                                         enum mylite_sql_ast_transaction_release expected_release,
                                         const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_TRANSACTION_COMPLETION, context);

    if (node != NULL && node->transaction_chain != expected_chain) {
        fprintf(stderr, "%s: expected transaction chain %d, got %d\n", context, (int)expected_chain,
                (int)node->transaction_chain);
        failures = 1;
    }
    if (node != NULL && node->transaction_release != expected_release) {
        fprintf(stderr, "%s: expected transaction release %d, got %d\n", context,
                (int)expected_release, (int)node->transaction_release);
        failures = 1;
    }

    return failures;
}

static int expect_current_timestamp(const struct mylite_sql_ast_node *node, bool has_precision,
                                    uint64_t precision, const char *context)
{
    int failures = expect_node(node, MYLITE_SQL_AST_CURRENT_TIMESTAMP, context);

    if (node != NULL && node->has_column_precision != has_precision) {
        fprintf(stderr, "%s: expected current timestamp precision flag %d, got %d\n", context,
                (int)has_precision, (int)node->has_column_precision);
        failures = 1;
    }

    if (node != NULL && has_precision && node->column_precision != precision) {
        fprintf(stderr, "%s: expected current timestamp precision %llu, got %llu\n", context,
                (unsigned long long)precision, (unsigned long long)node->column_precision);
        failures = 1;
    }

    return failures;
}
