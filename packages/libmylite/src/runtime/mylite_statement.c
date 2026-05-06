#include "mylite_statement.h"

#include "mylite_connection.h"
#include "mylite_connection_statement.h"
#include "mylite_diagnostics.h"
#include "mylite_dml.h"
#include "mylite_error_codes.h"
#include "mylite_metadata.h"
#include "mylite_prepared_statements.h"
#include "mylite_schema.h"
#include "mylite_select.h"
#include "mylite_select_rowset.h"
#include "mylite_statement_prepare.h"
#include "mylite_table_ddl.h"
#include "mylite_transactions.h"
#include "mylite_user_variables.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

int64_t mylite_affected_rows(const mylite_stmt *stmt)
{
    if (stmt == NULL) {
        return -1;
    }
    if (stmt->kind == MYLITE_STMT_EXECUTE_PREPARED && stmt->prepared_execute_stmt != NULL) {
        return mylite_affected_rows(stmt->prepared_execute_stmt);
    }

    return stmt->affected_rows;
}

int mylite_step(mylite_stmt *stmt)
{
    int rc = SQLITE_OK;

    if (stmt == NULL) {
        return MYLITE_MISUSE;
    }

    mylite_diagnostics_clear_error_message(stmt->database);
    if (stmt->database->transaction_released) {
        return mylite_connection_set_released_error(stmt->database);
    }
    if (!stmt->executed && !stmt->preserve_prepare_warnings) {
        mylite_diagnostics_clear_warnings(stmt->database);
    }
    if (stmt->kind != MYLITE_STMT_SQLITE) {
        int status = mylite_statement_execute_custom(stmt);

        if (status == MYLITE_DONE) {
            mylite_statement_record_row_count(stmt);
        }
        if (status != MYLITE_ROW && status != MYLITE_DONE && status != MYLITE_OK &&
            status != MYLITE_NOMEM) {
            (void)mylite_diagnostics_ensure_current_error_condition(stmt->database,
                                                                    MYLITE_MYSQL_ER_UNKNOWN_ERROR);
        }
        return status;
    }

    rc = sqlite3_step(stmt->sqlite_stmt);
    if (rc == SQLITE_ROW) {
        if (sqlite3_stmt_readonly(stmt->sqlite_stmt) && stmt->found_rows != UINT64_MAX) {
            ++stmt->found_rows;
        }
        return MYLITE_ROW;
    }
    if (rc == SQLITE_DONE) {
        bool readonly = sqlite3_stmt_readonly(stmt->sqlite_stmt) != 0;

        if (readonly) {
            stmt->affected_rows = -1;
        } else {
            stmt->affected_rows = sqlite3_changes(stmt->database->sqlite);
        }
        if (readonly && !stmt->previous_found_rows_recorded) {
            stmt->database->previous_found_rows = stmt->found_rows;
            stmt->previous_found_rows_recorded = true;
        }
        mylite_statement_record_row_count(stmt);
        return MYLITE_DONE;
    }

    rc = mylite_diagnostics_set_sqlite_error(stmt->database);
    if (rc != MYLITE_NOMEM) {
        (void)mylite_diagnostics_ensure_current_error_condition(stmt->database,
                                                                MYLITE_MYSQL_ER_UNKNOWN_ERROR);
    }
    return rc;
}

