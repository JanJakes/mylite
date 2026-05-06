#include "mylite_show.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_field_descriptor.h"
#include "mylite_metadata.h"
#include "mylite_runtime.h"
#include "mylite_show_types.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "mylite_storage_engine.h"
#include "sqlite3.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

static struct mylite_field_descriptor show_engines_field_descriptor(uint64_t length, bool nullable);
static int show_engines_sql(mylite_db *database, char **out_sql);
static int append_show_index_where_expression(mylite_db *database, sqlite3_str *sql,
                                              const struct mylite_sql_ast_node *expression);
static int append_show_index_expression_list(mylite_db *database, sqlite3_str *sql,
                                             const struct mylite_sql_ast_node *list);
static int append_show_index_identifier(sqlite3_str *sql,
                                        const struct mylite_sql_ast_node *expression);
static int append_show_index_literal(mylite_db *database, sqlite3_str *sql,
                                     const struct mylite_sql_ast_node *expression);
static int append_show_index_raw_span(sqlite3_str *sql,
                                      const struct mylite_sql_ast_node *expression);
static const char *show_index_column_name(const struct mylite_sql_ast_node *expression);
static const char *show_index_binary_operator_sql(enum mylite_sql_ast_operator operator_kind);
static const unsigned int mylite_show_latin1_swedish_ci_charset_id = 8U;
static const unsigned int mylite_show_not_fixed_decimals = 31U;

static const char show_schemas_sql[] =
    "SELECT name AS \"Database\" FROM __mylite_schema_catalog ORDER BY name COLLATE BINARY";

int mylite_show_prepare_engines_statement(mylite_db *database, mylite_stmt **out_stmt)
{
    char *sqlite_sql = NULL;
    mylite_stmt *stmt = NULL;
    int status = show_engines_sql(database, &sqlite_sql);

    *out_stmt = NULL;
    if (status == MYLITE_OK) {
        status = mylite_statement_prepare_sqlite(database, sqlite_sql, &stmt);
    }
    if (status == MYLITE_OK) {
        status = mylite_show_attach_engines_result_metadata(database, stmt);
    }

    if (status == MYLITE_OK) {
        *out_stmt = stmt;
    } else {
        mylite_finalize(stmt);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
    }
    sqlite3_free(sqlite_sql);
    return status;
}

int mylite_show_prepare_schemas_statement(mylite_db *database, mylite_stmt **out_stmt)
{
    return mylite_statement_prepare_sqlite(database, mylite_show_schemas_sql(), out_stmt);
}

static int show_engines_sql(mylite_db *database, char **out_sql)
{
    return mylite_storage_engine_show_sql(database, out_sql);
}

char *mylite_show_columns_sql(mylite_db *database, const struct mylite_show_columns_query *query)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql, "SELECT column_name AS \"Field\", column_type AS \"Type\"");
    if (query->full) {
        sqlite3_str_appendf(sql, ", collation_name AS \"Collation\"");
    }
    sqlite3_str_appendf(sql, ", is_nullable AS \"Null\", column_key AS \"Key\", "
                             "column_default AS \"Default\", extra AS \"Extra\"");
    if (query->full) {
        sqlite3_str_appendf(sql, ", privileges AS \"Privileges\", column_comment AS \"Comment\"");
    }
    sqlite3_str_appendf(sql, " FROM %s WHERE table_schema = %Q AND table_name = %Q",
                        mylite_catalog_column_catalog_name(query->temporary), query->schema_name,
                        query->table_name);
    if (query->like_pattern != NULL) {
        sqlite3_str_appendf(sql, " AND column_name LIKE %Q ESCAPE '\\'", query->like_pattern);
    }
    sqlite3_str_appendf(sql, " ORDER BY ordinal_position");
    return sqlite3_str_finish(sql);
}

