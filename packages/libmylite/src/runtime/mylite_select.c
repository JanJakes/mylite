#include "mylite_select.h"

#include <stdlib.h>

void mylite_select_plan_deinit(struct mylite_select_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    mylite_select_table_deinit(&plan->table);
    for (size_t index = 0U; index < plan->table_count; ++index) {
        mylite_select_table_deinit(&plan->tables[index]);
    }
    free(plan->tables);
    free(plan->from_ranges);
    free(plan->join_steps);
    for (size_t index = 0U; index < plan->output_count; ++index) {
        mylite_select_output_column_deinit(&plan->outputs[index]);
    }
    free(plan->outputs);
    free(plan->order_keys);
    free(plan->group_keys);
    for (size_t index = 0U; index < plan->aggregate_binding_count; ++index) {
        mylite_select_aggregate_binding_deinit(&plan->aggregate_bindings[index]);
    }
    free(plan->aggregate_bindings);
    free(plan->join_predicates);
    for (size_t index = 0U; index < plan->using_column_count; ++index) {
        free(plan->using_columns[index].name);
    }
    free(plan->using_columns);
    for (size_t index = 0U; index < plan->using_request_count; ++index) {
        for (size_t name_index = 0U; name_index < plan->using_requests[index].name_count;
             ++name_index) {
            free(plan->using_requests[index].names[name_index]);
        }
        free((void *)plan->using_requests[index].names);
    }
    free(plan->using_requests);
    *plan = (struct mylite_select_plan){0};
}

void mylite_select_table_deinit(struct mylite_select_table *table)
{
    if (table == NULL) {
        return;
    }

    free(table->schema_name);
    free(table->table_name);
    free(table->alias);
    free(table->physical_name);
    for (size_t index = 0U; index < table->column_count; ++index) {
        mylite_select_column_deinit(&table->columns[index]);
    }
    free(table->columns);
    *table = (struct mylite_select_table){0};
}

void mylite_select_column_deinit(struct mylite_select_column *column)
{
    if (column == NULL) {
        return;
    }

    free(column->name);
    *column = (struct mylite_select_column){0};
}

void mylite_select_output_column_deinit(struct mylite_select_output_column *column)
{
    if (column == NULL) {
        return;
    }

    free(column->label);
    *column = (struct mylite_select_output_column){0};
}

void mylite_select_aggregate_binding_deinit(struct mylite_select_aggregate_binding *binding)
{
    if (binding == NULL) {
        return;
    }

    free(binding->argument_descriptors);
    *binding = (struct mylite_select_aggregate_binding){0};
}
