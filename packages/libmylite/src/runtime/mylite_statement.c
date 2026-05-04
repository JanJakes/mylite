#include "mylite_statement.h"

#include "mylite_dml.h"
#include "mylite_metadata.h"
#include "mylite_schema.h"
#include "mylite_select.h"
#include "mylite_table_ddl.h"
#include "mylite_transactions.h"
#include "sqlite3.h"

#include <stdlib.h>

int64_t mylite_affected_rows(const mylite_stmt *stmt)
{
    if (stmt == NULL) {
        return -1;
    }

    return stmt->affected_rows;
}

// NOLINTNEXTLINE(misc-no-recursion)
void mylite_finalize(mylite_stmt *stmt)
{
    if (stmt == NULL) {
        return;
    }

    sqlite3_finalize(stmt->sqlite_stmt);
    free(stmt->schema_name);
    free(stmt->character_set_name);
    free(stmt->collation_name);
    mylite_schema_options_deinit(&stmt->options);
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
    mylite_statement_table_select_result_deinit(&stmt->select_result);
    mylite_sql_ast_deinit(&stmt->select_predicate_ast);
    mylite_sql_ast_deinit(&stmt->scalar_select_ast);
    mylite_sql_ast_deinit(&stmt->update_ast);
    mylite_sql_ast_deinit(&stmt->delete_ast);
    free(stmt->select_sql_text);
    free(stmt->scalar_select_sql_text);
    free(stmt->update_sql_text);
    free(stmt->delete_sql_text);
    mylite_statement_select_constant_values_deinit(stmt);
    free(stmt);
}

int64_t mylite_column_int64(const mylite_stmt *stmt, int column)
{
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

void mylite_statement_table_select_result_deinit(struct mylite_table_select_result *result)
{
    if (result == NULL) {
        return;
    }

    mylite_statement_table_select_current_values_deinit(result);
    for (size_t index = 0U; index < result->row_count; ++index) {
        mylite_statement_table_select_row_deinit(&result->rows[index]);
    }
    free(result->rows);
    *result = (struct mylite_table_select_result){0};
}

void mylite_statement_table_select_current_values_deinit(struct mylite_table_select_result *result)
{
    if (result == NULL) {
        return;
    }
    for (size_t index = 0U; index < result->current_value_count; ++index) {
        mylite_expression_value_deinit(&result->current_values[index]);
        free(result->current_texts[index]);
    }
    free(result->current_values);
    free((void *)result->current_texts);
    result->current_values = NULL;
    result->current_texts = NULL;
    result->current_value_count = 0U;
    result->has_current_row = false;
}

void mylite_statement_table_select_row_deinit(struct mylite_table_select_row *row)
{
    if (row == NULL) {
        return;
    }

    for (size_t index = 0U; index < row->value_count; ++index) {
        mylite_expression_value_deinit(&row->values[index]);
    }
    for (size_t index = 0U; index < row->output_value_count; ++index) {
        mylite_expression_value_deinit(&row->output_values[index]);
    }
    for (size_t index = 0U; index < row->order_value_count; ++index) {
        mylite_expression_value_deinit(&row->order_values[index]);
    }
    for (size_t index = 0U; index < row->aggregate_value_count; ++index) {
        mylite_expression_value_deinit(&row->aggregate_values[index]);
    }
    free(row->values);
    free(row->output_values);
    free(row->order_values);
    free(row->aggregate_values);
    free(row->source_row_indexes);
    *row = (struct mylite_table_select_row){0};
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
