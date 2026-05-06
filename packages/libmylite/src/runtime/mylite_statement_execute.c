#include "mylite_statement_execute.h"

#include "mylite_connection.h"
#include "mylite_connection_statement.h"
#include "mylite_dml_statement.h"
#include "mylite_runtime.h"
#include "mylite_schema.h"
#include "mylite_select_context.h"
#include "mylite_select_union.h"
#include "mylite_statement.h"
#include "mylite_table_ddl.h"
#include "mylite_table_ddl_statement.h"
#include "mylite_transactions.h"

int mylite_statement_execute_custom(mylite_stmt *stmt)
{
    return mylite_statement_execute_custom_with_callbacks(
        stmt, mylite_select_context_statement_execute_callbacks());
}

int mylite_statement_execute_custom_with_callbacks(
    mylite_stmt *stmt, const struct mylite_statement_execute_callbacks *callbacks)
{
    if (stmt == NULL || callbacks == NULL || callbacks->execute_scalar_select == NULL ||
        callbacks->execute_table_select == NULL || callbacks->union_callbacks == NULL ||
        callbacks->eval_dml_materialize_session_function == NULL ||
        callbacks->set_dml_materialize_where_predicate_eval_error == NULL) {
        return MYLITE_MISUSE;
    }

    const struct mylite_dml_expression_callbacks dml_expression_callbacks = {
        .user_data = stmt,
        .eval_session_function = callbacks->eval_dml_materialize_session_function,
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
    case MYLITE_STMT_CREATE_TABLE:
        status = mylite_table_ddl_execute_create_table_statement(
            stmt->database, stmt->database->selected_schema, &stmt->create_table,
            stmt->if_not_exists);
        break;
    case MYLITE_STMT_DROP_TABLE:
        status = mylite_table_ddl_execute_drop_table_statement(
            stmt->database, stmt->database->selected_schema, &stmt->drop_table, stmt->if_exists);
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
    case MYLITE_STMT_SCALAR_SELECT:
        return callbacks->execute_scalar_select(stmt);
    case MYLITE_STMT_TABLE_SELECT:
        return callbacks->execute_table_select(stmt);
    case MYLITE_STMT_UNION_QUERY:
        return mylite_select_union_execute_query(stmt, callbacks->union_callbacks);
    case MYLITE_STMT_SQLITE:
        status = MYLITE_MISUSE;
        break;
    }

    return status == MYLITE_OK ? MYLITE_DONE : status;
}
