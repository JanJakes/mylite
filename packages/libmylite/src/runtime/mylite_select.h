#ifndef MYLITE_RUNTIME_MYLITE_SELECT_H
#define MYLITE_RUNTIME_MYLITE_SELECT_H

#include "mylite_runtime.h"

void mylite_select_plan_deinit(struct mylite_select_plan *plan);
void mylite_select_table_deinit(struct mylite_select_table *table);
void mylite_select_column_deinit(struct mylite_select_column *column);
void mylite_select_output_column_deinit(struct mylite_select_output_column *column);
void mylite_select_aggregate_binding_deinit(struct mylite_select_aggregate_binding *binding);

#endif
