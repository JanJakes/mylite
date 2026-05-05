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

void mylite_select_column_sequence_deinit(struct mylite_select_column_sequence *sequence)
{
    if (sequence == NULL) {
        return;
    }
    free(sequence->column_indexes);
    *sequence = (struct mylite_select_column_sequence){0};
}

int mylite_select_plan_add_output_column(struct mylite_select_plan *plan,
                                         const struct mylite_select_output_column *output)
{
    struct mylite_select_output_column *outputs =
        realloc(plan->outputs, (plan->output_count + 1U) * sizeof(*plan->outputs));

    if (outputs == NULL) {
        return MYLITE_NOMEM;
    }

    plan->outputs = outputs;
    plan->outputs[plan->output_count++] = *output;
    return MYLITE_OK;
}

int mylite_select_plan_add_order_key(struct mylite_select_plan *plan,
                                     const struct mylite_select_order_key *order_key)
{
    struct mylite_select_order_key *order_keys =
        realloc(plan->order_keys, (plan->order_key_count + 1U) * sizeof(*plan->order_keys));

    if (order_keys == NULL) {
        return MYLITE_NOMEM;
    }

    plan->order_keys = order_keys;
    plan->order_keys[plan->order_key_count++] = *order_key;
    return MYLITE_OK;
}

int mylite_select_plan_add_group_key(struct mylite_select_plan *plan,
                                     const struct mylite_select_group_key *group_key)
{
    struct mylite_select_group_key *group_keys =
        realloc(plan->group_keys, (plan->group_key_count + 1U) * sizeof(*plan->group_keys));

    if (group_keys == NULL) {
        return MYLITE_NOMEM;
    }

    plan->group_keys = group_keys;
    plan->group_keys[plan->group_key_count++] = *group_key;
    return MYLITE_OK;
}

int mylite_select_plan_add_aggregate_binding(struct mylite_select_plan *plan,
                                             const struct mylite_select_aggregate_binding *binding)
{
    struct mylite_select_aggregate_binding *bindings =
        realloc(plan->aggregate_bindings,
                (plan->aggregate_binding_count + 1U) * sizeof(*plan->aggregate_bindings));

    if (bindings == NULL) {
        return MYLITE_NOMEM;
    }

    plan->aggregate_bindings = bindings;
    plan->aggregate_bindings[plan->aggregate_binding_count++] = *binding;
    return MYLITE_OK;
}

void mylite_select_plan_clear_aggregate_bindings(struct mylite_select_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    for (size_t index = 0U; index < plan->aggregate_binding_count; ++index) {
        mylite_select_aggregate_binding_deinit(&plan->aggregate_bindings[index]);
    }
    free(plan->aggregate_bindings);
    plan->aggregate_bindings = NULL;
    plan->aggregate_binding_count = 0U;
    plan->has_aggregate = false;
}

void mylite_select_plan_mark_output_order_reference(struct mylite_select_plan *plan,
                                                    size_t output_index)
{
    if (plan != NULL && output_index < plan->output_count) {
        plan->outputs[output_index].referenced_by_order = true;
    }
}
