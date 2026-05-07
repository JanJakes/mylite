#include "mylite_statement_execute.h"

#include "mylite_connection.h"
#include "mylite_connection_statement.h"
#include "mylite_diagnostics.h"
#include "mylite_dml_statement.h"
#include "mylite_error_codes.h"
#include "mylite_prepared_statements.h"
#include "mylite_runtime.h"
#include "mylite_schema.h"
#include "mylite_select_context.h"
#include "mylite_select_union.h"
#include "mylite_statement.h"
#include "mylite_table_ddl.h"
#include "mylite_table_ddl_statement.h"
#include "mylite_transactions.h"
#include "mylite_user_variables.h"
#include "mylite_values_query.h"

static int execute_parser_placeholder_statement(mylite_stmt *stmt);

static const char *parser_placeholder_warning_message(enum mylite_stmt_kind kind);

int mylite_statement_execute_custom(mylite_stmt *stmt) {
    return mylite_statement_execute_custom_with_callbacks(
        stmt,
        mylite_select_context_statement_execute_callbacks()
    );
}

int mylite_statement_execute_custom_with_callbacks(
    mylite_stmt *stmt,
    const struct mylite_statement_execute_callbacks *callbacks
) {
    if (stmt == NULL || callbacks == NULL || callbacks->execute_scalar_select == NULL ||
        callbacks->execute_table_select == NULL || callbacks->union_callbacks == NULL ||
        callbacks->scalar_callbacks == NULL ||
        callbacks->eval_dml_materialize_session_function == NULL ||
        callbacks->eval_dml_materialize_subquery == NULL ||
        callbacks->set_dml_materialize_where_predicate_eval_error == NULL) {
        return MYLITE_MISUSE;
    }

    const struct mylite_dml_expression_callbacks dml_expression_callbacks = {
        .user_data = stmt,
        .eval_session_function = callbacks->eval_dml_materialize_session_function,
        .eval_subquery = callbacks->eval_dml_materialize_subquery,
        .set_where_predicate_eval_error = callbacks->set_dml_materialize_where_predicate_eval_error,
    };
    int status = MYLITE_OK;

    if (stmt->kind == MYLITE_STMT_SCALAR_SELECT) {
        return callbacks->execute_scalar_select(stmt);
    }
    if (stmt->kind == MYLITE_STMT_TABLE_SELECT) {
        return callbacks->execute_table_select(stmt);
    }
    if (stmt->kind == MYLITE_STMT_UNION_QUERY) {
        return mylite_select_union_execute_query(stmt, callbacks->union_callbacks);
    }
    if (stmt->kind == MYLITE_STMT_VALUES_QUERY) {
        return mylite_values_query_execute_statement(
            stmt,
            callbacks->scalar_callbacks,
            callbacks->union_callbacks
        );
    }
    if (stmt->kind == MYLITE_STMT_EXECUTE_PREPARED) {
        return mylite_prepared_statement_execute_execute(stmt);
    }
    if (stmt->executed) {
        return MYLITE_DONE;
    }
    if (stmt->kind == MYLITE_STMT_REPLACE_VALUES || stmt->kind == MYLITE_STMT_REPLACE_SET) {
        status = mylite_dml_append_replace_delayed_warning(stmt);
        if (status != MYLITE_OK) {
            stmt->affected_rows = -1;
            return status;
        }
    }
    if (mylite_statement_kind_writes(stmt->kind) && stmt->database->transaction_active &&
        stmt->database->transaction_access_mode == MYLITE_TRANSACTION_ACCESS_READ_ONLY) {
        stmt->affected_rows = -1;
        return mylite_transaction_set_read_only_error(stmt->database);
    }
    stmt->executed = true;

    switch (stmt->kind) {
    case MYLITE_STMT_CREATE_SCHEMA:
        status = mylite_schema_execute_create_statement(stmt);
        break;
    case MYLITE_STMT_ALTER_SCHEMA:
        status = mylite_schema_execute_alter_statement(stmt);
        break;
    case MYLITE_STMT_DROP_SCHEMA:
        status = mylite_schema_execute_drop_statement(stmt);
        break;
    case MYLITE_STMT_USE_SCHEMA:
        status = mylite_schema_execute_use_statement(stmt);
        break;
    case MYLITE_STMT_SET_NAMES:
        status = mylite_connection_execute_set_names_statement(stmt);
        break;
    case MYLITE_STMT_SET_CHARACTER_SET:
        status = mylite_connection_execute_set_character_set_statement(stmt);
        break;
    case MYLITE_STMT_SET_SYSTEM_VARIABLE:
        status = mylite_connection_execute_set_system_variable_statement(stmt);
        break;
    case MYLITE_STMT_SET_USER_VARIABLE:
        status = mylite_user_variable_execute_set_statement(stmt);
        break;
    case MYLITE_STMT_PREPARE_STATEMENT:
        status = mylite_prepared_statement_execute_prepare(stmt);
        break;
    case MYLITE_STMT_DEALLOCATE_PREPARE:
        status = mylite_prepared_statement_execute_deallocate(stmt);
        break;
    case MYLITE_STMT_CREATE_TABLE:
        status = mylite_table_ddl_execute_create_table_statement(
            stmt->database,
            stmt->database->selected_schema,
            &stmt->create_table,
            stmt->if_not_exists
        );
        if (status != MYLITE_OK) {
            stmt->affected_rows = -1;
        } else if (stmt->create_table.select) {
            stmt->affected_rows = stmt->create_table.selected_row_count;
        }
        break;
    case MYLITE_STMT_DROP_TABLE:
        status = mylite_table_ddl_execute_drop_table_statement(
            stmt->database,
            stmt->database->selected_schema,
            &stmt->drop_table,
            stmt->if_exists
        );
        break;
    case MYLITE_STMT_RENAME_TABLE:
        status = mylite_table_ddl_execute_rename_table_prepared_statement(stmt);
        break;
    case MYLITE_STMT_TRUNCATE_TABLE:
        status = mylite_table_ddl_execute_truncate_table_prepared_statement(stmt);
        break;
    case MYLITE_STMT_ALTER_TABLE:
        status = mylite_table_ddl_execute_alter_table_prepared_statement(stmt);
        break;
    case MYLITE_STMT_CREATE_INDEX:
        status = mylite_table_ddl_execute_create_index_prepared_statement(stmt);
        break;
    case MYLITE_STMT_DROP_INDEX:
        status = mylite_table_ddl_execute_drop_index_prepared_statement(stmt);
        break;
    case MYLITE_STMT_INSERT_VALUES:
        status = mylite_dml_execute_insert_values_statement(stmt, &dml_expression_callbacks);
        break;
    case MYLITE_STMT_INSERT_SET:
        status = mylite_dml_execute_insert_set_statement(stmt, &dml_expression_callbacks);
        break;
    case MYLITE_STMT_REPLACE_VALUES:
        status = mylite_dml_execute_replace_values_statement(stmt, &dml_expression_callbacks);
        break;
    case MYLITE_STMT_REPLACE_SET:
        status = mylite_dml_execute_replace_set_statement(stmt, &dml_expression_callbacks);
        break;
    case MYLITE_STMT_UPDATE:
        status = mylite_dml_execute_update_statement(stmt, &dml_expression_callbacks);
        break;
    case MYLITE_STMT_DELETE:
        status = mylite_dml_execute_delete_statement(stmt, &dml_expression_callbacks);
        break;
    case MYLITE_STMT_START_TRANSACTION:
    case MYLITE_STMT_BEGIN_TRANSACTION:
    case MYLITE_STMT_COMMIT:
    case MYLITE_STMT_ROLLBACK:
    case MYLITE_STMT_SAVEPOINT:
    case MYLITE_STMT_ROLLBACK_TO_SAVEPOINT:
    case MYLITE_STMT_RELEASE_SAVEPOINT:
        status = mylite_transaction_execute_statement(stmt);
        break;
    case MYLITE_STMT_CALL_PLACEHOLDER:
    case MYLITE_STMT_CREATE_PROCEDURE_PLACEHOLDER:
    case MYLITE_STMT_CREATE_FUNCTION_PLACEHOLDER:
    case MYLITE_STMT_CREATE_TRIGGER_PLACEHOLDER:
    case MYLITE_STMT_CREATE_EVENT_PLACEHOLDER:
    case MYLITE_STMT_DROP_PROCEDURE_PLACEHOLDER:
    case MYLITE_STMT_DROP_FUNCTION_PLACEHOLDER:
    case MYLITE_STMT_DROP_TRIGGER_PLACEHOLDER:
    case MYLITE_STMT_DROP_EVENT_PLACEHOLDER:
    case MYLITE_STMT_SIGNAL_PLACEHOLDER:
    case MYLITE_STMT_EXPLAIN_PLACEHOLDER:
    case MYLITE_STMT_ALTER_USER_PLACEHOLDER:
    case MYLITE_STMT_CREATE_USER_PLACEHOLDER:
    case MYLITE_STMT_CREATE_ROLE_PLACEHOLDER:
    case MYLITE_STMT_DROP_USER_PLACEHOLDER:
    case MYLITE_STMT_DROP_ROLE_PLACEHOLDER:
    case MYLITE_STMT_GRANT_PLACEHOLDER:
    case MYLITE_STMT_RENAME_USER_PLACEHOLDER:
    case MYLITE_STMT_REVOKE_PLACEHOLDER:
    case MYLITE_STMT_SET_DEFAULT_ROLE_PLACEHOLDER:
    case MYLITE_STMT_SET_PASSWORD_PLACEHOLDER:
    case MYLITE_STMT_SET_ROLE_PLACEHOLDER:
    case MYLITE_STMT_SHOW_GRANTS_PLACEHOLDER:
    case MYLITE_STMT_SHOW_PRIVILEGES_PLACEHOLDER:
    case MYLITE_STMT_TABLE_PARTITIONING_PLACEHOLDER:
    case MYLITE_STMT_CTE_PLACEHOLDER:
    case MYLITE_STMT_LOCK_TABLES_PLACEHOLDER:
    case MYLITE_STMT_UNLOCK_TABLES_PLACEHOLDER:
        status = execute_parser_placeholder_statement(stmt);
        break;
    case MYLITE_STMT_SCALAR_SELECT:
        return callbacks->execute_scalar_select(stmt);
    case MYLITE_STMT_TABLE_SELECT:
        return callbacks->execute_table_select(stmt);
    case MYLITE_STMT_UNION_QUERY:
        return mylite_select_union_execute_query(stmt, callbacks->union_callbacks);
    case MYLITE_STMT_VALUES_QUERY:
        return mylite_values_query_execute_statement(
            stmt,
            callbacks->scalar_callbacks,
            callbacks->union_callbacks
        );
    case MYLITE_STMT_EXECUTE_PREPARED:
        return mylite_prepared_statement_execute_execute(stmt);
    case MYLITE_STMT_SQLITE:
        status = MYLITE_MISUSE;
        break;
    }

    return status == MYLITE_OK ? MYLITE_DONE : status;
}

