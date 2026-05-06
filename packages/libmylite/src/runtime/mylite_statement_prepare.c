#include "mylite_statement_prepare.h"

#include "mylite_connection.h"
#include "mylite_connection_statement.h"
#include "mylite_diagnostics.h"
#include "mylite_dml.h"
#include "mylite_error_codes.h"
#include "mylite_prepared_statements.h"
#include "mylite_runtime.h"
#include "mylite_select_context.h"
#include "mylite_select_prepare.h"
#include "mylite_select_union.h"
#include "mylite_show.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "mylite_statement_types.h"
#include "mylite_table_maintenance.h"
#include "mylite_user_variables.h"
#include "mylite_values_query.h"

#include <stdbool.h>

static int prepare_parsed_statement(mylite_db *database, const struct mylite_sql_ast_node *root,
                                    const char *sql, size_t sql_length, mylite_stmt **out_stmt,
                                    const struct mylite_statement_prepare_callbacks *callbacks);
static enum mylite_stmt_kind
placeholder_statement_kind(const struct mylite_sql_ast_node *statement);
static bool placeholder_statement_is_table_maintenance(const struct mylite_sql_ast_node *statement);

int mylite_prepare(mylite_db *database, const char *sql, size_t length, mylite_stmt **out_stmt)
{
    return mylite_statement_prepare_with_callbacks(
        database, sql, length, out_stmt, mylite_select_context_statement_prepare_callbacks());
}

int mylite_statement_prepare_with_callbacks(
    mylite_db *database, const char *sql, size_t length, mylite_stmt **out_stmt,
    const struct mylite_statement_prepare_callbacks *callbacks)
{
    struct mylite_sql_parse_result parse_result;
    enum mylite_sql_parse_status parse_status = MYLITE_SQL_PARSE_OK;
    const struct mylite_sql_ast_node *statement = NULL;
    int status = MYLITE_OK;

    if (out_stmt == NULL) {
        return MYLITE_MISUSE;
    }
    *out_stmt = NULL;

    if (database == NULL || sql == NULL || callbacks == NULL ||
        callbacks->select_callbacks == NULL || callbacks->scalar_callbacks == NULL ||
        callbacks->union_callbacks == NULL) {
        return MYLITE_MISUSE;
    }

    mylite_diagnostics_clear_error_message(database);
    if (database->transaction_released) {
        return mylite_connection_set_released_error(database);
    }
    parse_status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = length,
            .modes = 0U,
        },
        &parse_result);
    if (parse_status != MYLITE_SQL_PARSE_OK) {
        mylite_diagnostics_clear_warnings(database);
        status = mylite_statement_map_parse_status(database, parse_status);
        if (status != MYLITE_NOMEM) {
            (void)mylite_diagnostics_append_current_error_condition(database,
                                                                    MYLITE_MYSQL_ER_PARSE_ERROR);
        }
        mylite_sql_parse_result_deinit(&parse_result);
        return status;
    }

    statement = mylite_ast_single_statement(parse_result.root);
    if (!mylite_statement_ast_preserves_diagnostics(statement)) {
        mylite_diagnostics_clear_warnings(database);
    }

    status =
        prepare_parsed_statement(database, parse_result.root, sql, length, out_stmt, callbacks);
    if (status != MYLITE_OK && status != MYLITE_NOMEM) {
        (void)mylite_diagnostics_ensure_current_error_condition(database,
                                                                MYLITE_MYSQL_ER_UNKNOWN_ERROR);
    }
    mylite_sql_parse_result_deinit(&parse_result);
    return status;
}

