#include "parser_test_support.h"

static int test_schema_lifecycle_statements(void);
static int test_table_lifecycle_statements(void);
static int test_alter_table_multi_action_statements(void);
static int test_create_table_generated_column_statements(void);
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
static int test_create_table_like_statements(void);
static int test_create_table_select_statements(void);
static int test_create_view_lifecycle_statements(void);
static int test_alter_table_default_charset_collation_statements(void);
static int test_alter_table_order_by_statements(void);
static int test_alter_table_algorithm_lock_option_statements(void);
static int test_alter_table_force_statements(void);
static int test_alter_table_disable_enable_keys_statements(void);

int main(void) {
    int failures = 0;

    failures += test_schema_lifecycle_statements();
    failures += test_table_lifecycle_statements();
    failures += test_alter_table_multi_action_statements();
    failures += test_create_table_generated_column_statements();
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
    failures += test_create_table_like_statements();
    failures += test_create_table_select_statements();
    failures += test_create_view_lifecycle_statements();
    failures += test_alter_table_default_charset_collation_statements();
    failures += test_alter_table_order_by_statements();
    failures += test_alter_table_algorithm_lock_option_statements();
    failures += test_alter_table_force_statements();
    failures += test_alter_table_disable_enable_keys_statements();

    return failures == 0 ? 0 : 1;
}

