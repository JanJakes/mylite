#ifndef MYLITE_RUNTIME_MYLITE_STATEMENT_H
#define MYLITE_RUNTIME_MYLITE_STATEMENT_H

#include "mylite_runtime.h"

const struct mylite_expression_value *
mylite_statement_table_select_current_output_value(const mylite_stmt *stmt, int column);
const char *mylite_statement_table_select_current_output_text(const mylite_stmt *stmt, int column);
void mylite_statement_record_row_count(mylite_stmt *stmt);
void mylite_statement_scalar_result_deinit(struct mylite_scalar_result *result);
void mylite_statement_select_constant_values_deinit(mylite_stmt *stmt);
void mylite_statement_table_select_result_deinit(struct mylite_table_select_result *result);
void mylite_statement_table_select_current_values_deinit(struct mylite_table_select_result *result);
void mylite_statement_table_select_row_deinit(struct mylite_table_select_row *row);
void mylite_statement_union_plan_deinit(struct mylite_union_plan *plan);

#endif
