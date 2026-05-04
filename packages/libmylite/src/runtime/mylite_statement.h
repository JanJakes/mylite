#ifndef MYLITE_RUNTIME_MYLITE_STATEMENT_H
#define MYLITE_RUNTIME_MYLITE_STATEMENT_H

#include "mylite_runtime.h"

void mylite_statement_record_row_count(mylite_stmt *stmt);
void mylite_statement_table_select_result_deinit(struct mylite_table_select_result *result);
void mylite_statement_table_select_current_values_deinit(struct mylite_table_select_result *result);
void mylite_statement_table_select_row_deinit(struct mylite_table_select_row *row);

#endif