int mylite_show_index_sql(mylite_db *database, const struct mylite_show_index_query *query,
                          char **out_sql)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);
    char *finished_sql = NULL;
    int status = MYLITE_OK;

    *out_sql = NULL;
    if (sql == NULL) {
        return MYLITE_NOMEM;
    }

    sqlite3_str_appendf(sql,
                        "SELECT \"Table\", \"Non_unique\", \"Key_name\", \"Seq_in_index\", "
                        "\"Column_name\", \"Collation\", \"Cardinality\", \"Sub_part\", "
                        "\"Packed\", \"Null\", \"Index_type\", \"Comment\", \"Index_comment\", "
                        "\"Visible\", \"Expression\" FROM ("
                        "SELECT table_name AS \"Table\", non_unique AS \"Non_unique\", "
                        "index_name AS \"Key_name\", seq_in_index AS \"Seq_in_index\", "
                        "column_name AS \"Column_name\", collation AS \"Collation\", "
                        "cardinality AS \"Cardinality\", sub_part AS \"Sub_part\", "
                        "packed AS \"Packed\", nullable AS \"Null\", index_type AS \"Index_type\", "
                        "comment AS \"Comment\", index_comment AS \"Index_comment\", "
                        "is_visible AS \"Visible\", expression AS \"Expression\", "
                        "rowid AS \"__mylite_order\" "
                        "FROM %s "
                        "WHERE table_schema = %Q AND table_name = %Q) AS show_index",
                        mylite_catalog_index_catalog_name(query->temporary), query->schema_name,
                        query->table_name);
    if (status == MYLITE_OK && query->where_expression != NULL) {
        sqlite3_str_appendall(sql, " WHERE ");
        status = append_show_index_where_expression(database, sql, query->where_expression);
    }
    if (status == MYLITE_OK) {
        sqlite3_str_appendall(sql, " ORDER BY \"__mylite_order\"");
    }

    finished_sql = sqlite3_str_finish(sql);
    if (status != MYLITE_OK) {
        sqlite3_free(finished_sql);
        if (status == MYLITE_UNSUPPORTED) {
            (void)mylite_diagnostics_set_error_message(
                database, "SHOW INDEX WHERE expression is not supported");
        }
        return status;
    }
    if (finished_sql == NULL) {
        return MYLITE_NOMEM;
    }

    *out_sql = finished_sql;
    return MYLITE_OK;
}

static int append_show_index_where_expression(mylite_db *database, sqlite3_str *sql,
                                              const struct mylite_sql_ast_node *expression)
{
    const char *operator_sql = NULL;

    expression = mylite_sql_ast_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    if (show_index_column_name(expression) != NULL) {
        return append_show_index_identifier(sql, expression);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return append_show_index_literal(database, sql, expression);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        if (expression->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
            expression->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE ||
            expression->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT) {
            sqlite3_str_appendall(
                sql,
                expression->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT ? "NOT " : "");
            if (expression->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
                sqlite3_str_appendall(sql, "-");
            } else if (expression->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
                sqlite3_str_appendall(sql, "+");
            }
            return append_show_index_where_expression(database, sql,
                                                      mylite_ast_child_at(expression, 0U));
        }
        if (expression->operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NULL ||
            expression->operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL) {
            sqlite3_str_appendall(sql, "(");
            int status = append_show_index_where_expression(database, sql,
                                                            mylite_ast_child_at(expression, 0U));

            if (status == MYLITE_OK) {
                sqlite3_str_appendall(sql, expression->operator_kind ==
                                                   MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL
                                               ? " IS NOT NULL)"
                                               : " IS NULL)");
            }
            return status;
        }
        return MYLITE_UNSUPPORTED;
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        if (expression->operator_kind == MYLITE_SQL_AST_OPERATOR_IN ||
            expression->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_IN) {
            sqlite3_str_appendall(sql, "(");
            int status = append_show_index_where_expression(database, sql,
                                                            mylite_ast_child_at(expression, 0U));

            if (status == MYLITE_OK) {
                sqlite3_str_appendall(
                    sql, expression->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_IN ? " NOT IN ("
                                                                                     : " IN (");
                status = append_show_index_expression_list(database, sql,
                                                           mylite_ast_child_at(expression, 1U));
            }
            if (status == MYLITE_OK) {
                sqlite3_str_appendall(sql, "))");
            }
            return status;
        }

        operator_sql = show_index_binary_operator_sql(expression->operator_kind);
        if (operator_sql == NULL) {
            return MYLITE_UNSUPPORTED;
        }
        sqlite3_str_appendall(sql, "(");
        int status =
            append_show_index_where_expression(database, sql, mylite_ast_child_at(expression, 0U));

        if (status == MYLITE_OK) {
            sqlite3_str_appendf(sql, " %s ", operator_sql);
            status = append_show_index_where_expression(database, sql,
                                                        mylite_ast_child_at(expression, 1U));
        }
        if (status == MYLITE_OK) {
            sqlite3_str_appendall(sql, ")");
        }
        return status;
    case MYLITE_SQL_AST_EXPRESSION_LIST:
        return append_show_index_expression_list(database, sql, expression);
    default:
        return MYLITE_UNSUPPORTED;
    }
}

