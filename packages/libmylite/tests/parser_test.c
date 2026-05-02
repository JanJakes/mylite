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
static int test_drop_table_syntax(void);
static int test_insert_values_syntax(void);
static int test_insert_set_syntax(void);
static int test_update_single_table_syntax(void);
static int test_delete_single_table_syntax(void);
static int test_transaction_statement_syntax(void);
static int test_savepoint_statement_syntax(void);
static int test_select_expression_list(void);
static int test_expression_operator_foundation_syntax(void);
static int test_information_schema_select(void);
static int test_select_table_core_syntax(void);
static int test_select_where_clause_syntax(void);
static int test_select_order_limit_offset_syntax(void);
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
static int expect_span_text(const struct mylite_sql_ast_node *node, const char *expected,
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
static int expect_limit_bound(const struct mylite_sql_ast_node *node, uint64_t expected,
                              const char *context);
static int expect_index_algorithm(const struct mylite_sql_ast_node *node,
                                  enum mylite_sql_ast_index_algorithm expected,
                                  const char *context);
static int expect_index_option(const struct mylite_sql_ast_node *node,
                               enum mylite_sql_ast_index_option expected, const char *context);
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
    failures += test_drop_table_syntax();
    failures += test_insert_values_syntax();
    failures += test_insert_set_syntax();
    failures += test_update_single_table_syntax();
    failures += test_delete_single_table_syntax();
    failures += test_transaction_statement_syntax();
    failures += test_savepoint_statement_syntax();
    failures += test_select_expression_list();
    failures += test_expression_operator_foundation_syntax();
    failures += test_information_schema_select();
    failures += test_select_table_core_syntax();
    failures += test_select_where_clause_syntax();
    failures += test_select_order_limit_offset_syntax();
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

    failures +=
        parse_sql("INSERT IGNORE INTO t VALUES (1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT LOW_PRIORITY INTO t VALUES (1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("INSERT DELAYED INTO t VALUES (1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t VALUES (1) ON DUPLICATE KEY UPDATE a = 1;",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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

    failures += parse_sql("INSERT IGNORE INTO t SET a = 1", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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

    failures += parse_sql("INSERT INTO t SET a = 1 AS new", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("INSERT INTO t SET a = 1 ON DUPLICATE KEY UPDATE a = 2",
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

    failures +=
        parse_sql("UPDATE t SET a = LAST_INSERT_ID(a + 1)", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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

static int test_information_schema_select(void)
{
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *select = NULL;
    const struct mylite_sql_ast_node *from_table = NULL;
    const struct mylite_sql_ast_node *qualified = NULL;
    int failures = 0;

    failures +=
        parse_sql("SELECT * FROM INFORMATION_SCHEMA.SCHEMATA;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_node(select, MYLITE_SQL_AST_SELECT_STATEMENT, "information schema select");
    failures += expect_node(from_table, MYLITE_SQL_AST_FROM_TABLE, "information schema from");
    failures += expect_node(qualified, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
                            "information schema table name");
    failures += expect_span_text(child_at(qualified, 0U), "INFORMATION_SCHEMA",
                                 "information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "SCHEMATA", "information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parse_sql("SELECT * FROM `information_schema`.`STATISTICS`;", MYLITE_SQL_PARSE_OK, &result);
    select = child_at(result.root, 0U);
    from_table = child_at(select, 1U);
    qualified = child_at(from_table, 0U);
    failures += expect_span_text(child_at(qualified, 0U), "`information_schema`",
                                 "quoted information schema qualifier");
    failures += expect_span_text(child_at(qualified, 1U), "`STATISTICS`",
                                 "quoted information schema table");
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA;",
                          MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT * FROM INFORMATION_SCHEMA.SCHEMATA WHERE TRUE;",
                          MYLITE_SQL_PARSE_OK, &result);
    failures += expect_node(child_at(child_at(result.root, 0U), 2U), MYLITE_SQL_AST_WHERE_CLAUSE,
                            "information schema where clause");
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

    failures += parse_sql("SELECT a FROM t GROUP BY a;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a FROM t JOIN u;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT COUNT(*) FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parse_sql("SELECT a INTO @x FROM t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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

    failures += parse_sql("SELECT id FROM t JOIN t AS u WHERE t.id = u.id;",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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
