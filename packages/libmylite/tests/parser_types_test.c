#include "parser_test_support.h"

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
static int test_sysdate_function_statements(void);

int main(void) {
    int failures = 0;

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
    failures += test_sysdate_function_statements();

    return failures == 0 ? 0 : 1;
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

    failures += parser_test_parse_sql(
        "CREATE TABLE string_types (v0 VARCHAR(0), label VARCHAR(255) NOT NULL DEFAULT 'tag', "
        "alias CHARACTER VARYING(3), short_alias CHAR VARYING(4));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(columns, 4U, "varchar column list");
    column = parser_test_child_at(columns, 0U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "v0",
        "varchar zero column name"
    );
    failures += parser_test_expect_varchar_type(column_type, "0", "varchar zero column type");
    failures += parser_test_expect_span_text(column_type, "VARCHAR(0)", "varchar zero span");
    column = parser_test_child_at(columns, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "label",
        "varchar max column name"
    );
    failures += parser_test_expect_varchar_type(column_type, "255", "varchar max column type");
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "varchar max not null"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_STRING,
        "varchar string default"
    );
    column = parser_test_child_at(columns, 2U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_varchar_type(column_type, "3", "character varying column type");
    failures +=
        parser_test_expect_span_text(column_type, "CHARACTER VARYING(3)", "character varying span");
    column = parser_test_child_at(columns, 3U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_varchar_type(column_type, "4", "char varying column type");
    failures += parser_test_expect_span_text(column_type, "CHAR VARYING(4)", "char varying span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
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
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(
        columns,
        national_varchar_column_count,
        "national varchar column list"
    );
    column = parser_test_child_at(columns, 0U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_national_varchar_type(column_type, "5", "nvarchar column type");
    failures += parser_test_expect_span_text(column_type, "NVARCHAR(5)", "nvarchar span");
    column = parser_test_child_at(columns, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures +=
        parser_test_expect_national_varchar_type(column_type, "6", "national varchar column type");
    failures +=
        parser_test_expect_span_text(column_type, "NATIONAL VARCHAR(6)", "national varchar span");
    column = parser_test_child_at(columns, 2U);
    column_type = parser_test_child_at(column, 1U);
    failures +=
        parser_test_expect_national_varchar_type(column_type, "7", "nchar varchar column type");
    failures += parser_test_expect_span_text(column_type, "NCHAR VARCHAR(7)", "nchar varchar span");
    column = parser_test_child_at(columns, 3U);
    column_type = parser_test_child_at(column, 1U);
    failures +=
        parser_test_expect_national_varchar_type(column_type, "8", "nchar varying column type");
    failures += parser_test_expect_span_text(column_type, "NCHAR VARYING(8)", "nchar varying span");
    column = parser_test_child_at(columns, 4U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_national_varchar_type(
        column_type,
        "9",
        "national char varying column type"
    );
    failures += parser_test_expect_span_text(
        column_type,
        "NATIONAL CHAR VARYING(9)",
        "national char varying span"
    );
    column = parser_test_child_at(columns, national_varchar_last_column_index);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_national_varchar_type(
        column_type,
        "10",
        "national character varying column type"
    );
    failures += parser_test_expect_span_text(
        column_type,
        "NATIONAL CHARACTER VARYING(10)",
        "national character varying span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle ADD COLUMN label VARCHAR(12) NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_varchar_type(column_type, "12", "alter add varchar column type");
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "alter add varchar not null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col VARCHAR(15) NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures +=
        parser_test_expect_varchar_type(column_type, "15", "varchar alter modify column type");
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "varchar alter modify nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_text VARCHAR(20) NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 2U);
    column_type = parser_test_child_at(column, 1U);
    failures +=
        parser_test_expect_varchar_type(column_type, "20", "varchar alter change column type");
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "varchar alter change nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO simple_lifecycle VALUES ('text');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "string insert value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE bad_nvarchar (c NVARCHAR);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
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

    failures += parser_test_parse_sql(
        "CREATE TABLE char_types (c CHAR, c0 CHAR(0), c255 CHAR(255) NOT NULL DEFAULT 'z', "
        "alias CHARACTER, alias2 CHARACTER(2));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(columns, char_column_count, "char column list");
    column = parser_test_child_at(columns, 0U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "c",
        "bare char column name"
    );
    failures += parser_test_expect_char_type(column_type, NULL, 0, "bare char column type");
    failures += parser_test_expect_span_text(column_type, "CHAR", "bare char span");
    column = parser_test_child_at(columns, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "c0",
        "char zero column name"
    );
    failures += parser_test_expect_char_type(column_type, "0", 1, "char zero column type");
    failures += parser_test_expect_span_text(column_type, "CHAR(0)", "char zero span");
    column = parser_test_child_at(columns, 2U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "c255",
        "char max column name"
    );
    failures += parser_test_expect_char_type(column_type, "255", 1, "char max column type");
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "char max not null"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_STRING,
        "char string default"
    );
    column = parser_test_child_at(columns, 3U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_char_type(column_type, NULL, 0, "bare character alias type");
    failures += parser_test_expect_span_text(column_type, "CHARACTER", "bare character alias span");
    column = parser_test_child_at(columns, 4U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_char_type(column_type, "2", 1, "character alias length type");
    failures +=
        parser_test_expect_span_text(column_type, "CHARACTER(2)", "character alias length span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
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
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(
        columns,
        national_char_column_count,
        "national char column list"
    );
    column = parser_test_child_at(columns, 0U);
    column_type = parser_test_child_at(column, 1U);
    failures +=
        parser_test_expect_national_char_type(column_type, NULL, 0, "bare nchar column type");
    failures += parser_test_expect_span_text(column_type, "NCHAR", "bare nchar span");
    column = parser_test_child_at(columns, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures +=
        parser_test_expect_national_char_type(column_type, "2", 1, "nchar length column type");
    failures += parser_test_expect_span_text(column_type, "NCHAR(2)", "nchar length span");
    column = parser_test_child_at(columns, 2U);
    column_type = parser_test_child_at(column, 1U);
    failures +=
        parser_test_expect_national_char_type(column_type, NULL, 0, "national char column type");
    failures += parser_test_expect_span_text(column_type, "NATIONAL CHAR", "national char span");
    column = parser_test_child_at(columns, 3U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_national_char_type(
        column_type,
        "3",
        1,
        "national char length column type"
    );
    failures +=
        parser_test_expect_span_text(column_type, "NATIONAL CHAR(3)", "national char length span");
    column = parser_test_child_at(columns, 4U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_national_char_type(
        column_type,
        NULL,
        0,
        "national character column type"
    );
    failures +=
        parser_test_expect_span_text(column_type, "NATIONAL CHARACTER", "national character span");
    column = parser_test_child_at(columns, national_char_last_column_index);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_national_char_type(
        column_type,
        "4",
        1,
        "national character length column type"
    );
    failures += parser_test_expect_span_text(
        column_type,
        "NATIONAL CHARACTER(4)",
        "national character length span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle ADD COLUMN code CHAR(2) NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_char_type(column_type, "2", 1, "alter add char column type");
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "alter add char not null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col CHAR(3) NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_char_type(column_type, "3", 1, "char alter modify column type");
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "char alter modify nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_code CHAR(4) NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 2U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_char_type(column_type, "4", 1, "char alter change column type");
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "char alter change nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE bad_char (c CHAR());",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE bad_char (c CHAR(-1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE bad_char (c CHARACTER VARYING);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE bad_char (c CHAR VARYING);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE bad_char (c CHARACTER());",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE bad_char (c NCHAR());",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE bad_char (c NCHAR(-1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
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

    failures += parser_test_parse_sql(
        "CREATE TABLE text_types (tt TINYTEXT, t TEXT, mt MEDIUMTEXT NOT NULL, lt LONGTEXT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(columns, 4U, "text family column list");

    column = parser_test_child_at(columns, 0U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "tt",
        "tinytext column name"
    );
    failures += parser_test_expect_text_type(
        column_type,
        MYLITE_SQL_AST_TEXT_TYPE_TINYTEXT,
        "tinytext column type"
    );
    failures += parser_test_expect_span_text(column_type, "TINYTEXT", "tinytext span");

    column = parser_test_child_at(columns, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures +=
        parser_test_expect_span_text(parser_test_child_at(column, 0U), "t", "text column name");
    failures += parser_test_expect_text_type(
        column_type,
        MYLITE_SQL_AST_TEXT_TYPE_TEXT,
        "text column type"
    );
    failures += parser_test_expect_span_text(column_type, "TEXT", "text span");

    column = parser_test_child_at(columns, 2U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "mt",
        "mediumtext column name"
    );
    failures += parser_test_expect_text_type(
        column_type,
        MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT,
        "mediumtext column type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "mediumtext not null"
    );

    column = parser_test_child_at(columns, 3U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "lt",
        "longtext column name"
    );
    failures += parser_test_expect_text_type(
        column_type,
        MYLITE_SQL_AST_TEXT_TYPE_LONGTEXT,
        "longtext column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE long_aliases (a LONG, b LONG VARCHAR NOT NULL, c LONG VARBINARY);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(columns, 3U, "long alias column list");
    column = parser_test_child_at(columns, 0U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "a",
        "long alias column name"
    );
    failures += parser_test_expect_text_type(
        column_type,
        MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT,
        "long alias mediumtext type"
    );
    failures += parser_test_expect_span_text(column_type, "LONG", "long alias span");
    column = parser_test_child_at(columns, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "b",
        "long varchar alias column name"
    );
    failures += parser_test_expect_text_type(
        column_type,
        MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT,
        "long varchar alias mediumtext type"
    );
    failures +=
        parser_test_expect_span_text(column_type, "LONG VARCHAR", "long varchar alias span");
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "long varchar alias not null"
    );
    column = parser_test_child_at(columns, 2U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "c",
        "long varbinary alias column name"
    );
    failures += parser_test_expect_binary_string_type(
        column_type,
        MYLITE_SQL_AST_BINARY_STRING_TYPE_MEDIUMBLOB,
        "long varbinary alias mediumblob type"
    );
    failures +=
        parser_test_expect_span_text(column_type, "LONG VARBINARY", "long varbinary alias span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE text (text INT, body TEXT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "text",
        "nonreserved text table name"
    );
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(columns, 2U, "nonreserved text identifier columns");
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "text",
        "nonreserved text column name"
    );
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "nonreserved text column integer type"
    );
    column = parser_test_child_at(columns, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "body",
        "nonreserved text body column name"
    );
    failures += parser_test_expect_text_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_TEXT_TYPE_TEXT,
        "nonreserved text body type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE long (long INT, body LONG);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "long",
        "nonreserved long table name"
    );
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(columns, 2U, "nonreserved long identifier columns");
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "long",
        "nonreserved long column name"
    );
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "nonreserved long column integer type"
    );
    column = parser_test_child_at(columns, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "body",
        "nonreserved long body name"
    );
    failures += parser_test_expect_text_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT,
        "nonreserved long body alias type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE text_lengths (a TEXT(0), b TEXT(63), c TEXT(4294967295));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(columns, 3U, "text length column list");
    column = parser_test_child_at(columns, 0U);
    column_type = parser_test_child_at(column, 1U);
    failures +=
        parser_test_expect_text_type(column_type, MYLITE_SQL_AST_TEXT_TYPE_TEXT, "text(0) type");
    failures += parser_test_expect_span_text(column_type, "TEXT(0)", "text(0) span");
    failures += parser_test_expect_text_type_length(column_type, "0", "text(0) length");
    column = parser_test_child_at(columns, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures +=
        parser_test_expect_text_type(column_type, MYLITE_SQL_AST_TEXT_TYPE_TEXT, "text(63) type");
    failures += parser_test_expect_text_type_length(column_type, "63", "text(63) length");
    column = parser_test_child_at(columns, 2U);
    column_type = parser_test_child_at(column, 1U);
    failures +=
        parser_test_expect_text_type_length(column_type, "4294967295", "text maximum length");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle ADD COLUMN body TEXT NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_text_type(
        column_type,
        MYLITE_SQL_AST_TEXT_TYPE_TEXT,
        "alter add text column type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "alter add text not null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY body MEDIUMTEXT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_text_type(
        column_type,
        MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT,
        "alter modify mediumtext column type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "alter modify mediumtext nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_body new_body LONGTEXT NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 2U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_text_type(
        column_type,
        MYLITE_SQL_AST_TEXT_TYPE_LONGTEXT,
        "alter change longtext column type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "alter change longtext nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle ADD COLUMN body TEXT(255) NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_text_type_length(column_type, "255", "alter add text length");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY body TEXT(65536) NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures +=
        parser_test_expect_text_type_length(column_type, "65536", "alter modify text length");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_body new_body TEXT(16777216) NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 2U);
    column_type = parser_test_child_at(column, 1U);
    failures +=
        parser_test_expect_text_type_length(column_type, "16777216", "alter change text length");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE invalid_text_signed_length (body TEXT(+10));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE invalid_text_quoted_length (body TEXT('10'));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE invalid_text_expression_length (body TEXT(1 + 2));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE invalid_tinytext_length (body TINYTEXT(10));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE invalid_mediumtext_length (body MEDIUMTEXT(10));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE invalid_longtext_length (body LONGTEXT(10));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE invalid_long_varchar_length (body LONG VARCHAR(10));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE invalid_long_varbinary_length (body LONG VARBINARY(10));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE invalid_long_text (body LONG TEXT);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
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

    failures += parser_test_parse_sql(
        "CREATE TABLE json_types (id INT, payload JSON, required JSON NOT NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(columns, 3U, "json column list");

    column = parser_test_child_at(columns, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "payload",
        "json column name"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_JSON_TYPE,
        "json column type"
    );
    failures +=
        parser_test_expect_span_text(parser_test_child_at(column, 1U), "JSON", "json type span");

    column = parser_test_child_at(columns, 2U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_JSON_TYPE,
        "json not null type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "json not null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE json (json INT, payload JSON);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "json",
        "nonreserved json table name"
    );
    columns = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "json",
        "nonreserved json column name"
    );
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "nonreserved json column integer type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE json_types ADD COLUMN metadata JSON NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_JSON_TYPE,
        "alter add json type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
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

    failures += parser_test_parse_sql(
        "CREATE TABLE enum_types (status ENUM('draft','published') NOT NULL DEFAULT 'draft', "
        "enum ENUM('', 'A''B'));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(columns, 2U, "enum column list");
    column = parser_test_child_at(columns, 0U);
    column_type = parser_test_child_at(column, 1U);
    labels = parser_test_child_at(column_type, 0U);
    failures +=
        parser_test_expect_node(column_type, MYLITE_SQL_AST_ENUM_TYPE, "enum status column type");
    failures +=
        parser_test_expect_span_text(column_type, "ENUM('draft','published')", "enum status span");
    failures += parser_test_expect_child_count(labels, 2U, "enum status label count");
    failures += parser_test_expect_literal(
        parser_test_child_at(labels, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "enum draft label"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(labels, 0U),
        "'draft'",
        "enum draft label span"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(labels, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "enum published label"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "enum not null"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_STRING,
        "enum string default"
    );
    column = parser_test_child_at(columns, 1U);
    column_type = parser_test_child_at(column, 1U);
    labels = parser_test_child_at(column_type, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "enum",
        "enum keyword identifier"
    );
    failures +=
        parser_test_expect_node(column_type, MYLITE_SQL_AST_ENUM_TYPE, "enum keyword column type");
    failures += parser_test_expect_child_count(labels, 2U, "enum escaped label count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(labels, 0U),
        "''",
        "enum empty label span"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(labels, 1U),
        "'A''B'",
        "enum escaped label span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE enum_types ADD COLUMN next_status ENUM('queued','done') NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_node(
        column_type,
        MYLITE_SQL_AST_ENUM_TYPE,
        "alter add enum column type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "alter add enum nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE bad_enum (v ENUM());",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
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

    failures += parser_test_parse_sql(
        "CREATE TABLE set_types (flags SET('active','featured') NOT NULL DEFAULT '', "
        "set SET('', 'A''B'));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(columns, 2U, "set column list");
    column = parser_test_child_at(columns, 0U);
    column_type = parser_test_child_at(column, 1U);
    members = parser_test_child_at(column_type, 0U);
    failures +=
        parser_test_expect_node(column_type, MYLITE_SQL_AST_SET_TYPE, "set flags column type");
    failures +=
        parser_test_expect_span_text(column_type, "SET('active','featured')", "set flags span");
    failures += parser_test_expect_child_count(members, 2U, "set flags member count");
    failures += parser_test_expect_literal(
        parser_test_child_at(members, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "set active member"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(members, 0U),
        "'active'",
        "set active member span"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(members, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "set featured member"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "set not null"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_STRING,
        "set string default"
    );
    column = parser_test_child_at(columns, 1U);
    column_type = parser_test_child_at(column, 1U);
    members = parser_test_child_at(column_type, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "set",
        "set keyword identifier"
    );
    failures +=
        parser_test_expect_node(column_type, MYLITE_SQL_AST_SET_TYPE, "set keyword column type");
    failures += parser_test_expect_child_count(members, 2U, "set escaped member count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(members, 0U),
        "''",
        "set empty member span"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(members, 1U),
        "'A''B'",
        "set escaped member span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE set_types ADD COLUMN next_flags SET('queued','done') NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures +=
        parser_test_expect_node(column_type, MYLITE_SQL_AST_SET_TYPE, "alter add set column type");
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "alter add set nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE bad_set (v SET());",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
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

    failures += parser_test_parse_sql(
        "CREATE TABLE bit (bit INT, b BIT, b6 BIT(6) NOT NULL DEFAULT b'101');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "bit",
        "nonreserved bit table name"
    );
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(columns, 3U, "bit column list");
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "bit",
        "nonreserved bit column name"
    );
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "nonreserved bit column integer type"
    );
    column = parser_test_child_at(columns, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_bit_type(column_type, NULL, 0, "bare bit column type");
    failures += parser_test_expect_span_text(column_type, "BIT", "bare bit span");
    column = parser_test_child_at(columns, 2U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_bit_type(column_type, "6", 1, "bit width column type");
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "bit width not null"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_BIT,
        "bit literal default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle ADD COLUMN flags BIT(9) DEFAULT b'101';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_bit_type(column_type, "9", 1, "alter add bit column type");
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_BIT,
        "alter add bit literal default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col BIT(8) NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_bit_type(
        parser_test_child_at(column, 1U),
        "8",
        1,
        "bit alter modify column type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "bit alter modify nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_bits BIT NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_bit_type(
        parser_test_child_at(column, 1U),
        NULL,
        0,
        "bit alter change column type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "bit alter change nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE bad_bit (b BIT());",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE bad_bit (b BIT(-1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE bad_bit (b BIT(1.0));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
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

    failures += parser_test_parse_sql(
        "CREATE TABLE year_types (y YEAR, y4 YEAR(4) NOT NULL DEFAULT '70', year INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(columns, year_column_count, "year column list");
    column = parser_test_child_at(columns, 0U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_year_type(column_type, NULL, 0, "bare year column type");
    failures += parser_test_expect_span_text(column_type, "YEAR", "bare year span");
    column = parser_test_child_at(columns, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_year_type(column_type, "4", 1, "year width column type");
    failures += parser_test_expect_span_text(column_type, "YEAR(4)", "year width span");
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "year width not null"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_STRING,
        "year string default"
    );
    column = parser_test_child_at(columns, 2U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "year",
        "year keyword identifier"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE,
        "year identifier type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE year_types ADD COLUMN created YEAR DEFAULT 70;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    failures +=
        parser_test_expect_year_type(parser_test_child_at(column, 1U), NULL, 0, "alter add year");
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "alter year default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM year_types WHERE y IN ('70', 2000, NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    where_clause = parser_test_child_at(statement, 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    failures +=
        parser_test_expect_node(predicate, MYLITE_SQL_AST_IN_PREDICATE, "year in predicate");
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(predicate, 1U), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "year in string"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(predicate, 1U), 1U),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "year in integer"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(predicate, 1U), 2U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "year in null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("UPDATE year_types SET y = '69';", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 1U), 0U);
    failures += parser_test_expect_literal(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "year update string value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE bad_year (y YEAR());",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE bad_year (y YEAR(-1));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE bad_year (y YEAR(1.0));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
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

    failures += parser_test_parse_sql(
        "CREATE TABLE decimal_types (a DECIMAL, b DECIMAL(5), c DECIMAL(5,2) UNSIGNED, "
        "d NUMERIC(4,1), e DEC(6,0), f FIXED(7,3));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures +=
        parser_test_expect_child_count(columns, decimal_column_count, "decimal column list");

    column = parser_test_child_at(columns, 0U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_decimal_type(
        column_type,
        MYLITE_SQL_AST_DECIMAL_TYPE_DECIMAL,
        NULL,
        NULL,
        0,
        "bare decimal column type"
    );
    failures += parser_test_expect_span_text(column_type, "DECIMAL", "bare decimal span");

    column = parser_test_child_at(columns, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_decimal_type(
        column_type,
        MYLITE_SQL_AST_DECIMAL_TYPE_DECIMAL,
        "5",
        NULL,
        0,
        "precision decimal column type"
    );

    column = parser_test_child_at(columns, 2U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_decimal_type(
        column_type,
        MYLITE_SQL_AST_DECIMAL_TYPE_DECIMAL,
        "5",
        "2",
        1,
        "unsigned decimal column type"
    );
    failures +=
        parser_test_expect_span_text(column_type, "DECIMAL(5,2) UNSIGNED", "unsigned decimal span");

    column = parser_test_child_at(columns, 3U);
    failures += parser_test_expect_decimal_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_DECIMAL_TYPE_NUMERIC,
        "4",
        "1",
        0,
        "numeric column type"
    );

    column = parser_test_child_at(columns, 4U);
    failures += parser_test_expect_decimal_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_DECIMAL_TYPE_DEC,
        "6",
        "0",
        0,
        "dec column type"
    );

    column = parser_test_child_at(columns, decimal_fixed_column_index);
    failures += parser_test_expect_decimal_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_DECIMAL_TYPE_FIXED,
        "7",
        "3",
        0,
        "fixed column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE decimal_defaults (a DECIMAL(5,2) DEFAULT 1.23, "
        "b NUMERIC(5,2) DEFAULT -0.50, c DECIMAL(5,2) DEFAULT +1.20);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_DECIMAL,
        "decimal default literal"
    );
    column = parser_test_child_at(columns, 1U);
    failures += parser_test_expect_operator(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "negative decimal default"
    );
    column = parser_test_child_at(columns, 2U);
    failures += parser_test_expect_operator(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "positive decimal default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle ADD COLUMN amount DECIMAL(8,2) NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_decimal_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_DECIMAL_TYPE_DECIMAL,
        "8",
        "2",
        0,
        "alter add decimal column type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "alter add decimal not null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY amount NUMERIC(9,3) NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_decimal_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_DECIMAL_TYPE_NUMERIC,
        "9",
        "3",
        0,
        "alter modify numeric column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE amount total DEC(10,4) NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_decimal_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_DECIMAL_TYPE_DEC,
        "10",
        "4",
        0,
        "alter change dec column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO decimal_types VALUES (1.20, -2.30, +3.40, NULL, TRUE, FALSE);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_LITERAL_DECIMAL,
        "decimal insert value"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "negative decimal insert value"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 2U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "positive decimal insert value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("UPDATE decimal_types SET a = 9.99;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 0U), 1U),
        MYLITE_SQL_AST_LITERAL_DECIMAL,
        "decimal update value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
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
        approximate_scaled_float_column = 9,
        approximate_scaled_double_column = 10,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "CREATE TABLE approximate_types (a FLOAT, b FLOAT(24), c FLOAT(25), "
        "d FLOAT4, e FLOAT8, f DOUBLE, g DOUBLE PRECISION, h REAL, i FLOAT UNSIGNED, "
        "j FLOAT(10,2), k DOUBLE(10,2));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(
        columns,
        approximate_column_count,
        "approximate column list"
    );
    failures += parser_test_expect_approximate_type(
        parser_test_child_at(parser_test_child_at(columns, 0U), 1U),
        MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT,
        NULL,
        NULL,
        0,
        "bare float column type"
    );
    failures += parser_test_expect_approximate_type(
        parser_test_child_at(parser_test_child_at(columns, 1U), 1U),
        MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT,
        "24",
        NULL,
        0,
        "precision float column type"
    );
    failures += parser_test_expect_approximate_type(
        parser_test_child_at(parser_test_child_at(columns, 2U), 1U),
        MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT,
        "25",
        NULL,
        0,
        "double precision float column type"
    );
    failures += parser_test_expect_approximate_type(
        parser_test_child_at(parser_test_child_at(columns, 3U), 1U),
        MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT4,
        NULL,
        NULL,
        0,
        "float4 column type"
    );
    failures += parser_test_expect_approximate_type(
        parser_test_child_at(parser_test_child_at(columns, 4U), 1U),
        MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT8,
        NULL,
        NULL,
        0,
        "float8 column type"
    );
    failures += parser_test_expect_approximate_type(
        parser_test_child_at(parser_test_child_at(columns, approximate_double_column), 1U),
        MYLITE_SQL_AST_APPROXIMATE_TYPE_DOUBLE,
        NULL,
        NULL,
        0,
        "double column type"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(
            parser_test_child_at(columns, approximate_double_precision_column),
            1U
        ),
        "DOUBLE PRECISION",
        "double precision span"
    );
    failures += parser_test_expect_approximate_type(
        parser_test_child_at(parser_test_child_at(columns, approximate_real_column), 1U),
        MYLITE_SQL_AST_APPROXIMATE_TYPE_REAL,
        NULL,
        NULL,
        0,
        "real column type"
    );
    failures += parser_test_expect_approximate_type(
        parser_test_child_at(parser_test_child_at(columns, approximate_unsigned_float_column), 1U),
        MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT,
        NULL,
        NULL,
        1,
        "unsigned float column type"
    );
    failures += parser_test_expect_approximate_type(
        parser_test_child_at(parser_test_child_at(columns, approximate_scaled_float_column), 1U),
        MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT,
        "10",
        "2",
        0,
        "scaled float column type"
    );
    failures += parser_test_expect_approximate_type(
        parser_test_child_at(parser_test_child_at(columns, approximate_scaled_double_column), 1U),
        MYLITE_SQL_AST_APPROXIMATE_TYPE_DOUBLE,
        "10",
        "2",
        0,
        "scaled double column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE approximate_defaults (a FLOAT DEFAULT 1.25, "
        "b DOUBLE DEFAULT -2.5e1, c FLOAT DEFAULT +3.5E-1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_DECIMAL,
        "approximate decimal default literal"
    );
    column = parser_test_child_at(columns, 1U);
    failures += parser_test_expect_operator(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "negative approximate default"
    );
    column = parser_test_child_at(columns, 2U);
    failures += parser_test_expect_operator(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "positive approximate default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO approximate_types VALUES (1.25e1, -2.5E0, +3.5e-1, NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_LITERAL_FLOAT,
        "approximate insert float literal"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "negative approximate insert value"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 2U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "positive approximate insert value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE approximate_types SET a = 9.75e0;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 0U), 1U),
        MYLITE_SQL_AST_LITERAL_FLOAT,
        "approximate update value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE bad_approximate (c FLOAT SIGNED);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "CREATE TABLE bad_approximate_scale (c REAL(7,4));",
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

    failures += parser_test_parse_sql(
        "CREATE TABLE date_types (d DATE, nn DATE NOT NULL DEFAULT '2024-02-29', date INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(columns, date_column_count, "date column list");
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_DATE_TYPE,
        "date column type"
    );
    failures +=
        parser_test_expect_span_text(parser_test_child_at(column, 1U), "DATE", "date column span");
    column = parser_test_child_at(columns, 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_DATE_TYPE,
        "not null date type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "not null date"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_STRING,
        "date string default"
    );
    column = parser_test_child_at(columns, 2U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "date",
        "date keyword identifier"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE,
        "date identifier type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE date_types ADD COLUMN created DATE DEFAULT '1000-01-01';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_DATE_TYPE,
        "alter add date"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_STRING,
        "alter date default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM date_types WHERE d BETWEEN '2024-01-01' AND '2024-12-31';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    where_clause = parser_test_child_at(statement, 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    failures +=
        parser_test_expect_node(predicate, MYLITE_SQL_AST_BETWEEN_PREDICATE, "date between");
    failures += parser_test_expect_literal(
        parser_test_child_at(predicate, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "date between lower"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(predicate, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "date between upper"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM date_types WHERE d IN ('2024-02-29', NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    predicate = parser_test_child_at(parser_test_child_at(statement, 2U), 0U);
    failures +=
        parser_test_expect_node(predicate, MYLITE_SQL_AST_IN_PREDICATE, "date in predicate");
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(predicate, 1U), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "date in string"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(predicate, 1U), 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "date in null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE date_types SET d = '2025-01-02';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 1U), 0U);
    failures += parser_test_expect_literal(
        parser_test_child_at(assignment, 1U),
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

    failures += parser_test_parse_sql(
        "CREATE TABLE datetime_types (d DATETIME, nn DATETIME NOT NULL DEFAULT "
        "'2024-05-06 07:08:09', datetime INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures +=
        parser_test_expect_child_count(columns, datetime_column_count, "datetime column list");
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_DATETIME_TYPE,
        "datetime column type"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 1U),
        "DATETIME",
        "datetime column span"
    );
    column = parser_test_child_at(columns, 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_DATETIME_TYPE,
        "not null datetime type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "not null datetime"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_STRING,
        "datetime string default"
    );
    column = parser_test_child_at(columns, 2U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "datetime",
        "datetime keyword identifier"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE,
        "datetime identifier type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE datetime_types ADD COLUMN created DATETIME DEFAULT "
        "'1000-01-01 00:00:00';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_DATETIME_TYPE,
        "alter add datetime"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_STRING,
        "alter datetime default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE datetime_current_defaults ("
        "dt DATETIME DEFAULT (CURRENT_TIMESTAMP), nested DATETIME DEFAULT ((NOW())));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        "datetime parenthesized current timestamp default"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE,
        "datetime current timestamp default value"
    );
    column = parser_test_child_at(columns, 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(
                parser_test_child_at(
                    parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
                    0U
                ),
                0U
            ),
            0U
        ),
        MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE,
        "datetime nested now default value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM datetime_types WHERE d BETWEEN '2024-01-01 00:00:00' "
        "AND '2024-12-31 23:59:59';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    where_clause = parser_test_child_at(statement, 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    failures +=
        parser_test_expect_node(predicate, MYLITE_SQL_AST_BETWEEN_PREDICATE, "datetime between");
    failures += parser_test_expect_literal(
        parser_test_child_at(predicate, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "datetime between lower"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(predicate, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "datetime between upper"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM datetime_types WHERE d IN ('2024-05-06 07:08:09', NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    predicate = parser_test_child_at(parser_test_child_at(statement, 2U), 0U);
    failures +=
        parser_test_expect_node(predicate, MYLITE_SQL_AST_IN_PREDICATE, "datetime in predicate");
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(predicate, 1U), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "datetime in string"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(predicate, 1U), 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "datetime in null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE datetime_types SET d = '2025-01-02 03:04:05';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 1U), 0U);
    failures += parser_test_expect_literal(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "datetime update string value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
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

    failures += parser_test_parse_sql(
        "CREATE TABLE time_types (t TIME, nn TIME NOT NULL DEFAULT '01:02:03', time INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(columns, time_column_count, "time column list");
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_TIME_TYPE,
        "time column type"
    );
    failures +=
        parser_test_expect_span_text(parser_test_child_at(column, 1U), "TIME", "time column span");
    column = parser_test_child_at(columns, 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_TIME_TYPE,
        "not null time type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "not null time"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_STRING,
        "time string default"
    );
    column = parser_test_child_at(columns, 2U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "time",
        "time keyword identifier"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE,
        "time identifier type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE time_types ADD COLUMN elapsed TIME DEFAULT '-00:00:01';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_TIME_TYPE,
        "alter add time"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_STRING,
        "alter time default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM time_types WHERE t BETWEEN '-00:00:01' AND '24:00:00';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    where_clause = parser_test_child_at(statement, 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    failures +=
        parser_test_expect_node(predicate, MYLITE_SQL_AST_BETWEEN_PREDICATE, "time between");
    failures += parser_test_expect_literal(
        parser_test_child_at(predicate, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "time between lower"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(predicate, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "time between upper"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM time_types WHERE t IN ('838:59:59', NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    predicate = parser_test_child_at(parser_test_child_at(statement, 2U), 0U);
    failures +=
        parser_test_expect_node(predicate, MYLITE_SQL_AST_IN_PREDICATE, "time in predicate");
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(predicate, 1U), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "time in string"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(predicate, 1U), 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "time in null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE time_types SET t = '02:03:04';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 1U), 0U);
    failures += parser_test_expect_literal(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "time update string value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
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

    failures += parser_test_parse_sql(
        "CREATE TABLE timestamp_types (ts TIMESTAMP, nn TIMESTAMP NOT NULL DEFAULT "
        "'2024-05-06 07:08:09', timestamp INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures +=
        parser_test_expect_child_count(columns, timestamp_column_count, "timestamp column list");
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_TIMESTAMP_TYPE,
        "timestamp column type"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 1U),
        "TIMESTAMP",
        "timestamp column span"
    );
    column = parser_test_child_at(columns, 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_TIMESTAMP_TYPE,
        "not null timestamp type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "not null timestamp"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_STRING,
        "timestamp string default"
    );
    column = parser_test_child_at(columns, 2U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "timestamp",
        "timestamp keyword identifier"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE,
        "timestamp identifier type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE timestamp_types ADD COLUMN created TIMESTAMP DEFAULT "
        "'1970-01-01 00:00:01';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_TIMESTAMP_TYPE,
        "alter add timestamp"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_STRING,
        "alter timestamp default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE timestamp_types ALTER ts SET DEFAULT (CURRENT_TIMESTAMP);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE,
        "timestamp alter parenthesized current timestamp default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM timestamp_types WHERE ts BETWEEN '1970-01-01 00:00:01' "
        "AND '2038-01-19 03:14:07';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    where_clause = parser_test_child_at(statement, 2U);
    predicate = parser_test_child_at(where_clause, 0U);
    failures +=
        parser_test_expect_node(predicate, MYLITE_SQL_AST_BETWEEN_PREDICATE, "timestamp between");
    failures += parser_test_expect_literal(
        parser_test_child_at(predicate, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "timestamp between lower"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(predicate, 2U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "timestamp between upper"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id FROM timestamp_types WHERE ts IN ('2024-05-06 07:08:09', NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    predicate = parser_test_child_at(parser_test_child_at(statement, 2U), 0U);
    failures +=
        parser_test_expect_node(predicate, MYLITE_SQL_AST_IN_PREDICATE, "timestamp in predicate");
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(predicate, 1U), 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "timestamp in string"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(predicate, 1U), 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "timestamp in null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE timestamp_types SET ts = '2025-01-02 03:04:05';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 1U), 0U);
    failures += parser_test_expect_literal(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "timestamp update string value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE timestamp_fractional (ts TIMESTAMP(3));",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE timestamp_bad_on_update (ts TIMESTAMP ON UPDATE (CURRENT_TIMESTAMP));",
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

    failures += parser_test_parse_sql(
        "SELECT CURDATE(), CURRENT_DATE, CURRENT_DATE(), CURTIME(), CURRENT_TIME, CURRENT_TIME();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, curdate_item_index), 0U),
        MYLITE_SQL_AST_CURRENT_DATE_VALUE,
        "CURDATE select item"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, current_date_item_index), 0U),
        MYLITE_SQL_AST_CURRENT_DATE_VALUE,
        "CURRENT_DATE select item"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, current_date_call_item_index), 0U),
        MYLITE_SQL_AST_CURRENT_DATE_VALUE,
        "CURRENT_DATE() select item"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, curtime_item_index), 0U),
        MYLITE_SQL_AST_CURRENT_TIME_VALUE,
        "CURTIME select item"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, current_time_item_index), 0U),
        MYLITE_SQL_AST_CURRENT_TIME_VALUE,
        "CURRENT_TIME select item"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, current_time_call_item_index), 0U),
        MYLITE_SQL_AST_CURRENT_TIME_VALUE,
        "CURRENT_TIME() select item"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO t VALUES (CURDATE(), CURRENT_TIME);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "INSERT INTO t SET d = CURDATE(), tm = CURRENT_TIME;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "REPLACE INTO t VALUES (CURRENT_DATE, CURTIME());",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "REPLACE INTO t SET d = CURRENT_DATE, tm = CURTIME();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE t SET d = CURRENT_DATE, tm = CURTIME();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 1U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_CURRENT_DATE_VALUE,
        "current date update assignment"
    );
    assignment = parser_test_child_at(parser_test_child_at(statement, 1U), 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_CURRENT_TIME_VALUE,
        "current time update assignment"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT CURDATE(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT CURRENT_DATE(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT CURTIME(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT CURRENT_TIME(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT CURDATE ();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql("SELECT CURTIME ();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql_with_ignore_space(
        "SELECT CURDATE (), CURTIME ();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, curdate_item_index), 0U),
        MYLITE_SQL_AST_CURRENT_DATE_VALUE,
        "ignore_space CURDATE select item"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(
            parser_test_child_at(select_list, ignore_space_curtime_item_index),
            0U
        ),
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

    failures += parser_test_parse_sql(
        "SELECT UTC_DATE, UTC_DATE(), UTC_TIME, UTC_TIME(), UTC_TIMESTAMP, UTC_TIMESTAMP();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, utc_date_item_index), 0U),
        MYLITE_SQL_AST_UTC_DATE_VALUE,
        "UTC_DATE select item"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, utc_date_call_item_index), 0U),
        MYLITE_SQL_AST_UTC_DATE_VALUE,
        "UTC_DATE() select item"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, utc_time_item_index), 0U),
        MYLITE_SQL_AST_UTC_TIME_VALUE,
        "UTC_TIME select item"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, utc_time_call_item_index), 0U),
        MYLITE_SQL_AST_UTC_TIME_VALUE,
        "UTC_TIME() select item"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, utc_timestamp_item_index), 0U),
        MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE,
        "UTC_TIMESTAMP select item"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, utc_timestamp_call_item_index), 0U),
        MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE,
        "UTC_TIMESTAMP() select item"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT UTC_DATE (), UTC_TIME (), UTC_TIMESTAMP ();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, utc_date_item_index), 0U),
        MYLITE_SQL_AST_UTC_DATE_VALUE,
        "UTC_DATE whitespace select item"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 1U), 0U),
        MYLITE_SQL_AST_UTC_TIME_VALUE,
        "UTC_TIME whitespace select item"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, 2U), 0U),
        MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE,
        "UTC_TIMESTAMP whitespace select item"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO t VALUES (UTC_DATE(), UTC_TIME(), UTC_TIMESTAMP());",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "INSERT INTO t SET d = UTC_DATE, tm = UTC_TIME, dt = UTC_TIMESTAMP;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "REPLACE INTO t VALUES (UTC_DATE(), UTC_TIME(), UTC_TIMESTAMP());",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "REPLACE INTO t SET d = UTC_DATE, tm = UTC_TIME, dt = UTC_TIMESTAMP;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "UPDATE t SET d = UTC_DATE, tm = UTC_TIME, dt = UTC_TIMESTAMP;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 1U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_UTC_DATE_VALUE,
        "UTC_DATE update assignment"
    );
    assignment = parser_test_child_at(parser_test_child_at(statement, 1U), 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_UTC_TIME_VALUE,
        "UTC_TIME update assignment"
    );
    assignment = parser_test_child_at(parser_test_child_at(statement, 1U), 2U);
    failures += parser_test_expect_node(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE,
        "UTC_TIMESTAMP update assignment"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT UTC_DATE(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT UTC_TIME(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT UTC_TIMESTAMP(1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_sysdate_function_statements(void) {
    enum {
        sysdate_item_index = 0U,
    };
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("SELECT SYSDATE();", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    select_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, sysdate_item_index), 0U),
        MYLITE_SQL_AST_SYSDATE_FUNCTION,
        "SYSDATE select item"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT SYSDATE(1);", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, sysdate_item_index), 0U),
        MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR,
        "SYSDATE argument-count select item"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT SYSDATE ();", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("SELECT SYSDATE (1);", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql_with_ignore_space("SELECT SYSDATE ();", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, sysdate_item_index), 0U),
        MYLITE_SQL_AST_SYSDATE_FUNCTION,
        "ignore_space SYSDATE select item"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql_with_ignore_space(
        "SELECT SYSDATE (1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, sysdate_item_index), 0U),
        MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR,
        "ignore_space SYSDATE argument-count select item"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SELECT SYSDATE;", MYLITE_SQL_PARSE_OK, &result);
    select_list = parser_test_child_at(parser_test_child_at(result.root, 0U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(select_list, sysdate_item_index), 0U),
        MYLITE_SQL_AST_IDENTIFIER,
        "bare SYSDATE identifier select item"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("INSERT INTO t VALUES (SYSDATE());", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("INSERT INTO t SET dt = SYSDATE();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("REPLACE INTO t VALUES (SYSDATE());", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);
    failures +=
        parser_test_parse_sql("REPLACE INTO t SET dt = SYSDATE();", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("UPDATE t SET dt = SYSDATE();", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    assignment = parser_test_child_at(parser_test_child_at(statement, 1U), 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(assignment, 1U),
        MYLITE_SQL_AST_SYSDATE_FUNCTION,
        "SYSDATE update assignment"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}