static int test_schema_lifecycle_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("CREATE DATABASE app;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT,
        "create database"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "create database name"
    );
    failures += parser_test_expect_child_count(statement, 1U, "create database child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE DATABASE configured DEFAULT CHARACTER SET utf8mb4 "
        "COLLATE='utf8mb4_unicode_ci';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT,
        "create database options"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "configured",
        "create options name"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_TABLE_OPTION_LIST,
        "create database option list"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(statement, 1U), 0U),
        MYLITE_SQL_AST_TABLE_CHARSET_OPTION,
        "create database charset option"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(statement, 1U), 1U),
        MYLITE_SQL_AST_TABLE_COLLATION_OPTION,
        "create database collation option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("CREATE SCHEMA `select`;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT, "create schema");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "`select`",
        "create schema name"
    );
    failures += parser_test_expect_child_count(statement, 1U, "create schema child count");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE DATABASE IF NOT EXISTS app;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT,
        "create database if"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "create database if name"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_CREATE_SCHEMA_IF_NOT_EXISTS_CLAUSE,
        "create database if marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER DATABASE configured DEFAULT COLLATE utf8mb4_0900_bin;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_SCHEMA_DEFAULT_CHARSET_COLLATION_STATEMENT,
        "alter database options"
    );
    failures += parser_test_expect_child_count(statement, 2U, "alter database option child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "configured",
        "alter database name"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_TABLE_OPTION_LIST,
        "alter database option list"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(statement, 1U), 0U),
        MYLITE_SQL_AST_TABLE_COLLATION_OPTION,
        "alter database collation option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("ALTER SCHEMA DEFAULT CHARSET=binary;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_SCHEMA_DEFAULT_CHARSET_COLLATION_STATEMENT,
        "alter selected schema options"
    );
    failures +=
        parser_test_expect_child_count(statement, 1U, "alter selected schema option child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_TABLE_OPTION_LIST,
        "alter selected schema option list"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DROP DATABASE app;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT, "drop database");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "drop database name"
    );
    failures += parser_test_expect_child_count(statement, 1U, "drop database child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DROP SCHEMA app;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT, "drop schema");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "drop schema name"
    );
    failures += parser_test_expect_child_count(statement, 1U, "drop schema child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("DROP SCHEMA IF EXISTS app;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT, "drop schema if");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "drop schema if name"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_DROP_SCHEMA_IF_EXISTS_CLAUSE,
        "drop schema if marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW DATABASES;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT,
        "show databases"
    );
    failures += parser_test_expect_child_count(statement, 0U, "show databases child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW DATABASES LIKE 'app%';", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT,
        "show databases like"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "databases like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "'app%'",
        "databases like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW DATABASES WHERE `Database` = 'app';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT,
        "show databases where"
    );
    failures += parser_test_expect_child_count(statement, 1U, "show databases where child count");
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "databases where"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW SCHEMAS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT, "show schemas");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW SCHEMAS WHERE `Database` IN ('mysql','sys');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT,
        "show schemas where"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "schemas where"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW CREATE DATABASE app;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_CREATE_DATABASE_STATEMENT,
        "show create database"
    );
    failures += parser_test_expect_child_count(statement, 1U, "show create database child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "show create database name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW CREATE SCHEMA `select`;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_CREATE_DATABASE_STATEMENT,
        "show create schema"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "`select`",
        "show create schema name"
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

    failures += parser_test_parse_sql(
        "CREATE TABLE app.simple_lifecycle (id INT, amount BIGINT NOT NULL, "
        "flags INTEGER UNSIGNED NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_name = parser_test_child_at(statement, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "create table statement"
    );
    failures += parser_test_expect_node(
        table_name,
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "create qualified table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_name, 0U),
        "app",
        "create schema name"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_name, 1U),
        "simple_lifecycle",
        "create table name"
    );
    failures += parser_test_expect_node(
        columns,
        MYLITE_SQL_AST_COLUMN_DEFINITION_LIST,
        "create column list"
    );
    failures += parser_test_expect_child_count(columns, 3U, "create column list");

    column = parser_test_child_at(columns, 0U);
    failures +=
        parser_test_expect_span_text(parser_test_child_at(column, 0U), "id", "first column name");
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "first column type"
    );
    failures += parser_test_expect_integer_display_width(
        parser_test_child_at(column, 1U),
        NULL,
        "first column width"
    );
    failures += parser_test_expect_true(
        parser_test_child_at(column, 2U) == NULL,
        "first column default nullability"
    );

    column = parser_test_child_at(columns, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "amount",
        "second column name"
    );
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        0,
        "second column type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "second column nullability"
    );

    column = parser_test_child_at(columns, 2U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "flags",
        "third column name"
    );
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        1,
        "third column type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "third column nullability"
    );

    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE small_integer_types (ti TINYINT, tiu TINYINT UNSIGNED, "
        "si SMALLINT, siu SMALLINT UNSIGNED, mi MEDIUMINT, miu MEDIUMINT UNSIGNED);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(
        columns,
        small_integer_column_count,
        "small integer column list"
    );
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint column type"
    );
    column = parser_test_child_at(columns, 1U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        1,
        "tinyint unsigned column type"
    );
    column = parser_test_child_at(columns, 2U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
        0,
        "smallint column type"
    );
    column = parser_test_child_at(columns, 3U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
        1,
        "smallint unsigned column type"
    );
    column = parser_test_child_at(columns, 4U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        0,
        "mediumint column type"
    );
    column = parser_test_child_at(columns, mediumint_unsigned_column_index);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        1,
        "mediumint unsigned column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE signed_integer_types (ti TINYINT SIGNED, si SMALLINT SIGNED, "
        "mi MEDIUMINT SIGNED, i INT SIGNED, ii INTEGER SIGNED, b BIGINT SIGNED);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(
        columns,
        signed_integer_column_count,
        "signed integer column list"
    );
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint signed column type"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 1U),
        "TINYINT SIGNED",
        "tinyint signed span"
    );
    column = parser_test_child_at(columns, 1U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
        0,
        "smallint signed column type"
    );
    column = parser_test_child_at(columns, 2U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        0,
        "mediumint signed column type"
    );
    column = parser_test_child_at(columns, 3U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "int signed column type"
    );
    column = parser_test_child_at(columns, signed_integer_column_index);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "integer signed column type"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 1U),
        "INTEGER SIGNED",
        "integer signed span"
    );
    column = parser_test_child_at(columns, signed_bigint_column_index);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        0,
        "bigint signed column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE integer_aliases (i1 INT1, i2 INT2, i3 INT3, i4 INT4, i8 INT8, "
        "i1u INT1 UNSIGNED, i2s INT2 SIGNED, i3u INT3 UNSIGNED, "
        "i4u INT4 UNSIGNED, i8u INT8 UNSIGNED);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(
        columns,
        alias_integer_column_count,
        "alias integer column list"
    );
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "int1 alias column type"
    );
    failures +=
        parser_test_expect_span_text(parser_test_child_at(column, 1U), "INT1", "int1 alias span");
    column = parser_test_child_at(columns, 1U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
        0,
        "int2 alias column type"
    );
    column = parser_test_child_at(columns, 2U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        0,
        "int3 alias column type"
    );
    column = parser_test_child_at(columns, 3U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "int4 alias column type"
    );
    column = parser_test_child_at(columns, 4U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        0,
        "int8 alias column type"
    );
    column = parser_test_child_at(columns, alias_int1_unsigned_column_index);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        1,
        "int1 unsigned alias column type"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 1U),
        "INT1 UNSIGNED",
        "int1 unsigned span"
    );
    column = parser_test_child_at(columns, alias_int2_signed_column_index);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
        0,
        "int2 signed alias column type"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 1U),
        "INT2 SIGNED",
        "int2 signed span"
    );
    column = parser_test_child_at(columns, alias_int3_unsigned_column_index);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        1,
        "int3 unsigned alias column type"
    );
    column = parser_test_child_at(columns, alias_int4_unsigned_column_index);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        1,
        "int4 unsigned alias column type"
    );
    column = parser_test_child_at(columns, alias_int8_unsigned_column_index);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        1,
        "int8 unsigned alias column type"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 1U),
        "INT8 UNSIGNED",
        "int8 unsigned span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE display_widths (ti0 TINYINT(0), ti1 TINYINT(1), "
        "ti2 TINYINT(2), si SMALLINT(5), mi MEDIUMINT(9), i INT(11), "
        "ii INTEGER(10), bi BIGINT(20), iu INT(10) UNSIGNED, "
        "tis TINYINT(1) SIGNED, tiu TINYINT(1) UNSIGNED, i1 INT1(1), "
        "i2 INT2(5), i3 INT3(7), i4 INT4(9), i8 INT8(20));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(
        columns,
        display_width_column_count,
        "display width column list"
    );

    column = parser_test_child_at(columns, 0U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint zero display width type"
    );
    failures +=
        parser_test_expect_integer_display_width(column_type, "0", "tinyint zero display width");
    failures +=
        parser_test_expect_span_text(column_type, "TINYINT(0)", "tinyint zero display width span");

    column = parser_test_child_at(columns, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint one display width type"
    );
    failures +=
        parser_test_expect_integer_display_width(column_type, "1", "tinyint one display width");

    column = parser_test_child_at(columns, display_width_int_unsigned_column_index);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        1,
        "int width unsigned"
    );
    failures +=
        parser_test_expect_integer_display_width(column_type, "10", "int unsigned display width");
    failures +=
        parser_test_expect_span_text(column_type, "INT(10) UNSIGNED", "int unsigned width span");

    column = parser_test_child_at(columns, display_width_tinyint_signed_column_index);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint width signed"
    );
    failures +=
        parser_test_expect_integer_display_width(column_type, "1", "tinyint signed display width");
    failures +=
        parser_test_expect_span_text(column_type, "TINYINT(1) SIGNED", "tinyint signed width span");

    column = parser_test_child_at(columns, display_width_tinyint_unsigned_column_index);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        1,
        "tinyint width unsigned"
    );
    failures += parser_test_expect_integer_display_width(
        column_type,
        "1",
        "tinyint unsigned display width"
    );
    failures += parser_test_expect_span_text(
        column_type,
        "TINYINT(1) UNSIGNED",
        "tinyint unsigned width span"
    );

    column = parser_test_child_at(columns, display_width_int1_column_index);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "int1 width"
    );
    failures += parser_test_expect_integer_display_width(column_type, "1", "int1 display width");

    column = parser_test_child_at(columns, display_width_int8_column_index);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        0,
        "int8 width"
    );
    failures += parser_test_expect_integer_display_width(column_type, "20", "int8 display width");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE bool_aliases (b BOOL, c BOOLEAN, nn BOOL NOT NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures +=
        parser_test_expect_child_count(columns, bool_alias_column_count, "bool alias column list");
    column = parser_test_child_at(columns, 0U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "bool alias column type"
    );
    failures +=
        parser_test_expect_integer_display_width(column_type, NULL, "bool alias display width");
    failures += parser_test_expect_integer_bool_alias(column_type, "bool alias marker");
    failures += parser_test_expect_span_text(column_type, "BOOL", "bool alias span");
    column = parser_test_child_at(columns, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "boolean alias column type"
    );
    failures += parser_test_expect_integer_bool_alias(column_type, "boolean alias marker");
    failures += parser_test_expect_span_text(column_type, "BOOLEAN", "boolean alias span");
    column = parser_test_child_at(columns, 2U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_integer_bool_alias(column_type, "bool not null alias marker");
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "bool alias not null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE explicit_default_nulls (a INT DEFAULT NULL, "
        "b BIGINT NULL DEFAULT NULL, c BOOL DEFAULT NULL, nn INT NOT NULL DEFAULT NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(columns, 4U, "explicit default null column list");
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_true(
        parser_test_first_child_kind(column, MYLITE_SQL_AST_NULLABILITY) == NULL,
        "default null omitted nullability"
    );
    failures += parser_test_expect_node(
        parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "default null marker"
    );
    failures += parser_test_expect_span_text(
        parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        "DEFAULT NULL",
        "default null span"
    );
    column = parser_test_child_at(columns, 1U);
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "default null explicit nullability"
    );
    failures += parser_test_expect_node(
        parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "explicit null default marker"
    );
    column = parser_test_child_at(columns, 2U);
    failures += parser_test_expect_integer_bool_alias(
        parser_test_child_at(column, 1U),
        "default null bool marker"
    );
    failures += parser_test_expect_node(
        parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "bool default null marker"
    );
    column = parser_test_child_at(columns, 3U);
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "not null default null parser marker"
    );
    failures += parser_test_expect_node(
        parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "not null default null marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE integer_defaults (a INT DEFAULT 5, b INT DEFAULT +9, "
        "c INT DEFAULT -7, d BOOL DEFAULT TRUE, e BOOLEAN DEFAULT FALSE);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(
        columns,
        integer_default_column_count,
        "integer default column list"
    );
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_node(
        parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
        MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE,
        "integer default marker"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_INTEGER,
        "integer default literal"
    );
    failures += parser_test_expect_span_text(
        parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
        "DEFAULT 5",
        "integer default span"
    );
    column = parser_test_child_at(columns, 1U);
    failures += parser_test_expect_operator(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "positive default operator"
    );
    column = parser_test_child_at(columns, 2U);
    failures += parser_test_expect_operator(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "negative default operator"
    );
    column = parser_test_child_at(columns, 3U);
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "true default literal"
    );
    column = parser_test_child_at(columns, 4U);
    failures += parser_test_expect_literal(
        parser_test_child_at(
            parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE),
            0U
        ),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "false default literal"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE bool_identifiers (BOOL INT, BOOLEAN TINYINT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "BOOL",
        "bool identifier column name"
    );
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "bool identifier column type"
    );
    column = parser_test_child_at(columns, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "BOOLEAN",
        "boolean identifier column name"
    );
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "boolean identifier column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE IF NOT EXISTS app.if_missing (id INT) ENGINE=InnoDB;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_name = parser_test_child_at(statement, 0U);
    columns = parser_test_child_at(statement, 1U);
    if_not_exists = parser_test_child_at(statement, 2U);
    table_options = parser_test_child_at(statement, 3U);
    failures += parser_test_expect_child_count(statement, 4U, "create if not exists child count");
    failures += parser_test_expect_node(
        if_not_exists,
        MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE,
        "create if not exists clause"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_name, 1U),
        "if_missing",
        "if not exists table"
    );
    failures += parser_test_expect_child_count(columns, 1U, "if not exists column list");
    failures += parser_test_expect_node(
        table_options,
        MYLITE_SQL_AST_TABLE_OPTION_LIST,
        "if not exists options"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("DROP TABLE IF EXISTS app.if_missing;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    table_names = parser_test_child_at(statement, 0U);
    table_name = parser_test_child_at(table_names, 0U);
    if_exists = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(statement, 2U, "drop if exists child count");
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_DROP_TABLE_STATEMENT,
        "drop table statement"
    );
    failures += parser_test_expect_node(
        table_names,
        MYLITE_SQL_AST_TABLE_NAME_LIST,
        "drop table name list"
    );
    failures += parser_test_expect_child_count(table_names, 1U, "drop if exists table name count");
    failures += parser_test_expect_node(
        table_name,
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "drop qualified table"
    );
    failures += parser_test_expect_node(
        if_exists,
        MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE,
        "drop if exists clause"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_name, 0U),
        "app",
        "drop if exists schema"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_name, 1U),
        "if_missing",
        "drop if exists table"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE engine_forms (id INT) ENGINE=InnoDB;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_options = parser_test_child_at(statement, 2U);
    engine_option = parser_test_child_at(table_options, 0U);
    failures += parser_test_expect_child_count(statement, 3U, "engine create child count");
    failures += parser_test_expect_node(
        table_options,
        MYLITE_SQL_AST_TABLE_OPTION_LIST,
        "create table options"
    );
    failures += parser_test_expect_child_count(table_options, 1U, "engine option list child count");
    failures += parser_test_expect_node(
        engine_option,
        MYLITE_SQL_AST_TABLE_ENGINE_OPTION,
        "create table engine option"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(engine_option, 0U),
        "InnoDB",
        "engine option name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE engine_space (id INT) ENGINE InnoDB;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE engine_string (id INT) ENGINE='InnoDB';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_options = parser_test_child_at(statement, 2U);
    engine_option = parser_test_child_at(table_options, 0U);
    failures += parser_test_expect_literal(
        parser_test_child_at(engine_option, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "string engine"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(engine_option, 0U),
        "'InnoDB'",
        "string engine name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE engine_quoted (id INT) ENGINE=`InnoDB`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE charset_options (id INT) DEFAULT CHARSET=utf8mb4 "
        "COLLATE='utf8mb4_0900_ai_ci';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_options = parser_test_child_at(statement, 2U);
    charset_option = parser_test_child_at(table_options, 0U);
    collation_option = parser_test_child_at(table_options, 1U);
    failures +=
        parser_test_expect_child_count(table_options, 2U, "charset option list child count");
    failures += parser_test_expect_node(
        charset_option,
        MYLITE_SQL_AST_TABLE_CHARSET_OPTION,
        "create table charset option"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(charset_option, 0U),
        "utf8mb4",
        "charset option name"
    );
    failures += parser_test_expect_node(
        collation_option,
        MYLITE_SQL_AST_TABLE_COLLATION_OPTION,
        "create table collation option"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(collation_option, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "string collation"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(collation_option, 0U),
        "'utf8mb4_0900_ai_ci'",
        "collation option name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE character_set_space (id INT) DEFAULT CHARACTER SET utf8mb4;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE charset_space (id INT) CHARSET utf8mb4;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE default_collate (id INT) DEFAULT COLLATE utf8mb4_0900_ai_ci;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE status (status INT);", MYLITE_SQL_PARSE_OK, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE collation (collation INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("DROP TABLE app.simple_lifecycle;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    table_names = parser_test_child_at(statement, 0U);
    table_name = parser_test_child_at(table_names, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_DROP_TABLE_STATEMENT,
        "drop table statement"
    );
    failures += parser_test_expect_node(
        table_names,
        MYLITE_SQL_AST_TABLE_NAME_LIST,
        "single drop table name list"
    );
    failures += parser_test_expect_child_count(table_names, 1U, "single drop table name count");
    failures += parser_test_expect_span_text(table_name, "app.simple_lifecycle", "drop target");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DROP TABLE first_table, app.second_table;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_names = parser_test_child_at(statement, 0U);
    table_name = parser_test_child_at(table_names, 0U);
    failures += parser_test_expect_child_count(statement, 1U, "multi drop child count");
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_DROP_TABLE_STATEMENT,
        "multi drop statement"
    );
    failures += parser_test_expect_node(
        table_names,
        MYLITE_SQL_AST_TABLE_NAME_LIST,
        "multi drop table name list"
    );
    failures += parser_test_expect_child_count(table_names, 2U, "multi drop table name count");
    failures += parser_test_expect_span_text(table_name, "first_table", "first multi drop target");
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_names, 1U),
        "app.second_table",
        "second multi drop target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DROP TABLE IF EXISTS first_table, app.second_table;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_names = parser_test_child_at(statement, 0U);
    if_exists = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(statement, 2U, "multi drop if exists child count");
    failures +=
        parser_test_expect_child_count(table_names, 2U, "multi drop if exists table name count");
    failures += parser_test_expect_node(
        if_exists,
        MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE,
        "multi drop if exists clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("TRUNCATE TABLE app.simple_lifecycle;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT,
        "truncate table statement"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.simple_lifecycle",
        "truncate target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("TRUNCATE simple_lifecycle;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT,
        "bare truncate statement"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "simple_lifecycle",
        "bare truncate target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW TABLES FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLES_STATEMENT,
        "show tables statement"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "show tables schema"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW TABLES IN app;", MYLITE_SQL_PARSE_OK, &result);
    failures += parser_test_expect_node(
        parser_test_child_at(result.root, 0U),
        MYLITE_SQL_AST_SHOW_TABLES_STATEMENT,
        "show tables in statement"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW TABLES;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLES_STATEMENT,
        "bare show tables"
    );
    failures += parser_test_expect_true(
        parser_test_child_at(statement, 0U) == NULL,
        "bare show has no schema child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW TABLES LIKE 'a%';", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLES_STATEMENT,
        "show tables like"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "tables like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "'a%'",
        "show tables like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW TABLES FROM app LIKE 'a\\_%';", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLES_STATEMENT,
        "show tables schema like"
    );
    failures += parser_test_expect_true(
        !mylite_sql_ast_node_show_tables_is_full(statement),
        "show tables schema like is not full"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "show tables like schema"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "schema like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "'a\\_%'",
        "schema like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW TABLES WHERE Tables_in_app LIKE 'a%';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLES_STATEMENT,
        "show tables where"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "tables where"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW TABLES FROM app WHERE Tables_in_app = 'alpha';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLES_STATEMENT,
        "show tables schema where"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "show tables where schema"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "tables schema where"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW TABLE STATUS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT,
        "bare show table status"
    );
    failures += parser_test_expect_true(
        parser_test_child_at(statement, 0U) == NULL,
        "bare table status has no schema child"
    );
    failures += parser_test_expect_true(
        parser_test_child_at(statement, 1U) == NULL,
        "bare table status has no like child"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW TABLE STATUS FROM app;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT,
        "show table status from"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "show table status schema"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW TABLE STATUS IN app LIKE 'a\\_%';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT,
        "show table status in like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "show table status like schema"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "status like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "'a\\_%'",
        "status like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW TABLE STATUS LIKE 'a%';", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT,
        "show table status like"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "status bare like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "'a%'",
        "status bare like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW CHARACTER SET;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT,
        "show character set"
    );
    failures += parser_test_expect_child_count(statement, 0U, "show character set child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW CHARSET LIKE 'utf8%';", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT,
        "show charset like"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "charset like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "'utf8%'",
        "charset like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW CHARACTER SET WHERE Charset = 'utf8mb4';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT,
        "show character set where"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "charset where clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW COLLATION;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT,
        "show collation"
    );
    failures += parser_test_expect_child_count(statement, 0U, "show collation child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW COLLATION LIKE 'utf8mb4\\_0900\\_ai\\_ci';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT,
        "show collation like"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "collation like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "'utf8mb4\\_0900\\_ai\\_ci'",
        "collation like pattern"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW COLLATION WHERE Collation = 'utf8mb4_0900_ai_ci';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT,
        "show collation where"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "collation where clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW ENGINES;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT, "show engines");
    failures += parser_test_expect_child_count(statement, 0U, "show engines child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW STORAGE ENGINES;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT,
        "show storage engines"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "RENAME TABLE app.simple_lifecycle TO archive.renamed_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_RENAME_TABLE_STATEMENT,
        "rename table statement"
    );
    rename_pairs = parser_test_child_at(statement, 0U);
    rename_pair = parser_test_child_at(rename_pairs, 0U);
    failures += parser_test_expect_child_count(statement, 1U, "rename table statement");
    failures += parser_test_expect_node(
        rename_pairs,
        MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST,
        "rename pair list"
    );
    failures += parser_test_expect_child_count(rename_pairs, 1U, "rename pair list child count");
    failures +=
        parser_test_expect_node(rename_pair, MYLITE_SQL_AST_RENAME_TABLE_PAIR, "rename pair");
    failures += parser_test_expect_span_text(
        parser_test_child_at(rename_pair, 0U),
        "app.simple_lifecycle",
        "rename source"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(rename_pair, 1U),
        "archive.renamed_lifecycle",
        "rename target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "RENAME TABLE first_old TO first_new, app.second_old TO archive.second_new;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    rename_pairs = parser_test_child_at(statement, 0U);
    rename_pair = parser_test_child_at(rename_pairs, 0U);
    failures += parser_test_expect_child_count(statement, 1U, "multi rename statement");
    failures += parser_test_expect_child_count(rename_pairs, 2U, "multi rename pair count");
    failures +=
        parser_test_expect_node(rename_pair, MYLITE_SQL_AST_RENAME_TABLE_PAIR, "first rename pair");
    failures += parser_test_expect_span_text(
        parser_test_child_at(rename_pair, 0U),
        "first_old",
        "first rename source"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(rename_pair, 1U),
        "first_new",
        "first rename target"
    );
    rename_pair = parser_test_child_at(rename_pairs, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(rename_pair, 0U),
        "app.second_old",
        "second rename source"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(rename_pair, 1U),
        "archive.second_new",
        "second rename target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.simple_lifecycle RENAME TO archive.renamed_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT,
        "alter table rename to statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "alter table rename statement");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter rename source"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "archive.renamed_lifecycle",
        "alter rename target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle RENAME renamed_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT,
        "alter table bare rename statement"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "renamed_lifecycle",
        "bare rename target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle RENAME AS renamed_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT,
        "alter table rename as statement"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "renamed_lifecycle",
        "rename as target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.simple_lifecycle ADD COLUMN added INT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT,
        "alter table add column statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "alter add column child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter add target"
    );
    column = parser_test_child_at(statement, 1U);
    failures +=
        parser_test_expect_node(column, MYLITE_SQL_AST_COLUMN_DEFINITION, "alter add column");
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "added",
        "alter added column name"
    );
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "alter added column type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "alter added column nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.simple_lifecycle DROP COLUMN old_col;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT,
        "alter table drop column statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "alter drop column child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter drop target"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "old_col",
        "alter dropped column name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle DROP old_col;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT,
        "alter table bare drop column statement"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "simple_lifecycle",
        "bare alter drop target"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "old_col",
        "bare dropped column name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.simple_lifecycle RENAME COLUMN old_col TO new_col;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_RENAME_COLUMN_STATEMENT,
        "alter table rename column statement"
    );
    failures += parser_test_expect_child_count(statement, 3U, "alter rename column child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter rename column target"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "old_col",
        "alter rename old column name"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 2U),
        "new_col",
        "alter rename new column name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.simple_lifecycle MODIFY COLUMN old_col BIGINT NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT,
        "alter table modify column statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "alter modify column child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter modify column target"
    );
    column = parser_test_child_at(statement, 1U);
    failures +=
        parser_test_expect_node(column, MYLITE_SQL_AST_COLUMN_DEFINITION, "alter modify column");
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "old_col",
        "alter modified column name"
    );
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        0,
        "alter modified column type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "alter modified column nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.simple_lifecycle CHANGE COLUMN old_col new_col BIGINT NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT,
        "alter table change column statement"
    );
    failures += parser_test_expect_child_count(statement, 3U, "alter change column child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter change column target"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "old_col",
        "alter change old column"
    );
    column = parser_test_child_at(statement, 2U);
    failures +=
        parser_test_expect_node(column, MYLITE_SQL_AST_COLUMN_DEFINITION, "alter changed column");
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "new_col",
        "alter change new column"
    );
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        0,
        "alter changed column type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "alter changed column nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle ADD added BIGINT UNSIGNED NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT,
        "bare alter table add statement"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "simple_lifecycle",
        "bare alter add target"
    );
    column = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "added",
        "bare alter add column name"
    );
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        1,
        "bare alter add column type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "bare alter add column nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle ADD added_small SMALLINT UNSIGNED NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
        1,
        "smallint unsigned alter add column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle ADD added_signed TINYINT SIGNED NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint signed alter add column type"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 1U),
        "TINYINT SIGNED",
        "signed alter add span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle ADD added_alias INT1 UNSIGNED NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        1,
        "int1 unsigned alter add column type"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 1U),
        "INT1 UNSIGNED",
        "alias alter add span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle ADD added_width TINYINT(1) NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "display width alter add column type"
    );
    failures +=
        parser_test_expect_integer_display_width(column_type, "1", "display width alter add");
    failures +=
        parser_test_expect_span_text(column_type, "TINYINT(1)", "display width alter add span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle ADD added_bool BOOL NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "bool alias alter add column type"
    );
    failures += parser_test_expect_integer_bool_alias(column_type, "bool alias alter add marker");
    failures += parser_test_expect_span_text(column_type, "BOOL", "bool alias alter add span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle ADD added_default INT NULL DEFAULT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "alter add default null marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col INT UNSIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT,
        "bare alter table modify statement"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "simple_lifecycle",
        "bare alter modify target"
    );
    column = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "old_col",
        "bare alter modify column name"
    );
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        1,
        "bare alter modify column type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "bare alter modify column nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col TINYINT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "tinyint alter modify column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col INT SIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "int signed alter modify column type"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 1U),
        "INT SIGNED",
        "signed alter modify span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col INT4 SIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "int4 signed alter modify column type"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 1U),
        "INT4 SIGNED",
        "alias alter modify span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col INT(11) UNSIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        1,
        "display width alter modify column type"
    );
    failures +=
        parser_test_expect_integer_display_width(column_type, "11", "display width alter modify");
    failures += parser_test_expect_span_text(
        column_type,
        "INT(11) UNSIGNED",
        "display width alter modify span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col BOOLEAN NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "boolean alias alter modify column type"
    );
    failures +=
        parser_test_expect_integer_bool_alias(column_type, "boolean alias alter modify marker");
    failures +=
        parser_test_expect_span_text(column_type, "BOOLEAN", "boolean alias alter modify span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle MODIFY old_col BIGINT DEFAULT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "alter modify default null marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_col INT UNSIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT,
        "bare alter table change statement"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "simple_lifecycle",
        "bare alter change target"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "old_col",
        "bare alter change old name"
    );
    column = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "new_col",
        "bare alter change new name"
    );
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        1,
        "bare alter change column type"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "bare alter change column nullability"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_col MEDIUMINT UNSIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        1,
        "mediumint unsigned alter change column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_col MEDIUMINT SIGNED NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        0,
        "mediumint signed alter change column type"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 1U),
        "MEDIUMINT SIGNED",
        "signed alter change span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_col INT3 NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
        0,
        "int3 alias alter change column type"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 1U),
        "INT3",
        "alias alter change span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_width INT1(1) NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 2U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "display width alter change column type"
    );
    failures +=
        parser_test_expect_integer_display_width(column_type, "1", "display width alter change");
    failures +=
        parser_test_expect_span_text(column_type, "INT1(1)", "display width alter change span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_bool BOOL NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 2U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        0,
        "bool alias alter change column type"
    );
    failures +=
        parser_test_expect_integer_bool_alias(column_type, "bool alias alter change marker");
    failures += parser_test_expect_span_text(column_type, "BOOL", "bool alias alter change span");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle CHANGE old_col new_default BIGINT DEFAULT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    column = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_node(
        parser_test_first_child_kind(column, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "alter change default null marker"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.simple_lifecycle ALTER COLUMN old_col SET DEFAULT +8;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT,
        "alter table set default statement"
    );
    failures += parser_test_expect_child_count(statement, 3U, "alter set default child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter set default target"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "old_col",
        "alter set default column"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE,
        "alter set default value node"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(statement, 2U), 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "alter set default positive value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle ALTER old_col SET DEFAULT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT,
        "alter table bare set default statement"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_COLUMN_DEFAULT_NULL,
        "alter set default null node"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.simple_lifecycle ALTER COLUMN old_col DROP DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT,
        "alter table drop default statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "alter drop default child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter drop default target"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "old_col",
        "alter drop default column"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle ALTER old_col DROP DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT,
        "alter table bare drop default statement"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.simple_lifecycle ALTER COLUMN old_col SET INVISIBLE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT,
        "alter table column invisible statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "alter column invisible child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter column invisible target"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "old_col",
        "alter column invisible column"
    );
    failures += parser_test_expect_column_visibility(
        statement,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_INVISIBLE,
        "alter column invisible payload"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle ALTER old_col SET VISIBLE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT,
        "alter table column visible statement"
    );
    failures += parser_test_expect_column_visibility(
        statement,
        MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE,
        "alter column visible payload"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE visibility_names (visible INT, invisible INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO app.simple_lifecycle (amount, id) VALUES (+1, -2), (NULL, 3);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "insert statement");
    failures += parser_test_expect_child_count(statement, 3U, "insert statement");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.simple_lifecycle",
        "insert target"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_IDENTIFIER_LIST,
        "insert columns"
    );
    failures +=
        parser_test_expect_child_count(parser_test_child_at(statement, 1U), 2U, "insert columns");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(statement, 1U), 0U),
        "amount",
        "insert col 1"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(statement, 1U), 1U),
        "id",
        "insert col 2"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_INSERT_ROW_LIST,
        "insert rows"
    );
    failures +=
        parser_test_expect_child_count(parser_test_child_at(statement, 2U), 2U, "insert rows");
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(statement, 2U), 0U),
        MYLITE_SQL_AST_INSERT_ROW,
        "first insert row"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "positive insert value"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "negative insert value"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 1U), 0U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "null insert value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO simple_lifecycle VALUES (1);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_STATEMENT,
        "insert without columns"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_IDENTIFIER_LIST,
        "empty insert columns"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(statement, 1U),
        0U,
        "insert has no column list"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO simple_lifecycle VALUES (TRUE, false);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "true insert value"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 1U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "false insert value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO app.simple_lifecycle SET id = +1, amount = NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SET_STATEMENT,
        "insert set statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "insert set statement");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.simple_lifecycle",
        "insert set target"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_INSERT_ASSIGNMENT_LIST,
        "insert set assignments"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(statement, 1U),
        2U,
        "insert set assignment count"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(statement, 1U), 0U),
        MYLITE_SQL_AST_INSERT_ASSIGNMENT,
        "insert set first assignment"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 0U), 0U),
        "id",
        "insert set first target"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 0U), 1U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "insert set first value"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 1U), 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "insert set second value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO app.simple_lifecycle SET id = TRUE, amount = false;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 0U), 1U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "insert set true value"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 1U), 1U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "insert set false value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("INSERT simple_lifecycle SET id = -1;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_SET_STATEMENT,
        "insert set no into"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "simple_lifecycle",
        "no into target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO simple_lifecycle SET simple_lifecycle.id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 0U), 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "insert set qualified target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE INTO app.simple_lifecycle (amount, id) VALUES (+1, -2), (NULL, TRUE);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT,
        "replace statement"
    );
    failures += parser_test_expect_child_count(statement, 3U, "replace statement");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.simple_lifecycle",
        "replace target"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_IDENTIFIER_LIST,
        "replace columns"
    );
    failures +=
        parser_test_expect_child_count(parser_test_child_at(statement, 1U), 2U, "replace columns");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(statement, 1U), 0U),
        "amount",
        "replace col 1"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(statement, 1U), 1U),
        "id",
        "replace col 2"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 2U),
        MYLITE_SQL_AST_INSERT_ROW_LIST,
        "replace rows"
    );
    failures +=
        parser_test_expect_child_count(parser_test_child_at(statement, 2U), 2U, "replace rows");
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "positive replace value"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 1U),
        MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        "negative replace value"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 1U), 0U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "null replace value"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 1U), 1U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "true replace value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE simple_lifecycle VALUES (FALSE);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT,
        "replace without into"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "simple_lifecycle",
        "no into replace"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(statement, 1U),
        0U,
        "replace has no column list"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 2U), 0U), 0U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "false replace value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE INTO app.simple_lifecycle SET id = +1, amount = NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_REPLACE_SET_STATEMENT,
        "replace set statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "replace set statement");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.simple_lifecycle",
        "replace set target"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_INSERT_ASSIGNMENT_LIST,
        "replace set assignments"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(statement, 1U),
        2U,
        "replace set assignment count"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(statement, 1U), 0U),
        MYLITE_SQL_AST_INSERT_ASSIGNMENT,
        "replace set first assignment"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 0U), 0U),
        "id",
        "replace set first target"
    );
    failures += parser_test_expect_operator(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 0U), 1U),
        MYLITE_SQL_AST_OPERATOR_POSITIVE,
        "replace set first value"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 1U), 1U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "replace set second value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE simple_lifecycle SET id = TRUE, amount = false;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_REPLACE_SET_STATEMENT,
        "replace set no into"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "simple_lifecycle",
        "replace set target"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 0U), 1U),
        MYLITE_SQL_AST_LITERAL_TRUE,
        "replace set true value"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 1U), 1U),
        MYLITE_SQL_AST_LITERAL_FALSE,
        "replace set false value"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE INTO simple_lifecycle SET simple_lifecycle.id = 1;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 0U), 0U),
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "replace set qualified target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT * FROM app.simple_lifecycle;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "table wildcard select"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "table select from"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(statement, 1U), 0U),
        "app.simple_lifecycle",
        "table select target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SELECT id, amount FROM simple_lifecycle;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "table projection select"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_FROM_TABLE,
        "projection from"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 0U), 0U), 0U),
        "id",
        "first projection"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 0U), 1U), 0U),
        "amount",
        "second projection"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_multi_action_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *actions = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle ADD COLUMN first_added INT, ADD COLUMN second_added INT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    actions = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT,
        "alter table multi add column statement"
    );
    failures +=
        parser_test_expect_child_count(statement, 2U, "alter multi action statement child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "simple_lifecycle",
        "alter multi table"
    );
    failures += parser_test_expect_node(
        actions,
        MYLITE_SQL_AST_ALTER_TABLE_ACTION_LIST,
        "alter multi list"
    );
    failures += parser_test_expect_child_count(actions, 2U, "alter multi add column action count");
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 0U),
        MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT,
        "alter multi first add column action"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 1U),
        MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT,
        "alter multi second add column action"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_idx ADD INDEX k_v (v), ADD INDEX k_id (id);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    actions = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT,
        "alter multi add index statement"
    );
    failures += parser_test_expect_child_count(actions, 2U, "alter multi add index count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE drop_idx DROP INDEX k_v, ADD INDEX k_id (id);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    actions = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT,
        "alter multi drop add index statement"
    );
    failures += parser_test_expect_child_count(actions, 2U, "alter multi drop add count");
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 0U),
        MYLITE_SQL_AST_ALTER_TABLE_DROP_INDEX_STATEMENT,
        "alter multi first drop index action"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 1U),
        MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT,
        "alter multi second add index action"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_pk ADD PRIMARY KEY (id), ADD KEY k_v (v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    actions = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT,
        "alter multi add primary key statement"
    );
    failures += parser_test_expect_child_count(actions, 2U, "alter multi add primary key count");
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 0U),
        MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT,
        "alter multi first add primary key action"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 1U),
        MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT,
        "alter multi second add index after primary key action"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE drop_pk DROP PRIMARY KEY, ADD PRIMARY KEY (v);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    actions = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT,
        "alter multi drop add primary key statement"
    );
    failures +=
        parser_test_expect_child_count(actions, 2U, "alter multi drop add primary key count");
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 0U),
        MYLITE_SQL_AST_ALTER_TABLE_DROP_PRIMARY_KEY_STATEMENT,
        "alter multi first drop primary key action"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 1U),
        MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT,
        "alter multi second add primary key action"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE add_pk ADD COLUMN id INT NOT NULL, ADD PRIMARY KEY (id);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    actions = parser_test_child_at(parser_test_child_at(result.root, 0U), 1U);
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 1U),
        MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT,
        "alter multi add primary key after add column action"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE defaults ALTER a SET DEFAULT 9, ALTER b DROP DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    actions = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT,
        "alter multi default statement"
    );
    failures += parser_test_expect_child_count(actions, 2U, "alter multi default action count");
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 0U),
        MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT,
        "alter multi first set default action"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 1U),
        MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT,
        "alter multi second drop default action"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE defaults ADD COLUMN c INT, ALTER a SET DEFAULT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    actions = parser_test_child_at(statement, 1U);
    failures +=
        parser_test_expect_child_count(actions, 2U, "alter multi add column set default count");
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 1U),
        MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT,
        "alter multi set default after add column action"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE columns MODIFY a BIGINT, MODIFY COLUMN b BIGINT NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    actions = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT,
        "alter multi modify statement"
    );
    failures += parser_test_expect_child_count(actions, 2U, "alter multi modify action count");
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 0U),
        MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT,
        "alter multi first modify action"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 1U),
        MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT,
        "alter multi second modify action"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE columns CHANGE a c BIGINT, CHANGE COLUMN b d BIGINT NOT NULL;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    actions = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT,
        "alter multi change statement"
    );
    failures += parser_test_expect_child_count(actions, 2U, "alter multi change action count");
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 0U),
        MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT,
        "alter multi first change action"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(actions, 1U),
        MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT,
        "alter multi second change action"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_create_table_generated_column_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *column = NULL;
    const struct mylite_sql_ast_node *generated = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "CREATE TABLE generated_lifecycle ("
        "a INT, "
        "b INT AS (a + 1), "
        "c BIGINT GENERATED ALWAYS AS ((a * 2)) STORED NOT NULL, "
        "d INT GENERATED ALWAYS AS (NULL) VIRTUAL COMMENT 'ok');",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "generated create table"
    );
    failures += parser_test_expect_child_count(columns, 4U, "generated column count");

    column = parser_test_child_at(columns, 1U);
    generated = parser_test_child_at(column, 2U);
    failures += parser_test_expect_node(
        generated,
        MYLITE_SQL_AST_GENERATED_COLUMN_CLAUSE,
        "default virtual generated clause"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(generated, 0U),
        "a + 1",
        "default generated expr"
    );

    column = parser_test_child_at(columns, 2U);
    generated = parser_test_child_at(column, 2U);
    failures += parser_test_expect_node(
        parser_test_child_at(generated, 1U),
        MYLITE_SQL_AST_GENERATED_COLUMN_STORED,
        "stored generated storage"
    );
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 3U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "generated not-null attribute"
    );

    column = parser_test_child_at(columns, 3U);
    generated = parser_test_child_at(column, 2U);
    failures += parser_test_expect_node(
        parser_test_child_at(generated, 1U),
        MYLITE_SQL_AST_GENERATED_COLUMN_VIRTUAL,
        "explicit virtual generated storage"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(generated, 0U),
        MYLITE_SQL_AST_LITERAL_NULL,
        "generated null expression"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(column, 3U),
        MYLITE_SQL_AST_COLUMN_COMMENT_ATTRIBUTE,
        "generated comment attribute"
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

    failures += parser_test_parse_sql(
        "CREATE TABLE combined_options (id INT) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 "
        "COLLATE=utf8mb4_0900_ai_ci COMMENT='metadata';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_options = parser_test_child_at(statement, 2U);
    failures +=
        parser_test_expect_child_count(table_options, 4U, "combined option list child count");
    failures += parser_test_expect_node(
        parser_test_child_at(table_options, 0U),
        MYLITE_SQL_AST_TABLE_ENGINE_OPTION,
        "combined engine option"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(table_options, 1U),
        MYLITE_SQL_AST_TABLE_CHARSET_OPTION,
        "combined charset option"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(table_options, 2U),
        MYLITE_SQL_AST_TABLE_COLLATION_OPTION,
        "combined collation option"
    );
    comment_option = parser_test_child_at(table_options, 3U);
    failures += parser_test_expect_node(
        comment_option,
        MYLITE_SQL_AST_TABLE_COMMENT_OPTION,
        "combined comment option"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(comment_option, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "combined comment literal"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(comment_option, 0U),
        "'metadata'",
        "comment literal"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE storage_options (id INT) ROW_FORMAT=COMPACT, KEY_BLOCK_SIZE=0 "
        "PACK_KEYS=DEFAULT CHECKSUM=2 STATS_PERSISTENT=1 STATS_AUTO_RECALC=0 "
        "STATS_SAMPLE_PAGES=7;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_options = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_child_count(
        table_options,
        storage_table_option_count,
        "storage option child count"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(table_options, 0U),
        MYLITE_SQL_AST_TABLE_ROW_FORMAT_OPTION,
        "row format option"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(table_options, 0U), 0U),
        "COMPACT",
        "row format value"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(table_options, 1U),
        MYLITE_SQL_AST_TABLE_KEY_BLOCK_SIZE_OPTION,
        "key block size option"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(table_options, 2U),
        MYLITE_SQL_AST_TABLE_PACK_KEYS_OPTION,
        "pack keys option"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(table_options, 3U),
        MYLITE_SQL_AST_TABLE_CHECKSUM_OPTION,
        "checksum option"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(table_options, 4U),
        MYLITE_SQL_AST_TABLE_STATS_PERSISTENT_OPTION,
        "stats persistent option"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(table_options, storage_stats_auto_recalc_option_index),
        MYLITE_SQL_AST_TABLE_STATS_AUTO_RECALC_OPTION,
        "stats auto recalc option"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(table_options, storage_stats_sample_pages_option_index),
        MYLITE_SQL_AST_TABLE_STATS_SAMPLE_PAGES_OPTION,
        "stats sample pages option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE reverse_options (id INT) COMMENT \"first\" COLLATE=utf8mb4_0900_ai_ci "
        "DEFAULT CHARSET=`utf8mb4` COMMENT='second' ENGINE=InnoDB;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_options = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_child_count(
        table_options,
        reverse_table_option_count,
        "reverse option child count"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(table_options, 0U),
        MYLITE_SQL_AST_TABLE_COMMENT_OPTION,
        "first reverse comment option"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(table_options, 0U), 0U),
        "\"first\"",
        "double quoted comment literal"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(table_options, 3U),
        MYLITE_SQL_AST_TABLE_COMMENT_OPTION,
        "second reverse comment option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE comment_identifier (COMMENT INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 1U), 0U), 0U),
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

    failures +=
        parser_test_parse_sql("ALTER TABLE commented COMMENT='new';", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_COMMENT_STATEMENT,
        "alter table comment statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "alter table comment child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "commented",
        "comment target table"
    );
    comment_option = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        comment_option,
        MYLITE_SQL_AST_TABLE_COMMENT_OPTION,
        "alter table comment option"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(comment_option, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "alter table comment literal"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(comment_option, 0U),
        "'new'",
        "alter comment text"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) ==
            MYLITE_SQL_AST_ALTER_ALGORITHM_UNSPECIFIED,
        "alter comment default algorithm"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_UNSPECIFIED,
        "alter comment default lock"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.commented COMMENT \"new\", ALGORITHM=INPLACE, LOCK=NONE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_COMMENT_STATEMENT,
        "qualified alter table comment statement"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.commented",
        "qualified target"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(statement, 1U), 0U),
        "\"new\"",
        "double quoted alter comment literal"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE,
        "alter comment inplace algorithm"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_NONE,
        "alter comment none lock"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE commented COMMENT=123;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "ALTER TABLE commented COMMENT=NULL;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
        "ALTER TABLE commented COMMENT=abc;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);
    failures += parser_test_parse_sql(
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

    failures += parser_test_parse_sql("SHOW FULL TABLES;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLES_STATEMENT,
        "show full tables statement"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_show_tables_is_full(statement),
        "show full tables has full flag"
    );
    failures += parser_test_expect_child_count(statement, 0U, "show full tables child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW FULL TABLES FROM app LIKE 'a\\_%';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLES_STATEMENT,
        "show full tables schema like"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_show_tables_is_full(statement),
        "show full tables schema like full flag"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "show full tables schema"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "show full tables like"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "'a\\_%'",
        "show full tables like"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW FULL TABLES IN app;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_show_tables_is_full(statement),
        "show full tables in full flag"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "show full tables in schema"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW EXTENDED FULL TABLES;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SHOW FULL EXTENDED TABLES;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW FULL TABLES WHERE Table_type = 'BASE TABLE';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_show_tables_is_full(statement),
        "show full tables where full flag"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "full tables where"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_show_table_status_where_statement(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "SHOW TABLE STATUS WHERE Name = 'numbers';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT,
        "show table status where"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 0U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "status bare where"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "SHOW TABLE STATUS FROM app WHERE `Rows` = '3';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT,
        "show table status schema where"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app",
        "show table status where schema"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "status schema where"
    );
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

    failures += parser_test_parse_sql(
        "CREATE TABLE binary_charset_options (id INT) DEFAULT CHARSET=binary COLLATE=binary;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_options = parser_test_child_at(statement, 2U);
    charset_option = parser_test_child_at(table_options, 0U);
    collation_option = parser_test_child_at(table_options, 1U);
    failures += parser_test_expect_node(
        charset_option,
        MYLITE_SQL_AST_TABLE_CHARSET_OPTION,
        "create table binary charset option"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(charset_option, 0U),
        "binary",
        "binary charset option name"
    );
    failures += parser_test_expect_node(
        collation_option,
        MYLITE_SQL_AST_TABLE_COLLATION_OPTION,
        "create table binary collation option"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(collation_option, 0U),
        "binary",
        "binary collation option name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE binary_collate_only (id INT) COLLATE=binary;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_options = parser_test_child_at(statement, 2U);
    collation_option = parser_test_child_at(table_options, 0U);
    failures += parser_test_expect_node(
        collation_option,
        MYLITE_SQL_AST_TABLE_COLLATION_OPTION,
        "create table binary collate-only option"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(collation_option, 0U),
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

    failures += parser_test_parse_sql(
        "ALTER TABLE app.simple_lifecycle ADD COLUMN added BIGINT FIRST;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT,
        "alter table add column first statement"
    );
    failures += parser_test_expect_child_count(statement, 3U, "alter add first child count");
    position = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_node(
        position,
        MYLITE_SQL_AST_COLUMN_POSITION_FIRST,
        "alter add first position"
    );
    failures +=
        parser_test_expect_child_count(position, 0U, "alter add first position child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.simple_lifecycle ADD COLUMN added BIGINT AFTER other_col;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT,
        "alter table add column after statement"
    );
    failures += parser_test_expect_child_count(statement, 3U, "alter add after child count");
    position = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_node(
        position,
        MYLITE_SQL_AST_COLUMN_POSITION_AFTER,
        "alter add after position"
    );
    failures +=
        parser_test_expect_child_count(position, 1U, "alter add after position child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(position, 0U),
        "other_col",
        "alter add after column name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.simple_lifecycle MODIFY old_col BIGINT FIRST;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT,
        "alter table modify column first statement"
    );
    failures += parser_test_expect_child_count(statement, 3U, "alter modify first child count");
    position = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_node(
        position,
        MYLITE_SQL_AST_COLUMN_POSITION_FIRST,
        "alter modify first position"
    );
    failures +=
        parser_test_expect_child_count(position, 0U, "alter modify first position child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.simple_lifecycle MODIFY old_col BIGINT AFTER other_col;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT,
        "alter table modify column after statement"
    );
    failures += parser_test_expect_child_count(statement, 3U, "alter modify after child count");
    position = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_node(
        position,
        MYLITE_SQL_AST_COLUMN_POSITION_AFTER,
        "alter modify after position"
    );
    failures +=
        parser_test_expect_child_count(position, 1U, "alter modify after position child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(position, 0U),
        "other_col",
        "alter modify after column name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.simple_lifecycle CHANGE old_col new_col BIGINT FIRST;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT,
        "alter table change column first statement"
    );
    failures += parser_test_expect_child_count(statement, 4U, "alter change first child count");
    position = parser_test_child_at(statement, 3U);
    failures += parser_test_expect_node(
        position,
        MYLITE_SQL_AST_COLUMN_POSITION_FIRST,
        "alter change first position"
    );
    failures +=
        parser_test_expect_child_count(position, 0U, "alter change first position child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.simple_lifecycle CHANGE old_col new_col BIGINT AFTER other_col;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT,
        "alter table change column after statement"
    );
    failures += parser_test_expect_child_count(statement, 4U, "alter change after child count");
    position = parser_test_child_at(statement, 3U);
    failures += parser_test_expect_node(
        position,
        MYLITE_SQL_AST_COLUMN_POSITION_AFTER,
        "alter change after position"
    );
    failures +=
        parser_test_expect_child_count(position, 1U, "alter change after position child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(position, 0U),
        "other_col",
        "alter change after column name"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_column_position_nonreserved_identifier_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *position = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "CREATE TABLE first (after INT, first INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "first",
        "first table identifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(columns, 0U), 0U),
        "after",
        "after column identifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(columns, 1U), 0U),
        "first",
        "first column identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE first ADD first BIGINT AFTER after;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    position = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(statement, 1U), 0U),
        "first",
        "add first target identifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(position, 0U),
        "after",
        "add after target identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE first MODIFY first BIGINT AFTER after;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    position = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(statement, 1U), 0U),
        "first",
        "modify first target identifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(position, 0U),
        "after",
        "modify after target identifier"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE first CHANGE after first BIGINT AFTER after;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    position = parser_test_child_at(statement, 3U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "after",
        "change after old identifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(statement, 2U), 0U),
        "first",
        "change first new identifier"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(position, 0U),
        "after",
        "change after target identifier"
    );
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

    failures += parser_test_parse_sql(
        "CREATE TABLE column_charset ("
        "v VARCHAR(10) CHARACTER SET utf8mb4 COLLATE 'utf8mb4_bin', "
        "t TEXT CHARSET `utf8mb4`, "
        "c CHAR(2) COLLATE utf8mb4_unicode_ci NOT NULL"
        ");",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(columns, 0U);
    charset_option = parser_test_child_at(column, 2U);
    collation_option = parser_test_child_at(column, 3U);
    failures += parser_test_expect_node(
        charset_option,
        MYLITE_SQL_AST_COLUMN_CHARSET_ATTRIBUTE,
        "column charset attribute"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(charset_option, 0U),
        "utf8mb4",
        "column charset name"
    );
    failures += parser_test_expect_node(
        collation_option,
        MYLITE_SQL_AST_COLUMN_COLLATION_ATTRIBUTE,
        "column collation attribute"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(collation_option, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "column string collation"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE column_binary_charset (v VARCHAR(10) CHARACTER SET binary, "
        "c CHAR(3) COLLATE binary);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(columns, 0U);
    charset_option = parser_test_child_at(column, 2U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(charset_option, 0U),
        "binary",
        "binary charset name"
    );
    column = parser_test_child_at(columns, 1U);
    collation_option = parser_test_child_at(column, 2U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(collation_option, 0U),
        "binary",
        "binary collation name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE column_comment ("
        "a INT COMMENT 'alpha', "
        "b VARCHAR(5) CHARACTER SET ascii COMMENT 'bee' NOT NULL, "
        "c INT COMMENT 'first' COMMENT 'second'"
        ");",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(columns, 0U);
    comment_attribute = parser_test_child_at(column, 2U);
    failures += parser_test_expect_node(
        comment_attribute,
        MYLITE_SQL_AST_COLUMN_COMMENT_ATTRIBUTE,
        "column comment attribute"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(comment_attribute, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "column comment literal"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(comment_attribute, 0U),
        "'alpha'",
        "column comment literal text"
    );
    column = parser_test_child_at(columns, 2U);
    failures += parser_test_expect_node(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_COLUMN_COMMENT_ATTRIBUTE,
        "first duplicate column comment"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(column, 3U),
        MYLITE_SQL_AST_COLUMN_COMMENT_ATTRIBUTE,
        "second duplicate column comment"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE column_comment ADD COLUMN d INT COMMENT 'dee' FIRST;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE column_comment MODIFY COLUMN a BIGINT COMMENT 'modified';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE column_comment CHANGE COLUMN b bb VARCHAR(7) COMMENT 'changed';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE column_comment_equal (a INT COMMENT='x');",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE column_comment_number (a INT COMMENT 123);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE column_comment_null (a INT COMMENT NULL);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE column_comment_before_charset ("
        "v VARCHAR(10) COMMENT 'x' CHARACTER SET utf8mb4);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE column_charset_equal (v VARCHAR(10) CHARACTER SET=utf8mb4);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE column_collation_equal (v VARCHAR(10) COLLATE=utf8mb4_bin);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE column_collation_before_charset ("
        "v VARCHAR(10) COLLATE utf8mb4_bin CHARACTER SET utf8mb4);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE column_charset_after_nullability ("
        "v VARCHAR(10) NOT NULL CHARACTER SET utf8mb4);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE column_charset_after_unique ("
        "v VARCHAR(10) UNIQUE CHARACTER SET utf8mb4);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
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

    failures += parser_test_parse_sql(
        "INSERT INTO simple_lifecycle VALUES ();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "insert empty row");
    failures += parser_test_expect_child_count(
        parser_test_child_at(statement, 1U),
        0U,
        "empty row omitted columns"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(statement, 2U),
        1U,
        "empty row list count"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(parser_test_child_at(statement, 2U), 0U),
        0U,
        "empty insert row value count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO simple_lifecycle () VALUES ();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_STATEMENT,
        "insert explicit empty"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "()",
        "explicit empty insert columns"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(statement, 1U),
        0U,
        "explicit empty column count"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(parser_test_child_at(statement, 2U), 0U),
        0U,
        "explicit empty insert row value count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO simple_lifecycle VALUE ();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "insert value empty");
    failures += parser_test_expect_child_count(
        parser_test_child_at(statement, 2U),
        1U,
        "value empty row list count"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(parser_test_child_at(statement, 2U), 0U),
        0U,
        "value empty insert row value count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO simple_lifecycle VALUE (1), (2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "insert value rows");
    failures += parser_test_expect_child_count(
        parser_test_child_at(statement, 2U),
        2U,
        "value row list count"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(parser_test_child_at(statement, 2U), 0U),
        1U,
        "first value row count"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(parser_test_child_at(statement, 2U), 1U),
        1U,
        "second value row count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO simple_lifecycle VALUES ROW();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_INSERT_STATEMENT, "insert row empty");
    failures += parser_test_expect_child_count(
        parser_test_child_at(statement, 2U),
        1U,
        "row empty row list count"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(parser_test_child_at(statement, 2U), 0U),
        0U,
        "row empty insert row value count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "INSERT INTO simple_lifecycle VALUES ROW(1), ROW(2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_INSERT_STATEMENT,
        "insert row constructors"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(statement, 2U),
        2U,
        "row constructor list count"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(parser_test_child_at(statement, 2U), 0U),
        1U,
        "first row constructor count"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(parser_test_child_at(statement, 2U), 1U),
        1U,
        "second row constructor count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE INTO simple_lifecycle () VALUES (), ();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT,
        "replace explicit empty"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 1U),
        "()",
        "explicit empty replace columns"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(statement, 2U),
        2U,
        "empty replace rows"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(parser_test_child_at(statement, 2U), 0U),
        0U,
        "first empty replace row"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(parser_test_child_at(statement, 2U), 1U),
        0U,
        "second empty replace row"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE INTO simple_lifecycle VALUE (1), (2);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT,
        "replace value rows"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(statement, 2U),
        2U,
        "replace value row list count"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(parser_test_child_at(statement, 2U), 0U),
        1U,
        "replace first value row count"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(parser_test_child_at(statement, 2U), 1U),
        1U,
        "replace second value row count"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPLACE INTO simple_lifecycle VALUES ROW(), ROW();",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT,
        "replace row constructors"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(statement, 2U),
        2U,
        "replace row constructor count"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(parser_test_child_at(statement, 2U), 0U),
        0U,
        "replace first empty row constructor"
    );
    failures += parser_test_expect_child_count(
        parser_test_child_at(parser_test_child_at(statement, 2U), 1U),
        0U,
        "replace second empty row constructor"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE value (value INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
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

    failures += parser_test_parse_sql("ANALYZE TABLE t;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    table_names = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ANALYZE_TABLE_STATEMENT,
        "analyze statement"
    );
    failures += parser_test_expect_child_count(statement, 1U, "analyze child count");
    failures +=
        parser_test_expect_node(table_names, MYLITE_SQL_AST_TABLE_NAME_LIST, "analyze target list");
    failures += parser_test_expect_child_count(table_names, 1U, "analyze target count");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(table_names, 0U), "t", "analyze target");
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("ANALYZE LOCAL TABLE app.t, other;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    table_names = parser_test_child_at(statement, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_ANALYZE_TABLE_STATEMENT, "analyze local");
    failures += parser_test_expect_child_count(table_names, 2U, "analyze local target count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_names, 0U),
        "app.t",
        "analyze qualified target"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_names, 1U),
        "other",
        "analyze second target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("ANALYZE NO_WRITE_TO_BINLOG TABLE t;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ANALYZE_TABLE_STATEMENT,
        "analyze no_write_to_binlog"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CHECK TABLE t QUICK FAST MEDIUM EXTENDED CHANGED FOR UPGRADE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_names = parser_test_child_at(statement, 0U);
    failures +=
        parser_test_expect_node(statement, MYLITE_SQL_AST_CHECK_TABLE_STATEMENT, "check statement");
    failures += parser_test_expect_child_count(statement, 1U, "check child count");
    failures += parser_test_expect_child_count(table_names, 1U, "check target count");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(table_names, 0U), "t", "check target");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("CHECKSUM TABLE t;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    table_names = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CHECKSUM_TABLE_STATEMENT,
        "checksum statement"
    );
    failures += parser_test_expect_child_count(statement, 1U, "checksum child count");
    failures += parser_test_expect_child_count(table_names, 1U, "checksum target count");
    failures +=
        parser_test_expect_span_text(parser_test_child_at(table_names, 0U), "t", "checksum target");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CHECKSUM TABLE app.t, other EXTENDED;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_names = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CHECKSUM_TABLE_STATEMENT,
        "checksum extended"
    );
    failures += parser_test_expect_child_count(table_names, 2U, "checksum extended target count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_names, 0U),
        "app.t",
        "checksum qualified target"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_names, 1U),
        "other",
        "checksum second target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("CHECKSUM TABLE t QUICK;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CHECKSUM_TABLE_STATEMENT,
        "checksum quick"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("OPTIMIZE LOCAL TABLE t;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    table_names = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_OPTIMIZE_TABLE_STATEMENT,
        "optimize statement"
    );
    failures +=
        parser_test_expect_span_text(parser_test_child_at(table_names, 0U), "t", "optimize target");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPAIR NO_WRITE_TO_BINLOG TABLE app.t QUICK EXTENDED USE_FRM;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_names = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_REPAIR_TABLE_STATEMENT,
        "repair statement"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_names, 0U),
        "app.t",
        "repair target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("ANALYZE TABLE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ANALYZE TABLE t UPDATE HISTOGRAM ON c;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CHECKSUM LOCAL TABLE t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("CHECKSUM TABLE;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CHECKSUM TABLE QUICK t;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CHECKSUM TABLE t FAST;", MYLITE_SQL_PARSE_SYNTAX_ERROR, &result);
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CHECKSUM TABLE t QUICK EXTENDED;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CHECKSUM TABLE t QUICK, other;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "REPAIR TABLE t FOR UPGRADE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
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

    failures += parser_test_parse_sql(
        "CREATE TEMPORARY TABLE IF NOT EXISTS app.temp_lifecycle ("
        "id INT, note VARCHAR(10), KEY note_idx (note(3)));",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_name = parser_test_child_at(statement, 0U);
    columns = parser_test_child_at(statement, 1U);
    if_not_exists = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_STATEMENT,
        "create temporary table statement"
    );
    failures += parser_test_expect_node(
        table_name,
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "create temporary target"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_name, 0U),
        "app",
        "create temporary schema"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_name, 1U),
        "temp_lifecycle",
        "temporary table"
    );
    failures += parser_test_expect_child_count(columns, 3U, "temporary create item list");
    failures += parser_test_expect_node(
        if_not_exists,
        MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE,
        "temporary create if not exists clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "DROP TEMPORARY TABLE IF EXISTS first_temp, app.second_temp;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_names = parser_test_child_at(statement, 0U);
    if_exists = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_DROP_TEMPORARY_TABLE_STATEMENT,
        "drop temporary table statement"
    );
    failures +=
        parser_test_expect_child_count(statement, 2U, "drop temporary if exists child count");
    failures += parser_test_expect_child_count(table_names, 2U, "drop temporary table name count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_names, 0U),
        "first_temp",
        "first temp drop"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_names, 1U),
        "app.second_temp",
        "second temp drop"
    );
    failures += parser_test_expect_node(
        if_exists,
        MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE,
        "drop temporary if exists clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE temporary (temporary INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_name = parser_test_child_at(statement, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "temporary nonreserved create table statement"
    );
    failures +=
        parser_test_expect_span_text(table_name, "temporary", "temporary nonreserved table name");
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(columns, 0U), 0U),
        "temporary",
        "temporary nonreserved column name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("SELECT temporary FROM temporary;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "temporary nonreserved select"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(parser_test_child_at(statement, 0U), 0U), 0U),
        "temporary",
        "temporary nonreserved projection"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(statement, 1U), 0U),
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

    failures += parser_test_parse_sql(
        "CREATE TABLE serial_aliases (id SERIAL, nn SERIAL NOT NULL, nullable SERIAL NULL);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_child_count(columns, 3U, "serial alias column list");
    column = parser_test_child_at(columns, 0U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_integer_type(
        column_type,
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        1,
        "serial alias column type"
    );
    failures +=
        parser_test_expect_integer_display_width(column_type, NULL, "serial alias display width");
    failures += parser_test_expect_integer_serial_alias(column_type, "serial alias marker");
    failures += parser_test_expect_span_text(column_type, "SERIAL", "serial alias span");
    column = parser_test_child_at(columns, 1U);
    column_type = parser_test_child_at(column, 1U);
    failures +=
        parser_test_expect_integer_serial_alias(column_type, "serial not null alias marker");
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NOT_NULL,
        "serial alias not null"
    );
    column = parser_test_child_at(columns, 2U);
    column_type = parser_test_child_at(column, 1U);
    failures += parser_test_expect_integer_serial_alias(column_type, "serial null alias marker");
    failures += parser_test_expect_nullability(
        parser_test_child_at(column, 2U),
        MYLITE_SQL_AST_NULLABILITY_NULL,
        "serial alias null"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE serial_identifiers (SERIAL INT, serial BIGINT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "SERIAL",
        "serial identifier column name"
    );
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_INT,
        0,
        "serial identifier column type"
    );
    column = parser_test_child_at(columns, 1U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "serial",
        "lowercase serial identifier"
    );
    failures += parser_test_expect_integer_type(
        parser_test_child_at(column, 1U),
        MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
        0,
        "lowercase serial identifier column type"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE unsupported_serial_default_value (c SERIAL DEFAULT VALUE);",
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

    failures += parser_test_parse_sql(
        "CREATE TABLE IF NOT EXISTS app.clone LIKE other.source;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_name = parser_test_child_at(statement, 0U);
    source_table = parser_test_child_at(statement, 1U);
    if_not_exists = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_LIKE_STATEMENT,
        "create table like statement"
    );
    failures +=
        parser_test_expect_node(table_name, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "like target");
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_name, 0U),
        "app",
        "like target schema"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_name, 1U),
        "clone",
        "like target table"
    );
    failures +=
        parser_test_expect_node(source_table, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "like source");
    failures += parser_test_expect_span_text(
        parser_test_child_at(source_table, 0U),
        "other",
        "like source schema"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(source_table, 1U),
        "source",
        "like source table"
    );
    failures += parser_test_expect_node(
        if_not_exists,
        MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE,
        "like if not exists clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("CREATE TABLE clone (LIKE source);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    table_name = parser_test_child_at(statement, 0U);
    source_table = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_LIKE_STATEMENT,
        "parenthesized create table like statement"
    );
    failures += parser_test_expect_span_text(table_name, "clone", "parenthesized like target");
    failures += parser_test_expect_span_text(source_table, "source", "parenthesized like source");
    failures += parser_test_expect_child_count(statement, 2U, "parenthesized like child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE t (LIKE source, extra INT);",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE t LIKE source ENGINE=InnoDB;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TEMPORARY TABLE IF NOT EXISTS app.temp_clone LIKE other.source;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_name = parser_test_child_at(statement, 0U);
    source_table = parser_test_child_at(statement, 1U);
    if_not_exists = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_LIKE_STATEMENT,
        "create temporary table like statement"
    );
    failures += parser_test_expect_node(
        table_name,
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "temp like target"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_name, 0U),
        "app",
        "temp like target schema"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_name, 1U),
        "temp_clone",
        "temp like target table"
    );
    failures += parser_test_expect_node(
        source_table,
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "temp like source"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(source_table, 0U),
        "other",
        "temp like source schema"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(source_table, 1U),
        "source",
        "temp like source table"
    );
    failures += parser_test_expect_node(
        if_not_exists,
        MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE,
        "temp like if not exists clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TEMPORARY TABLE temp_clone (LIKE source);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_name = parser_test_child_at(statement, 0U);
    source_table = parser_test_child_at(statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_LIKE_STATEMENT,
        "parenthesized temporary create table like statement"
    );
    failures +=
        parser_test_expect_span_text(table_name, "temp_clone", "parenthesized temp like target");
    failures +=
        parser_test_expect_span_text(source_table, "source", "parenthesized temp like source");
    failures +=
        parser_test_expect_child_count(statement, 2U, "parenthesized temp like child count");
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

    failures += parser_test_parse_sql(
        "CREATE TABLE IF NOT EXISTS app.copy AS "
        "SELECT id AS copied_id, n FROM other.source s "
        "WHERE id >= 1 ORDER BY n DESC LIMIT 2;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_name = parser_test_child_at(statement, 0U);
    select_statement = parser_test_child_at(statement, 1U);
    if_not_exists = parser_test_child_at(statement, 2U);
    select_list = parser_test_child_at(select_statement, 0U);
    from_clause = parser_test_child_at(select_statement, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_SELECT_STATEMENT,
        "create table select statement"
    );
    failures +=
        parser_test_expect_node(table_name, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, "ctas target");
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_name, 0U),
        "app",
        "ctas target schema"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(table_name, 1U),
        "copy",
        "ctas target table"
    );
    failures += parser_test_expect_node(
        if_not_exists,
        MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE,
        "ctas if not exists clause"
    );
    failures += parser_test_expect_node(
        select_statement,
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "ctas source select"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 1U),
        "copied_id",
        "ctas source alias"
    );
    failures += parser_test_expect_node(from_clause, MYLITE_SQL_AST_FROM_TABLE, "ctas source from");
    failures += parser_test_expect_span_text(
        parser_test_child_at(from_clause, 0U),
        "other.source",
        "ctas source table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(from_clause, 1U),
        "s",
        "ctas source alias"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select_statement, 2U),
        MYLITE_SQL_AST_WHERE_CLAUSE,
        "ctas where"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select_statement, 3U),
        MYLITE_SQL_AST_ORDER_BY_CLAUSE,
        "ctas order"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(select_statement, 4U),
        MYLITE_SQL_AST_LIMIT_CLAUSE,
        "ctas limit"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE copy SELECT * FROM source;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_SELECT_STATEMENT,
        "create table select without as"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "copy",
        "ctas no-as target"
    );
    failures += parser_test_expect_child_count(statement, 2U, "ctas no-as child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TEMPORARY TABLE IF NOT EXISTS copy AS SELECT * FROM source;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_name = parser_test_child_at(statement, 0U);
    select_statement = parser_test_child_at(statement, 1U);
    if_not_exists = parser_test_child_at(statement, 2U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_SELECT_STATEMENT,
        "create temporary table select statement"
    );
    failures += parser_test_expect_span_text(table_name, "copy", "temporary ctas target");
    failures += parser_test_expect_node(
        select_statement,
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "temporary ctas source select"
    );
    failures += parser_test_expect_node(
        if_not_exists,
        MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE,
        "temporary ctas if not exists clause"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TEMPORARY TABLE copy SELECT * FROM source;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_SELECT_STATEMENT,
        "create temporary table select without as"
    );
    failures += parser_test_expect_child_count(statement, 2U, "temporary ctas no-as child count");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE copy (id INT) AS SELECT id FROM source;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_create_view_lifecycle_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *view_name = NULL;
    const struct mylite_sql_ast_node *select_statement = NULL;
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *drop_list = NULL;
    int failures = 0;

    failures += parser_test_parse_sql(
        "CREATE VIEW app.v AS SELECT id AS view_id, source.name FROM source;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    view_name = parser_test_child_at(statement, 0U);
    select_statement = parser_test_child_at(statement, 1U);
    select_list = parser_test_child_at(select_statement, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_VIEW_STATEMENT,
        "create view statement"
    );
    failures += parser_test_expect_node(
        view_name,
        MYLITE_SQL_AST_QUALIFIED_IDENTIFIER,
        "create view target"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(view_name, 0U),
        "app",
        "create view target schema"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(view_name, 1U),
        "v",
        "create view target name"
    );
    failures += parser_test_expect_node(
        select_statement,
        MYLITE_SQL_AST_SELECT_STATEMENT,
        "view source select"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(parser_test_child_at(select_list, 0U), 1U),
        "view_id",
        "view source alias"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("DROP VIEW IF EXISTS v, app.other;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    drop_list = parser_test_child_at(statement, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_DROP_VIEW_STATEMENT,
        "drop view statement"
    );
    failures += parser_test_expect_node(
        parser_test_child_at(statement, 1U),
        MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE,
        "drop view if exists"
    );
    failures += parser_test_expect_child_count(drop_list, 2U, "drop view target count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(drop_list, 0U),
        "v",
        "drop view first target"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(drop_list, 1U),
        "app.other",
        "drop view second target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("SHOW CREATE VIEW app.v;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_SHOW_CREATE_VIEW_STATEMENT,
        "show create view statement"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.v",
        "show create view target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql("CREATE TABLE view (id INT);", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_CREATE_TABLE_STATEMENT,
        "view keyword table name statement"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "view",
        "view keyword table name"
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

    failures += parser_test_parse_sql(
        "ALTER TABLE app.simple_lifecycle DEFAULT CHARSET=utf8mb4 "
        "COLLATE='utf8mb4_0900_ai_ci';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_options = parser_test_child_at(statement, 1U);
    charset_option = parser_test_child_at(table_options, 0U);
    collation_option = parser_test_child_at(table_options, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DEFAULT_CHARSET_COLLATION_STATEMENT,
        "alter table default charset collation statement"
    );
    failures +=
        parser_test_expect_child_count(statement, 2U, "alter charset collation child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.simple_lifecycle",
        "alter charset collation target"
    );
    failures += parser_test_expect_node(
        table_options,
        MYLITE_SQL_AST_TABLE_OPTION_LIST,
        "alter charset collation option list"
    );
    failures +=
        parser_test_expect_child_count(table_options, 2U, "alter charset collation option count");
    failures += parser_test_expect_node(
        charset_option,
        MYLITE_SQL_AST_TABLE_CHARSET_OPTION,
        "alter charset option"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(charset_option, 0U),
        "utf8mb4",
        "alter charset name"
    );
    failures += parser_test_expect_node(
        collation_option,
        MYLITE_SQL_AST_TABLE_COLLATION_OPTION,
        "alter collation option"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(collation_option, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "alter string collation"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(collation_option, 0U),
        "'utf8mb4_0900_ai_ci'",
        "alter collation name"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle DEFAULT CHARACTER SET utf8mb4;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle DEFAULT CHARACTER SET=utf8mb4;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle CHARSET `utf8mb4`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle DEFAULT COLLATE utf8mb4_0900_ai_ci;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle DEFAULT CHARSET=binary;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle DEFAULT COLLATE binary;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE simple_lifecycle DEFAULT CHARSET=utf8mb4 CHARSET=utf8mb4;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_options = parser_test_child_at(statement, 1U);
    failures +=
        parser_test_expect_child_count(table_options, 2U, "duplicate alter charset options");
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name DEFAULT CHARSET=DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name CHARACTER SET DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name COLLATE=DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name DEFAULT CHARACTER SET;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name DEFAULT CHARSET=utf8mb4, COLLATE=utf8mb4_0900_ai_ci;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name ENGINE=InnoDB;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.old_name CONVERT TO CHARACTER SET utf8mb4 "
        "COLLATE 'utf8mb4_bin';",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_options = parser_test_child_at(statement, 1U);
    charset_option = parser_test_child_at(table_options, 0U);
    collation_option = parser_test_child_at(table_options, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_CONVERT_CHARACTER_SET_STATEMENT,
        "alter table convert character set statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "alter convert charset child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.old_name",
        "alter convert target"
    );
    failures += parser_test_expect_node(
        table_options,
        MYLITE_SQL_AST_TABLE_OPTION_LIST,
        "alter convert option list"
    );
    failures += parser_test_expect_child_count(table_options, 2U, "alter convert option count");
    failures += parser_test_expect_node(
        charset_option,
        MYLITE_SQL_AST_TABLE_CHARSET_OPTION,
        "convert charset option"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(charset_option, 0U),
        "utf8mb4",
        "convert charset"
    );
    failures += parser_test_expect_node(
        collation_option,
        MYLITE_SQL_AST_TABLE_COLLATION_OPTION,
        "convert collation option"
    );
    failures += parser_test_expect_literal(
        parser_test_child_at(collation_option, 0U),
        MYLITE_SQL_AST_LITERAL_STRING,
        "convert string collation"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(collation_option, 0U),
        "'utf8mb4_bin'",
        "convert collation"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name CONVERT TO CHARSET `utf8mb4` COLLATE `utf8mb4_0900_ai_ci`;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name CONVERT TO CHARACTER SET binary;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name CONVERT TO CHARACTER SET utf8mb4 COLLATE binary;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name CONVERT TO CHARACTER SET DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    table_options = parser_test_child_at(statement, 1U);
    charset_option = parser_test_child_at(table_options, 0U);
    failures +=
        parser_test_expect_child_count(table_options, 1U, "alter convert default option count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(charset_option, 0U),
        "DEFAULT",
        "convert default"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name CONVERT TO CHARACTER SET=utf8mb4;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name CONVERT TO CHARACTER SET utf8mb4 COLLATE DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name CONVERT TO CHARACTER SET utf8mb4 DEFAULT COLLATE utf8mb4_bin;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name CONVERT TO CHARACTER SET utf8mb4, ALGORITHM=COPY;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name ALGORITHM=INSTANT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name LOCK=DEFAULT;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
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

    failures += parser_test_parse_sql(
        "ALTER TABLE app.old_name ORDER BY app.old_name.id DESC, `value` ASC;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    items = parser_test_child_at(statement, 1U);
    first_item = parser_test_child_at(items, 0U);
    second_item = parser_test_child_at(items, 1U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ORDER_BY_STATEMENT,
        "alter table order by statement"
    );
    failures += parser_test_expect_child_count(statement, 2U, "alter table order by child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.old_name",
        "alter table order by target"
    );
    failures +=
        parser_test_expect_node(items, MYLITE_SQL_AST_ORDER_BY_ITEM_LIST, "alter order list");
    failures += parser_test_expect_child_count(items, 2U, "alter order list count");
    failures +=
        parser_test_expect_node(first_item, MYLITE_SQL_AST_ORDER_BY_ITEM, "alter first order item");
    failures += parser_test_expect_span_text(
        parser_test_child_at(first_item, 0U),
        "app.old_name.id",
        "alter first order key"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(first_item, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DESC,
        "alter first order direction"
    );
    failures += parser_test_expect_node(
        second_item,
        MYLITE_SQL_AST_ORDER_BY_ITEM,
        "alter second order item"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(second_item, 0U),
        "`value`",
        "alter second order key"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(second_item, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_ASC,
        "alter second order direction"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("ALTER TABLE old_name ORDER BY id;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    items = parser_test_child_at(statement, 1U);
    first_item = parser_test_child_at(items, 0U);
    failures += parser_test_expect_node(
        first_item,
        MYLITE_SQL_AST_ORDER_BY_ITEM,
        "alter default order item"
    );
    failures += parser_test_expect_order_direction(
        parser_test_child_at(first_item, 1U),
        MYLITE_SQL_AST_ORDER_DIRECTION_DEFAULT,
        "alter default order direction"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name ORDER BY old_name.id ASC;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name ORDER BY 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name ORDER BY id + 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name ORDER BY id LIMIT 1;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name ORDER BY;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
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

    failures += parser_test_parse_sql(
        "ALTER TABLE app.simple_lifecycle ADD COLUMN added INT, ALGORITHM=INSTANT, LOCK=DEFAULT;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures +=
        parser_test_expect_child_count(statement, 2U, "alter add column options child count");
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT,
        "alter add column algorithm option"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_DEFAULT,
        "alter add column lock option"
    );
    failures += parser_test_expect_span_text(
        statement,
        "ALTER TABLE app.simple_lifecycle ADD COLUMN added INT, ALGORITHM=INSTANT, LOCK=DEFAULT",
        "alter add column options span"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "CREATE TABLE algorithm_identifier (algorithm INT);",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    columns = parser_test_child_at(statement, 1U);
    column = parser_test_child_at(columns, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "algorithm_identifier",
        "algorithm table"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(column, 0U),
        "algorithm",
        "algorithm column"
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_force_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures += parser_test_parse_sql("ALTER TABLE old_name FORCE;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_FORCE_STATEMENT,
        "alter table force statement"
    );
    failures += parser_test_expect_child_count(statement, 1U, "alter table force child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "old_name",
        "alter table force target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures +=
        parser_test_parse_sql("ALTER TABLE app.old_name FORCE;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.old_name",
        "qualified force target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name FORCE ORDER BY id;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name FORCE, FORCE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name FORCE, ALGORITHM=COPY;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY,
        "alter force algorithm option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name FORCE LOCK=NONE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name FORCE id;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}

static int test_alter_table_disable_enable_keys_statements(void) {
    struct mylite_sql_parse_result result;
    const struct mylite_sql_ast_node *statement = NULL;
    int failures = 0;

    failures +=
        parser_test_parse_sql("ALTER TABLE old_name DISABLE KEYS;", MYLITE_SQL_PARSE_OK, &result);
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_DISABLE_KEYS_STATEMENT,
        "alter table disable keys statement"
    );
    failures +=
        parser_test_expect_child_count(statement, 1U, "alter table disable keys child count");
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "old_name",
        "disable keys target"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE app.old_name ENABLE KEYS, ALGORITHM=COPY, LOCK=EXCLUSIVE;",
        MYLITE_SQL_PARSE_OK,
        &result
    );
    statement = parser_test_child_at(result.root, 0U);
    failures += parser_test_expect_node(
        statement,
        MYLITE_SQL_AST_ALTER_TABLE_ENABLE_KEYS_STATEMENT,
        "alter table enable keys statement"
    );
    failures += parser_test_expect_span_text(
        parser_test_child_at(statement, 0U),
        "app.old_name",
        "enable keys target"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY,
        "enable keys algorithm option"
    );
    failures += parser_test_expect_true(
        mylite_sql_ast_node_alter_lock(statement) == MYLITE_SQL_AST_ALTER_LOCK_EXCLUSIVE,
        "enable keys lock option"
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name DISABLE KEY;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name DISABLE KEYS, ENABLE KEYS;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    failures += parser_test_parse_sql(
        "ALTER TABLE old_name ENABLE KEYS LOCK=NONE;",
        MYLITE_SQL_PARSE_SYNTAX_ERROR,
        &result
    );
    mylite_sql_parse_result_deinit(&result);

    return failures;
}