static int append_show_index_expression_list(mylite_db *database, sqlite3_str *sql,
                                             const struct mylite_sql_ast_node *list)
{
    bool appended = false;

    if (list == NULL || list->kind != MYLITE_SQL_AST_EXPRESSION_LIST) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *item = list->first_child; item != NULL;
         item = item->next_sibling) {
        if (appended) {
            sqlite3_str_appendall(sql, ", ");
        }
        int status = append_show_index_where_expression(database, sql, item);

        if (status != MYLITE_OK) {
            return status;
        }
        appended = true;
    }
    return appended ? MYLITE_OK : MYLITE_UNSUPPORTED;
}

static int append_show_index_identifier(sqlite3_str *sql,
                                        const struct mylite_sql_ast_node *expression)
{
    const char *column_name = show_index_column_name(expression);

    if (column_name == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    sqlite3_str_appendf(sql, "\"%w\"", column_name);
    return MYLITE_OK;
}

static int append_show_index_literal(mylite_db *database, sqlite3_str *sql,
                                     const struct mylite_sql_ast_node *expression)
{
    char *text = NULL;

    switch (expression->literal_kind) {
    case MYLITE_SQL_AST_LITERAL_NULL:
        sqlite3_str_appendall(sql, "NULL");
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_TRUE:
        sqlite3_str_appendall(sql, "TRUE");
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_FALSE:
        sqlite3_str_appendall(sql, "FALSE");
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_INTEGER:
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        return append_show_index_raw_span(sql, expression);
    case MYLITE_SQL_AST_LITERAL_STRING:
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        text = mylite_copy_string_literal_span(expression);
        if (text == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        sqlite3_str_appendf(sql, "'%q'", text);
        free(text);
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_BIT:
    case MYLITE_SQL_AST_LITERAL_NONE:
        return MYLITE_UNSUPPORTED;
    }
    return MYLITE_UNSUPPORTED;
}

static int append_show_index_raw_span(sqlite3_str *sql,
                                      const struct mylite_sql_ast_node *expression)
{
    if (expression == NULL || expression->span.length > (size_t)INT_MAX) {
        return MYLITE_NOMEM;
    }
    sqlite3_str_append(sql, expression->span.text, (int)expression->span.length);
    return MYLITE_OK;
}

static const char *show_index_column_name(const struct mylite_sql_ast_node *expression)
{
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        expression = mylite_ast_child_at(expression, 1U);
    }
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return NULL;
    }
    if (mylite_span_equal_ci(expression->span, "Table")) {
        return "Table";
    }
    if (mylite_span_equal_ci(expression->span, "Non_unique")) {
        return "Non_unique";
    }
    if (mylite_span_equal_ci(expression->span, "Key_name")) {
        return "Key_name";
    }
    if (mylite_span_equal_ci(expression->span, "Seq_in_index")) {
        return "Seq_in_index";
    }
    if (mylite_span_equal_ci(expression->span, "Column_name")) {
        return "Column_name";
    }
    if (mylite_span_equal_ci(expression->span, "Collation")) {
        return "Collation";
    }
    if (mylite_span_equal_ci(expression->span, "Cardinality")) {
        return "Cardinality";
    }
    if (mylite_span_equal_ci(expression->span, "Sub_part")) {
        return "Sub_part";
    }
    if (mylite_span_equal_ci(expression->span, "Packed")) {
        return "Packed";
    }
    if (mylite_span_equal_ci(expression->span, "Null")) {
        return "Null";
    }
    if (mylite_span_equal_ci(expression->span, "Index_type")) {
        return "Index_type";
    }
    if (mylite_span_equal_ci(expression->span, "Comment")) {
        return "Comment";
    }
    if (mylite_span_equal_ci(expression->span, "Index_comment")) {
        return "Index_comment";
    }
    if (mylite_span_equal_ci(expression->span, "Visible")) {
        return "Visible";
    }
    if (mylite_span_equal_ci(expression->span, "Expression")) {
        return "Expression";
    }
    return NULL;
}

