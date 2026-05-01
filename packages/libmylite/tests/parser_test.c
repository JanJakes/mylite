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
static int test_select_expression_list(void);
static int test_information_schema_select(void);
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

int main(void)
{
    int failures = 0;

    failures += test_empty_script();
    failures += test_use_statements();
    failures += test_schema_lifecycle_statements();
    failures += test_connection_charset_statements();
    failures += test_create_table_integer_boolean_columns();
    failures += test_create_table_string_binary_columns();
    failures += test_select_expression_list();
    failures += test_information_schema_select();
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

    failures += parse_sql("CREATE TABLE unsupported_attributes (a INT NOT NULL);",
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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
                          MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
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