static int execute_parser_placeholder_statement(mylite_stmt *stmt) {
    return mylite_diagnostics_append_warning(
        stmt->database,
        MYLITE_MYSQL_ER_NOT_SUPPORTED_YET,
        parser_placeholder_warning_message(stmt->kind)
    );
}

static const char *parser_placeholder_warning_message(enum mylite_stmt_kind kind) {
    switch (kind) {
    case MYLITE_STMT_CALL_PLACEHOLDER:
        return "CALL statement is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_CREATE_PROCEDURE_PLACEHOLDER:
        return "CREATE PROCEDURE is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_CREATE_FUNCTION_PLACEHOLDER:
        return "CREATE FUNCTION is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_CREATE_TRIGGER_PLACEHOLDER:
        return "CREATE TRIGGER is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_CREATE_EVENT_PLACEHOLDER:
        return "CREATE EVENT is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_DROP_PROCEDURE_PLACEHOLDER:
        return "DROP PROCEDURE is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_DROP_FUNCTION_PLACEHOLDER:
        return "DROP FUNCTION is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_DROP_TRIGGER_PLACEHOLDER:
        return "DROP TRIGGER is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_DROP_EVENT_PLACEHOLDER:
        return "DROP EVENT is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_SIGNAL_PLACEHOLDER:
        return "SIGNAL statement is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_EXPLAIN_PLACEHOLDER:
        return "EXPLAIN statement is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_ALTER_USER_PLACEHOLDER:
        return "ALTER USER is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_CREATE_USER_PLACEHOLDER:
        return "CREATE USER is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_CREATE_ROLE_PLACEHOLDER:
        return "CREATE ROLE is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_DROP_USER_PLACEHOLDER:
        return "DROP USER is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_DROP_ROLE_PLACEHOLDER:
        return "DROP ROLE is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_GRANT_PLACEHOLDER:
        return "GRANT statement is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_RENAME_USER_PLACEHOLDER:
        return "RENAME USER is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_REVOKE_PLACEHOLDER:
        return "REVOKE statement is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_SET_DEFAULT_ROLE_PLACEHOLDER:
        return "SET DEFAULT ROLE is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_SET_PASSWORD_PLACEHOLDER:
        return "SET PASSWORD is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_SET_ROLE_PLACEHOLDER:
        return "SET ROLE is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_SHOW_GRANTS_PLACEHOLDER:
        return "SHOW GRANTS is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_SHOW_PRIVILEGES_PLACEHOLDER:
        return "SHOW PRIVILEGES is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_TABLE_PARTITIONING_PLACEHOLDER:
        return "table partitioning syntax is accepted as a MyLite parser placeholder and is not "
               "executed";
    case MYLITE_STMT_CTE_PLACEHOLDER:
        return "CTE query expression is accepted as a MyLite parser placeholder and is not "
               "executed";
    case MYLITE_STMT_LOCK_TABLES_PLACEHOLDER:
        return "LOCK TABLES is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_UNLOCK_TABLES_PLACEHOLDER:
        return "UNLOCK TABLES is accepted as a MyLite parser placeholder and is not executed";
    case MYLITE_STMT_SQLITE:
    case MYLITE_STMT_CREATE_SCHEMA:
    case MYLITE_STMT_ALTER_SCHEMA:
    case MYLITE_STMT_DROP_SCHEMA:
    case MYLITE_STMT_USE_SCHEMA:
    case MYLITE_STMT_SET_NAMES:
    case MYLITE_STMT_SET_CHARACTER_SET:
    case MYLITE_STMT_CREATE_TABLE:
    case MYLITE_STMT_DROP_TABLE:
    case MYLITE_STMT_INSERT_VALUES:
    case MYLITE_STMT_INSERT_SET:
    case MYLITE_STMT_REPLACE_VALUES:
    case MYLITE_STMT_REPLACE_SET:
    case MYLITE_STMT_SCALAR_SELECT:
    case MYLITE_STMT_TABLE_SELECT:
    case MYLITE_STMT_UNION_QUERY:
    case MYLITE_STMT_VALUES_QUERY:
    case MYLITE_STMT_UPDATE:
    case MYLITE_STMT_DELETE:
    case MYLITE_STMT_START_TRANSACTION:
    case MYLITE_STMT_BEGIN_TRANSACTION:
    case MYLITE_STMT_COMMIT:
    case MYLITE_STMT_ROLLBACK:
    case MYLITE_STMT_SAVEPOINT:
    case MYLITE_STMT_ROLLBACK_TO_SAVEPOINT:
    case MYLITE_STMT_RELEASE_SAVEPOINT:
    case MYLITE_STMT_CREATE_INDEX:
    case MYLITE_STMT_DROP_INDEX:
    case MYLITE_STMT_ALTER_TABLE:
    case MYLITE_STMT_RENAME_TABLE:
    case MYLITE_STMT_TRUNCATE_TABLE:
    case MYLITE_STMT_SET_SYSTEM_VARIABLE:
    case MYLITE_STMT_SET_USER_VARIABLE:
    case MYLITE_STMT_PREPARE_STATEMENT:
    case MYLITE_STMT_EXECUTE_PREPARED:
    case MYLITE_STMT_DEALLOCATE_PREPARE:
        break;
    }

    return "statement is accepted as a MyLite parser placeholder and is not executed";
}