static const char *show_index_binary_operator_sql(enum mylite_sql_ast_operator operator_kind)
{
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return "=";
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return "<>";
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return "<";
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return "<=";
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return ">";
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return ">=";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
        return "AND";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
        return "OR";
    case MYLITE_SQL_AST_OPERATOR_LIKE:
        return "LIKE";
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
        return "NOT LIKE";
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_NOT_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_NONE:
        return NULL;
    }
    return NULL;
}

char *mylite_show_tables_sql(mylite_db *database, const struct mylite_show_tables_query *query)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql, "SELECT TABLE_NAME AS \"%w\"", query->column_name);
    if (query->full) {
        sqlite3_str_appendf(sql, ", TABLE_TYPE AS \"Table_type\"");
    }
    sqlite3_str_appendf(
        sql,
        " FROM ("
        "SELECT 'information_schema' AS TABLE_SCHEMA, table_name AS TABLE_NAME, "
        "'SYSTEM VIEW' AS TABLE_TYPE FROM ("
        "SELECT 'CHARACTER_SETS' AS table_name "
        "UNION ALL SELECT 'CHECK_CONSTRAINTS' "
        "UNION ALL SELECT 'COLLATION_CHARACTER_SET_APPLICABILITY' "
        "UNION ALL SELECT 'COLLATIONS' "
        "UNION ALL SELECT 'SCHEMATA' "
        "UNION ALL SELECT 'TABLES' "
        "UNION ALL SELECT 'COLUMNS' "
        "UNION ALL SELECT 'ENGINES' "
        "UNION ALL SELECT 'KEYWORDS' "
        "UNION ALL SELECT 'KEY_COLUMN_USAGE' "
        "UNION ALL SELECT 'REFERENTIAL_CONSTRAINTS' "
        "UNION ALL SELECT 'STATISTICS' "
        "UNION ALL SELECT 'TABLE_CONSTRAINTS') "
        "UNION ALL "
        "SELECT table_schema AS TABLE_SCHEMA, table_name AS TABLE_NAME, table_type AS TABLE_TYPE "
        "FROM __mylite_table_catalog) "
        "WHERE TABLE_SCHEMA = %Q",
        query->schema_name);
    if (query->glob_pattern != NULL) {
        sqlite3_str_appendf(sql, " AND TABLE_NAME GLOB %Q", query->glob_pattern);
    }
    sqlite3_str_appendf(sql, " ORDER BY TABLE_NAME COLLATE BINARY");
    return sqlite3_str_finish(sql);
}