static int prepare_parsed_statement(mylite_db *database, const struct mylite_sql_ast_node *root,
                                    const char *sql, size_t sql_length, mylite_stmt **out_stmt,
                                    const struct mylite_statement_prepare_callbacks *callbacks)
{
    struct mylite_sqlite_translate_result translate_result;
    enum mylite_sqlite_translate_status translate_status = MYLITE_SQLITE_TRANSLATE_OK;
    const struct mylite_sql_ast_node *statement = mylite_ast_single_statement(root);
    int status = MYLITE_OK;

    if (statement != NULL) {
        switch (statement->kind) {
        case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
        case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
        case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
        case MYLITE_SQL_AST_USE_STATEMENT:
            return mylite_statement_prepare_schema_lifecycle_statement(database, statement,
                                                                       out_stmt, callbacks);
        case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
        case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
            return mylite_connection_prepare_charset_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_STATEMENT:
            return mylite_connection_prepare_system_variable_statement(database, statement,
                                                                       out_stmt);
        case MYLITE_SQL_AST_SET_USER_VARIABLE_STATEMENT:
            return mylite_user_variable_prepare_set_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_PREPARE_STATEMENT:
            return mylite_prepared_statement_prepare_prepare_statement(database, statement,
                                                                       out_stmt);
        case MYLITE_SQL_AST_EXECUTE_STATEMENT:
            return mylite_prepared_statement_prepare_execute_statement(database, statement,
                                                                       out_stmt);
        case MYLITE_SQL_AST_DEALLOCATE_PREPARE_STATEMENT:
            return mylite_prepared_statement_prepare_deallocate_statement(database, statement,
                                                                          out_stmt);
        case MYLITE_SQL_AST_PLACEHOLDER_STATEMENT:
            if (placeholder_statement_is_table_maintenance(statement)) {
                return mylite_table_maintenance_prepare_statement(database, statement, out_stmt);
            }
            return mylite_statement_prepare_custom_statement(
                database, placeholder_statement_kind(statement), statement, out_stmt, callbacks);
        case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
            return mylite_statement_prepare_custom_statement(database, MYLITE_STMT_CREATE_TABLE,
                                                             statement, out_stmt, callbacks);
        case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
            return mylite_statement_prepare_custom_statement(database, MYLITE_STMT_DROP_TABLE,
                                                             statement, out_stmt, callbacks);
        case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
            return mylite_statement_prepare_custom_statement(database, MYLITE_STMT_RENAME_TABLE,
                                                             statement, out_stmt, callbacks);
        case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
            return mylite_statement_prepare_custom_statement(database, MYLITE_STMT_TRUNCATE_TABLE,
                                                             statement, out_stmt, callbacks);
        case MYLITE_SQL_AST_ALTER_TABLE_STATEMENT:
            return mylite_statement_prepare_custom_statement(database, MYLITE_STMT_ALTER_TABLE,
                                                             statement, out_stmt, callbacks);
        case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
            return mylite_statement_prepare_custom_statement(database, MYLITE_STMT_CREATE_INDEX,
                                                             statement, out_stmt, callbacks);
        case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
            return mylite_statement_prepare_custom_statement(database, MYLITE_STMT_DROP_INDEX,
                                                             statement, out_stmt, callbacks);
        case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
            return mylite_dml_prepare_insert_values_statement(database, statement, sql, sql_length,
                                                              out_stmt);
        case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
            return mylite_dml_prepare_insert_set_statement(database, statement, sql, sql_length,
                                                           out_stmt);
        case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
            return mylite_dml_prepare_replace_values_statement(database, statement, sql, sql_length,
                                                               out_stmt);
        case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
            return mylite_dml_prepare_replace_set_statement(database, statement, sql, sql_length,
                                                            out_stmt);
        case MYLITE_SQL_AST_UPDATE_STATEMENT:
            return mylite_dml_prepare_update_statement(database, statement, sql, sql_length,
                                                       out_stmt);
        case MYLITE_SQL_AST_DELETE_STATEMENT:
            return mylite_dml_prepare_delete_statement(database, statement, sql, sql_length,
                                                       out_stmt);
        case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
        case MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT:
        case MYLITE_SQL_AST_COMMIT_STATEMENT:
        case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
        case MYLITE_SQL_AST_SAVEPOINT_STATEMENT:
        case MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT:
        case MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT:
            return mylite_statement_prepare_transaction_statement(database, statement, out_stmt,
                                                                  callbacks);
        case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
            return mylite_show_prepare_schemas_statement(database, out_stmt);
        case MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT:
            return mylite_show_prepare_variables_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_STATUS_STATEMENT:
            return mylite_show_prepare_status_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
            return mylite_show_prepare_engines_statement(database, out_stmt);
        case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
            return mylite_show_prepare_character_set_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
            return mylite_show_prepare_collation_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
            return mylite_show_prepare_tables_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
            return mylite_show_prepare_table_status_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
            return mylite_show_prepare_columns_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
            return mylite_show_prepare_index_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
            return mylite_show_prepare_create_table_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_CREATE_SCHEMA_STATEMENT:
            return mylite_show_prepare_create_schema_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_STATEMENT:
            return mylite_show_prepare_diagnostics_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_COUNT_STATEMENT:
            return mylite_show_prepare_diagnostics_count_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT:
            return mylite_show_prepare_describe_table_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_QUERY_EXPRESSION:
            return mylite_select_union_prepare_query_expression(
                database, statement, sql, sql_length, out_stmt, callbacks->union_callbacks);
        case MYLITE_SQL_AST_VALUES_STATEMENT:
            return mylite_values_query_prepare_statement(database, statement, sql, sql_length,
                                                         out_stmt, callbacks->scalar_callbacks,
                                                         callbacks->union_callbacks);
        case MYLITE_SQL_AST_SELECT_STATEMENT:
            status = mylite_select_prepare_statement(database, statement, sql, sql_length, out_stmt,
                                                     callbacks->select_callbacks);
            if (status != MYLITE_UNSUPPORTED || database->error_message != NULL) {
                return status;
            }
            break;
        case MYLITE_SQL_AST_SCRIPT:
        case MYLITE_SQL_AST_SELECT_LIST:
        case MYLITE_SQL_AST_SELECT_ITEM:
        case MYLITE_SQL_AST_FROM_DUAL:
        case MYLITE_SQL_AST_FROM_TABLE:
        case MYLITE_SQL_AST_FROM_TABLE_REFERENCES:
        case MYLITE_SQL_AST_TABLE_REFERENCE_LIST:
        case MYLITE_SQL_AST_JOIN_EXPRESSION:
        case MYLITE_SQL_AST_JOIN_CONDITION:
        case MYLITE_SQL_AST_USING_COLUMN_LIST:
        case MYLITE_SQL_AST_USING_COLUMN:
        case MYLITE_SQL_AST_IDENTIFIER:
        case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        case MYLITE_SQL_AST_WILDCARD:
        case MYLITE_SQL_AST_LITERAL:
        case MYLITE_SQL_AST_UNARY_EXPRESSION:
        case MYLITE_SQL_AST_BINARY_EXPRESSION:
        case MYLITE_SQL_AST_TERNARY_EXPRESSION:
        case MYLITE_SQL_AST_EXPRESSION_LIST:
        case MYLITE_SQL_AST_FUNCTION_CALL:
        case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
        case MYLITE_SQL_AST_CASE_EXPRESSION:
        case MYLITE_SQL_AST_CASE_WHEN_LIST:
        case MYLITE_SQL_AST_CASE_WHEN:
        case MYLITE_SQL_AST_CAST_EXPRESSION:
        case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
        case MYLITE_SQL_AST_GROUP_ITEM_LIST:
        case MYLITE_SQL_AST_GROUP_ITEM:
        case MYLITE_SQL_AST_HAVING_CLAUSE:
        case MYLITE_SQL_AST_AGGREGATE_CALL:
        case MYLITE_SQL_AST_WHERE_CLAUSE:
        case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
        case MYLITE_SQL_AST_ORDER_ITEM_LIST:
        case MYLITE_SQL_AST_ORDER_ITEM:
        case MYLITE_SQL_AST_LIMIT_CLAUSE:
        case MYLITE_SQL_AST_LIMIT_BOUND:
        case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
        case MYLITE_SQL_AST_IF_EXISTS:
        case MYLITE_SQL_AST_IF_NOT_EXISTS:
        case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
        case MYLITE_SQL_AST_SCHEMA_OPTION:
        case MYLITE_SQL_AST_DEFAULT:
        case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
        case MYLITE_SQL_AST_COLUMN_DEFINITION:
        case MYLITE_SQL_AST_COLUMN_TYPE:
        case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
        case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
        case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
        case MYLITE_SQL_AST_KEY_PART_LIST:
        case MYLITE_SQL_AST_KEY_PART:
        case MYLITE_SQL_AST_INDEX_TYPE:
        case MYLITE_SQL_AST_INDEX_OPTION_LIST:
        case MYLITE_SQL_AST_INDEX_OPTION:
        case MYLITE_SQL_AST_SECONDARY_INDEX:
        case MYLITE_SQL_AST_UNIQUE_INDEX:
        case MYLITE_SQL_AST_TABLE_OPTION_LIST:
        case MYLITE_SQL_AST_TABLE_OPTION:
        case MYLITE_SQL_AST_TABLE_NAME_LIST:
        case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
        case MYLITE_SQL_AST_INSERT_ROW:
        case MYLITE_SQL_AST_INSERT_ROW_LIST:
        case MYLITE_SQL_AST_INSERT_VALUE_LIST:
        case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
        case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
        case MYLITE_SQL_AST_UPDATE_TARGET:
        case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
        case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
        case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
        case MYLITE_SQL_AST_DELETE_TARGET:
        case MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE:
        case MYLITE_SQL_AST_DELETE_TARGET_LIST:
        case MYLITE_SQL_AST_DELETE_TARGET_NAME:
        case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST:
        case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC:
        case MYLITE_SQL_AST_TRANSACTION_COMPLETION:
        case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
        case MYLITE_SQL_AST_EXISTS_EXPRESSION:
        case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
        case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
        case MYLITE_SQL_AST_UNION_EXPRESSION:
        case MYLITE_SQL_AST_QUERY_PRIMARY:
        case MYLITE_SQL_AST_WINDOW_FUNCTION_CALL:
        case MYLITE_SQL_AST_OVER_CLAUSE:
        case MYLITE_SQL_AST_WINDOW_SPECIFICATION:
        case MYLITE_SQL_AST_WINDOW_CLAUSE:
        case MYLITE_SQL_AST_WINDOW_DEFINITION_LIST:
        case MYLITE_SQL_AST_WINDOW_DEFINITION:
        case MYLITE_SQL_AST_WINDOW_PARTITION_CLAUSE:
        case MYLITE_SQL_AST_WINDOW_FRAME_CLAUSE:
        case MYLITE_SQL_AST_WINDOW_FRAME_BOUND:
        case MYLITE_SQL_AST_WINDOW_NULL_TREATMENT:
        case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
        case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
        case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
        case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
        case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
        case MYLITE_SQL_AST_DDL_TABLE_OPTION_LIST:
        case MYLITE_SQL_AST_DDL_TABLE_OPTION:
        case MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST:
        case MYLITE_SQL_AST_ALTER_TABLE_ACTION:
        case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION:
        case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
        case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
        case MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT_LIST:
        case MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT:
        case MYLITE_SQL_AST_EXECUTE_USING_LIST:
            break;
        }
    }

    translate_status = mylite_sqlite_translate(root, &translate_result);
    if (translate_status != MYLITE_SQLITE_TRANSLATE_OK) {
        return mylite_statement_map_translate_status(database, translate_status);
    }

    status = mylite_statement_prepare_sqlite(database, translate_result.sql, out_stmt);
    mylite_sqlite_translate_result_deinit(&translate_result);
    return status;
}

