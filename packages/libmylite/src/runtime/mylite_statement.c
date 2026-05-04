#include "mylite_statement.h"

#include <stdlib.h>

int64_t mylite_affected_rows(const mylite_stmt *stmt)
{
    if (stmt == NULL) {
        return -1;
    }

    return stmt->affected_rows;
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