char *mylite_show_table_status_sql(mylite_db *database,
                                   const struct mylite_show_table_status_query *query)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendall(
        sql,
        "SELECT Name, Engine, Version, Row_format, Rows, Avg_row_length, Data_length, "
        "Max_data_length, Index_length, Data_free, Auto_increment, Create_time, Update_time, "
        "Check_time, Collation, Checksum, Create_options, Comment FROM ("
        "SELECT 'information_schema' AS TABLE_SCHEMA, table_name AS Name, NULL AS Engine, "
        "10 AS Version, NULL AS Row_format, 0 AS Rows, 0 AS Avg_row_length, 0 AS Data_length, "
        "0 AS Max_data_length, 0 AS Index_length, 0 AS Data_free, NULL AS Auto_increment, "
        "'1970-01-01 00:00:00' AS Create_time, NULL AS Update_time, NULL AS Check_time, "
        "NULL AS Collation, NULL AS Checksum, '' AS Create_options, '' AS Comment FROM ("
        "SELECT 'CHARACTER_SETS' AS table_name "
        "UNION ALL SELECT 'CHECK_CONSTRAINTS' "
        "UNION ALL SELECT 'COLLATION_CHARACTER_SET_APPLICABILITY' "
        "UNION ALL SELECT 'COLLATIONS' "
        "UNION ALL SELECT 'SCHEMATA' "
        "UNION ALL SELECT 'TABLES' "
        "UNION ALL SELECT 'COLUMNS' "
        "UNION ALL SELECT 'ENGINES' "
        "UNION ALL SELECT 'KEYWORDS' "
        "UNION ALL SELECT 'KEY_COLUMN_USAGE' "
        "UNION ALL SELECT 'REFERENTIAL_CONSTRAINTS' "
        "UNION ALL SELECT 'STATISTICS' "
        "UNION ALL SELECT 'TABLE_CONSTRAINTS') "
        "UNION ALL "
        "SELECT table_schema AS TABLE_SCHEMA, table_name AS Name, engine AS Engine, "
        "version AS Version, COALESCE(row_format, 'Dynamic') AS Row_format, "
        "COALESCE(table_rows, 0) AS Rows, COALESCE(avg_row_length, 0) AS Avg_row_length, "
        "COALESCE(data_length, 0) AS Data_length, COALESCE(max_data_length, 0) AS "
        "Max_data_length, COALESCE(index_length, 0) AS Index_length, "
        "COALESCE(data_free, 0) AS Data_free, auto_increment AS Auto_increment, "
        "create_time AS Create_time, update_time AS Update_time, check_time AS Check_time, "
        "table_collation AS Collation, checksum AS Checksum, create_options AS Create_options, "
        "table_comment AS Comment FROM __mylite_table_catalog) ");
    sqlite3_str_appendf(sql, "WHERE TABLE_SCHEMA = %Q", query->schema_name);
    if (query->glob_pattern != NULL) {
        sqlite3_str_appendf(sql, " AND Name GLOB %Q", query->glob_pattern);
    }
    sqlite3_str_appendall(sql, " ORDER BY Name COLLATE BINARY");
    return sqlite3_str_finish(sql);
}

const char *mylite_show_schemas_sql(void)
{
    return show_schemas_sql;
}

int mylite_show_attach_engines_result_metadata(mylite_db *database, mylite_stmt *stmt)
{
    static const struct mylite_show_engines_metadata_column columns[] = {
        {"Engine", 64U, false},     {"Support", 8U, false}, {"Comment", 80U, false},
        {"Transactions", 3U, true}, {"XA", 3U, true},       {"Savepoints", 3U, true},
    };
    struct mylite_result_metadata metadata = {0};

    metadata.columns = calloc(sizeof(columns) / sizeof(columns[0]), sizeof(*metadata.columns));
    if (metadata.columns == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    metadata.column_count = sizeof(columns) / sizeof(columns[0]);

    for (size_t index = 0U; index < metadata.column_count; ++index) {
        int status = mylite_result_metadata_copy_text(database, &metadata.columns[index].name,
                                                      columns[index].name);

        if (status != MYLITE_OK) {
            mylite_result_metadata_deinit(&metadata);
            return status;
        }
        metadata.columns[index].descriptor =
            show_engines_field_descriptor(columns[index].length, columns[index].nullable);
    }

    mylite_result_metadata_deinit(&stmt->result_metadata);
    stmt->result_metadata = metadata;
    return MYLITE_OK;
}

static struct mylite_field_descriptor show_engines_field_descriptor(uint64_t length, bool nullable)
{
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .length = length,
        .decimals = mylite_show_not_fixed_decimals,
        .charset_id = mylite_show_latin1_swedish_ci_charset_id,
        .nullable = nullable,
    };

    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    return descriptor;
}