static bool placeholder_statement_is_table_maintenance(const struct mylite_sql_ast_node *statement)
{
    switch (statement->placeholder_statement_kind) {
    case MYLITE_SQL_AST_PLACEHOLDER_CHECK_TABLE:
    case MYLITE_SQL_AST_PLACEHOLDER_OPTIMIZE_TABLE:
    case MYLITE_SQL_AST_PLACEHOLDER_REPAIR_TABLE:
        return true;
    default:
        return false;
    }
}

static enum mylite_stmt_kind placeholder_statement_kind(const struct mylite_sql_ast_node *statement)
{
    switch (statement->placeholder_statement_kind) {
    case MYLITE_SQL_AST_PLACEHOLDER_CALL:
        return MYLITE_STMT_CALL_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_CREATE_PROCEDURE:
        return MYLITE_STMT_CREATE_PROCEDURE_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_CREATE_FUNCTION:
        return MYLITE_STMT_CREATE_FUNCTION_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_CREATE_TRIGGER:
        return MYLITE_STMT_CREATE_TRIGGER_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_CREATE_EVENT:
        return MYLITE_STMT_CREATE_EVENT_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_DROP_PROCEDURE:
        return MYLITE_STMT_DROP_PROCEDURE_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_DROP_FUNCTION:
        return MYLITE_STMT_DROP_FUNCTION_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_DROP_TRIGGER:
        return MYLITE_STMT_DROP_TRIGGER_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_DROP_EVENT:
        return MYLITE_STMT_DROP_EVENT_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_SIGNAL:
        return MYLITE_STMT_SIGNAL_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_EXPLAIN:
        return MYLITE_STMT_EXPLAIN_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_ALTER_USER:
        return MYLITE_STMT_ALTER_USER_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_CREATE_USER:
        return MYLITE_STMT_CREATE_USER_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_CREATE_ROLE:
        return MYLITE_STMT_CREATE_ROLE_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_DROP_USER:
        return MYLITE_STMT_DROP_USER_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_DROP_ROLE:
        return MYLITE_STMT_DROP_ROLE_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_GRANT:
        return MYLITE_STMT_GRANT_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_RENAME_USER:
        return MYLITE_STMT_RENAME_USER_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_REVOKE:
        return MYLITE_STMT_REVOKE_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_SET_DEFAULT_ROLE:
        return MYLITE_STMT_SET_DEFAULT_ROLE_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_SET_PASSWORD:
        return MYLITE_STMT_SET_PASSWORD_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_SET_ROLE:
        return MYLITE_STMT_SET_ROLE_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_SHOW_GRANTS:
        return MYLITE_STMT_SHOW_GRANTS_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_SHOW_PRIVILEGES:
        return MYLITE_STMT_SHOW_PRIVILEGES_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING:
        return MYLITE_STMT_TABLE_PARTITIONING_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_CTE:
        return MYLITE_STMT_CTE_PLACEHOLDER;
    case MYLITE_SQL_AST_PLACEHOLDER_CHECK_TABLE:
    case MYLITE_SQL_AST_PLACEHOLDER_OPTIMIZE_TABLE:
    case MYLITE_SQL_AST_PLACEHOLDER_REPAIR_TABLE:
        break;
    }

    return MYLITE_STMT_SQLITE;
}
