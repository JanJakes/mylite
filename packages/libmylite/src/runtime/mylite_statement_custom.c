#include "mylite_statement_custom.h"

#include "mylite_diagnostics.h"
#include "mylite_dml_insert_copy.h"
#include "mylite_runtime.h"
#include "mylite_schema.h"
#include "mylite_select_scalar.h"
#include "mylite_span.h"
#include "mylite_table_ddl.h"
#include "mylite_transactions.h"

#include <stdlib.h>

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_statement_prepare_custom(
    mylite_db *database, enum mylite_stmt_kind kind, const struct mylite_sql_ast_node *statement,
    const struct mylite_select_scalar_eval_callbacks *scalar_select_callbacks,
    mylite_stmt **out_stmt)
{
    mylite_stmt *stmt = calloc(1U, sizeof(*stmt));
    int status = MYLITE_OK;

    if (stmt == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    *stmt = (mylite_stmt){
        .database = database,
        .kind = kind,
        .affected_rows = 0,
    };

    switch (kind) {
    case MYLITE_STMT_CREATE_SCHEMA:
    case MYLITE_STMT_ALTER_SCHEMA:
    case MYLITE_STMT_DROP_SCHEMA:
    case MYLITE_STMT_USE_SCHEMA:
        status = mylite_schema_copy_statement_name(statement, &stmt->schema_name);
        if (status == MYLITE_OK) {
            status = mylite_schema_copy_options(statement, &stmt->options);
        }
        break;
    case MYLITE_STMT_SET_NAMES:
    case MYLITE_STMT_SET_CHARACTER_SET:
    case MYLITE_STMT_SET_SYSTEM_VARIABLE:
    case MYLITE_STMT_SET_USER_VARIABLE:
    case MYLITE_STMT_PREPARE_STATEMENT:
    case MYLITE_STMT_EXECUTE_PREPARED:
    case MYLITE_STMT_DEALLOCATE_PREPARE:
        status = MYLITE_UNSUPPORTED;
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
        break;
    case MYLITE_STMT_CREATE_TABLE:
        status = mylite_table_ddl_copy_create_table_statement(statement, &stmt->create_table);
        break;
    case MYLITE_STMT_DROP_TABLE:
        status = mylite_table_ddl_copy_drop_table_statement(statement, &stmt->drop_table);
        break;
    case MYLITE_STMT_RENAME_TABLE:
        status = mylite_table_ddl_copy_rename_table_statement(statement, &stmt->rename_table);
        break;
    case MYLITE_STMT_TRUNCATE_TABLE:
        status = mylite_table_ddl_copy_truncate_table_statement(statement, &stmt->truncate_table);
        break;
    case MYLITE_STMT_ALTER_TABLE:
        status = mylite_table_ddl_copy_alter_table_statement(statement, &stmt->alter_table);
        break;
    case MYLITE_STMT_CREATE_INDEX:
        status = mylite_table_ddl_copy_create_index_statement(statement, &stmt->index_ddl);
        break;
    case MYLITE_STMT_DROP_INDEX:
        status = mylite_table_ddl_copy_drop_index_statement(statement, &stmt->index_ddl);
        break;
    case MYLITE_STMT_INSERT_VALUES:
        status = mylite_dml_copy_insert_values_statement(statement, &stmt->insert_values,
                                                         &stmt->insert_update);
        break;
    case MYLITE_STMT_INSERT_SET:
        status = mylite_dml_copy_insert_set_statement(statement, &stmt->insert_values,
                                                      &stmt->insert_set, &stmt->insert_update);
        break;
    case MYLITE_STMT_REPLACE_VALUES:
        status = mylite_dml_copy_replace_values_statement(statement, &stmt->insert_values);
        break;
    case MYLITE_STMT_REPLACE_SET:
        status = mylite_dml_copy_replace_set_statement(statement, &stmt->insert_values,
                                                       &stmt->insert_set);
        break;
    case MYLITE_STMT_UPDATE:
    case MYLITE_STMT_DELETE:
        status = MYLITE_UNSUPPORTED;
        break;
    case MYLITE_STMT_START_TRANSACTION:
    case MYLITE_STMT_BEGIN_TRANSACTION:
    case MYLITE_STMT_COMMIT:
    case MYLITE_STMT_ROLLBACK:
        status = mylite_transaction_copy_statement(statement, stmt);
        break;
    case MYLITE_STMT_SAVEPOINT:
    case MYLITE_STMT_ROLLBACK_TO_SAVEPOINT:
    case MYLITE_STMT_RELEASE_SAVEPOINT:
        status = mylite_transaction_copy_savepoint_statement(statement, stmt);
        break;
    case MYLITE_STMT_SCALAR_SELECT:
        status = mylite_select_scalar_copy_statement(statement, stmt, scalar_select_callbacks);
        break;
    case MYLITE_STMT_TABLE_SELECT:
    case MYLITE_STMT_UNION_QUERY:
    case MYLITE_STMT_VALUES_QUERY:
    case MYLITE_STMT_SQLITE:
        status = MYLITE_UNSUPPORTED;
        break;
    }
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        mylite_finalize(stmt);
        return status;
    }

    if (kind == MYLITE_STMT_CREATE_SCHEMA &&
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_IF_NOT_EXISTS) != NULL) {
        stmt->if_not_exists = true;
    }
    if (kind == MYLITE_STMT_CREATE_TABLE &&
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_IF_NOT_EXISTS) != NULL) {
        stmt->if_not_exists = true;
    }
    if (kind == MYLITE_STMT_DROP_SCHEMA &&
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_IF_EXISTS) != NULL) {
        stmt->if_exists = true;
    }
    if (kind == MYLITE_STMT_DROP_TABLE &&
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_IF_EXISTS) != NULL) {
        stmt->if_exists = true;
    }

    *out_stmt = stmt;
    return MYLITE_OK;
}
