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