int mylite_statement_prepare_sqlite(mylite_db *database, const char *sqlite_sql,
                                    mylite_stmt **out_stmt)
{
    sqlite3_stmt *sqlite_stmt = NULL;
    mylite_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v3(database->sqlite, sqlite_sql, -1, SQLITE_PREPARE_PERSISTENT,
                                &sqlite_stmt, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    stmt = malloc(sizeof(*stmt));
    if (stmt == NULL) {
        sqlite3_finalize(sqlite_stmt);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    *stmt = (mylite_stmt){
        .database = database,
        .kind = MYLITE_STMT_SQLITE,
        .sqlite_stmt = sqlite_stmt,
        .affected_rows = -1,
    };
    *out_stmt = stmt;
    return MYLITE_OK;
}

int mylite_statement_map_parse_status(mylite_db *database, enum mylite_sql_parse_status status)
{
    switch (status) {
    case MYLITE_SQL_PARSE_OK:
        return MYLITE_OK;
    case MYLITE_SQL_PARSE_MISUSE:
        return MYLITE_MISUSE;
    case MYLITE_SQL_PARSE_NOMEM:
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    case MYLITE_SQL_PARSE_LEXER_ERROR:
    case MYLITE_SQL_PARSE_SYNTAX_ERROR:
    case MYLITE_SQL_PARSE_STACK_OVERFLOW:
        if (mylite_diagnostics_set_error_message(database, mylite_sql_parse_status_name(status)) ==
            MYLITE_NOMEM) {
            return MYLITE_NOMEM;
        }
        return MYLITE_PARSE_ERROR;
    }

    return MYLITE_PARSE_ERROR;
}

int mylite_statement_map_translate_status(mylite_db *database,
                                          enum mylite_sqlite_translate_status status)
{
    switch (status) {
    case MYLITE_SQLITE_TRANSLATE_OK:
        return MYLITE_OK;
    case MYLITE_SQLITE_TRANSLATE_NOMEM:
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    case MYLITE_SQLITE_TRANSLATE_UNSUPPORTED:
        if (mylite_diagnostics_set_error_message(database, "unsupported SQL statement") ==
            MYLITE_NOMEM) {
            return MYLITE_NOMEM;
        }
        return MYLITE_UNSUPPORTED;
    }

    return MYLITE_UNSUPPORTED;
}

bool mylite_statement_kind_writes(enum mylite_stmt_kind kind)
{
    if (kind == MYLITE_STMT_INSERT_VALUES || kind == MYLITE_STMT_INSERT_SET ||
        kind == MYLITE_STMT_REPLACE_VALUES || kind == MYLITE_STMT_REPLACE_SET ||
        kind == MYLITE_STMT_UPDATE || kind == MYLITE_STMT_DELETE ||
        kind == MYLITE_STMT_ALTER_TABLE || kind == MYLITE_STMT_RENAME_TABLE ||
        kind == MYLITE_STMT_TRUNCATE_TABLE) {
        return true;
    }
    return false;
}

bool mylite_statement_ast_preserves_diagnostics(const struct mylite_sql_ast_node *statement)
{
    if (statement == NULL) {
        return false;
    }
    if (statement->kind == MYLITE_SQL_AST_SHOW_DIAGNOSTICS_STATEMENT) {
        return true;
    }
    if (statement->kind == MYLITE_SQL_AST_SHOW_DIAGNOSTICS_COUNT_STATEMENT) {
        return true;
    }
    return false;
}

// NOLINTNEXTLINE(misc-no-recursion)
void mylite_finalize(mylite_stmt *stmt)
{
    if (stmt == NULL) {
        return;
    }

    sqlite3_finalize(stmt->sqlite_stmt);
    mylite_finalize(stmt->prepared_execute_stmt);
    free(stmt->schema_name);
    mylite_schema_options_deinit(&stmt->options);
    mylite_connection_charset_plan_deinit(&stmt->connection_charset);
    mylite_connection_system_variable_plan_deinit(&stmt->connection_system_variable);
    mylite_user_variable_set_plan_deinit(&stmt->set_user_variable);
    mylite_prepared_statement_prepare_plan_deinit(&stmt->prepare_statement);
    mylite_prepared_statement_execute_plan_deinit(&stmt->execute_prepared);
    mylite_prepared_statement_deallocate_plan_deinit(&stmt->deallocate_prepare);
    mylite_table_ddl_create_table_plan_deinit(&stmt->create_table);
    mylite_table_ddl_drop_table_plan_deinit(&stmt->drop_table);
    mylite_table_ddl_rename_table_plan_deinit(&stmt->rename_table);
    mylite_table_ddl_truncate_table_plan_deinit(&stmt->truncate_table);
    mylite_table_ddl_alter_table_plan_deinit(&stmt->alter_table);
    mylite_table_ddl_index_ddl_plan_deinit(&stmt->index_ddl);
    mylite_dml_insert_values_plan_deinit(&stmt->insert_values);
    mylite_dml_insert_set_plan_deinit(&stmt->insert_set);
    mylite_dml_insert_duplicate_update_plan_deinit(&stmt->insert_update);
    mylite_dml_update_plan_deinit(&stmt->update);
    mylite_dml_delete_plan_deinit(&stmt->delete_plan);
    mylite_transaction_savepoint_plan_deinit(&stmt->savepoint);
    mylite_statement_union_plan_deinit(&stmt->union_plan);
    mylite_select_plan_deinit(&stmt->select_plan);
    mylite_result_metadata_deinit(&stmt->result_metadata);
    mylite_statement_scalar_result_deinit(&stmt->scalar_result);
    mylite_select_result_deinit(&stmt->select_result);
    mylite_sql_ast_deinit(&stmt->select_predicate_ast);
    mylite_sql_ast_deinit(&stmt->scalar_select_ast);
    mylite_sql_ast_deinit(&stmt->insert_ast);
    mylite_sql_ast_deinit(&stmt->update_ast);
    mylite_sql_ast_deinit(&stmt->delete_ast);
    free(stmt->select_sql_text);
    free(stmt->scalar_select_sql_text);
    free(stmt->insert_sql_text);
    free(stmt->update_sql_text);
    free(stmt->delete_sql_text);
    mylite_statement_select_constant_values_deinit(stmt);
    free(stmt->rand_states);
    free(stmt);
}

int64_t mylite_column_int64(const mylite_stmt *stmt, int column)
{
    if (stmt != NULL && stmt->kind == MYLITE_STMT_EXECUTE_PREPARED &&
        stmt->prepared_execute_stmt != NULL) {
        return mylite_column_int64(stmt->prepared_execute_stmt, column);
    }

    const struct mylite_expression_value *value =
        mylite_statement_table_select_current_output_value(stmt, column);

    if (value != NULL) {
        return mylite_expression_value_to_int64(value);
    }
    if (stmt != NULL && stmt->sqlite_stmt != NULL && stmt->kind == MYLITE_STMT_TABLE_SELECT &&
        column >= 0 && (size_t)column < stmt->select_plan.output_count) {
        size_t physical_column = stmt->select_plan.outputs[column].column_index;

        return (int64_t)sqlite3_column_int64(stmt->sqlite_stmt, (int)physical_column);
    }
    if (stmt == NULL || stmt->sqlite_stmt == NULL || column < 0 ||
        column >= sqlite3_column_count(stmt->sqlite_stmt)) {
        if (stmt != NULL && stmt->kind == MYLITE_STMT_SCALAR_SELECT && column >= 0 &&
            (size_t)column < stmt->scalar_result.value_count) {
            return mylite_expression_value_to_int64(&stmt->scalar_result.values[column]);
        }
        return 0;
    }

    return (int64_t)sqlite3_column_int64(stmt->sqlite_stmt, column);
}

const char *mylite_column_text(const mylite_stmt *stmt, int column)
{
    if (stmt != NULL && stmt->kind == MYLITE_STMT_EXECUTE_PREPARED &&
        stmt->prepared_execute_stmt != NULL) {
        return mylite_column_text(stmt->prepared_execute_stmt, column);
    }

    if (stmt != NULL &&
        (stmt->kind == MYLITE_STMT_TABLE_SELECT || stmt->kind == MYLITE_STMT_UNION_QUERY) &&
        stmt->select_result.has_current_row) {
        return mylite_statement_table_select_current_output_text(stmt, column);
    }
    if (stmt != NULL && stmt->sqlite_stmt != NULL && stmt->kind == MYLITE_STMT_TABLE_SELECT &&
        column >= 0 && (size_t)column < stmt->select_plan.output_count) {
        size_t physical_column = stmt->select_plan.outputs[column].column_index;

        return (const char *)sqlite3_column_text(stmt->sqlite_stmt, (int)physical_column);
    }
    if (stmt == NULL || stmt->sqlite_stmt == NULL || column < 0 ||
        column >= sqlite3_column_count(stmt->sqlite_stmt)) {
        if (stmt != NULL && stmt->kind == MYLITE_STMT_SCALAR_SELECT && column >= 0 &&
            (size_t)column < stmt->scalar_result.value_count) {
            return stmt->scalar_result.texts[column];
        }
        return NULL;
    }

    return (const char *)sqlite3_column_text(stmt->sqlite_stmt, column);
}

const struct mylite_expression_value *
mylite_statement_table_select_current_output_value(const mylite_stmt *stmt, int column)
{
    if (stmt == NULL ||
        (stmt->kind != MYLITE_STMT_TABLE_SELECT && stmt->kind != MYLITE_STMT_UNION_QUERY) ||
        column < 0 || (size_t)column >= stmt->select_result.current_value_count ||
        !stmt->select_result.has_current_row) {
        return NULL;
    }
    return &stmt->select_result.current_values[column];
}

const char *mylite_statement_table_select_current_output_text(const mylite_stmt *stmt, int column)
{
    if (stmt == NULL ||
        (stmt->kind != MYLITE_STMT_TABLE_SELECT && stmt->kind != MYLITE_STMT_UNION_QUERY) ||
        column < 0 || (size_t)column >= stmt->select_result.current_value_count ||
        !stmt->select_result.has_current_row) {
        return NULL;
    }
    return stmt->select_result.current_texts[column];
}

void mylite_statement_record_row_count(mylite_stmt *stmt)
{
    if (stmt == NULL || stmt->database == NULL || stmt->previous_row_count_recorded) {
        return;
    }

    stmt->database->previous_row_count = stmt->affected_rows;
    stmt->previous_row_count_recorded = true;
}

void mylite_statement_scalar_result_deinit(struct mylite_scalar_result *result)
{
    if (result == NULL) {
        return;
    }

    for (size_t index = 0U; index < result->value_count; ++index) {
        mylite_expression_value_deinit(&result->values[index]);
        free(result->texts[index]);
    }
    mylite_expression_warnings_deinit(&result->warnings);
    free(result->values);
    free((void *)result->texts);
    free((void *)result->expressions);
    *result = (struct mylite_scalar_result){0};
}

void mylite_statement_select_constant_values_deinit(mylite_stmt *stmt)
{
    if (stmt == NULL) {
        return;
    }

    for (size_t index = 0U; index < stmt->select_constant_value_count; ++index) {
        mylite_expression_value_deinit(&stmt->select_constant_values[index].value);
    }
    free(stmt->select_constant_values);
    stmt->select_constant_values = NULL;
    stmt->select_constant_value_count = 0U;
}

// NOLINTNEXTLINE(misc-no-recursion)
void mylite_statement_union_plan_deinit(struct mylite_union_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    for (size_t index = 0U; index < plan->operand_count; ++index) {
        mylite_finalize(plan->operands[index]);
    }
    free((void *)plan->operands);
    free(plan->operators);
    *plan = (struct mylite_union_plan){0};
}
