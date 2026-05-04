#include "mylite_select_group.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_aggregate.h"
#include "mylite_select_rowset.h"

#include <stdlib.h>

static int initialize_table_select_group(mylite_stmt *stmt, struct mylite_table_select_group *group,
                                         const struct mylite_table_select_row *row,
                                         const struct mylite_select_eval_callbacks *callbacks);

int mylite_select_group_append(mylite_stmt *stmt, struct mylite_table_select_group **groups,
                               size_t *group_count, const struct mylite_table_select_row *row,
                               const struct mylite_select_eval_callbacks *callbacks,
                               struct mylite_table_select_group **out_group)
{
    struct mylite_table_select_group *new_groups =
        realloc(*groups, (*group_count + 1U) * sizeof(**groups));
    int status = MYLITE_OK;

    *out_group = NULL;
    if (new_groups == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    *groups = new_groups;
    (*groups)[*group_count] = (struct mylite_table_select_group){0};
    status = initialize_table_select_group(stmt, &(*groups)[*group_count], row, callbacks);
    if (status != MYLITE_OK) {
        mylite_select_group_deinit(&(*groups)[*group_count]);
        return status;
    }

    *out_group = &(*groups)[*group_count];
    *group_count += 1U;
    return MYLITE_OK;
}

int mylite_select_group_find(mylite_stmt *stmt, struct mylite_table_select_group *groups,
                             size_t group_count, const struct mylite_table_select_row *row,
                             const struct mylite_select_eval_callbacks *callbacks,
                             struct mylite_table_select_group **out_group)
{
    struct mylite_expression_value *values = NULL;
    size_t value_count = stmt->select_plan.group_key_count;
    int status = MYLITE_OK;

    *out_group = NULL;
    if (value_count == 0U) {
        if (group_count != 0U) {
            *out_group = &groups[0];
        }
        return MYLITE_OK;
    }

    values = calloc(value_count, sizeof(*values));
    if (values == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < value_count; ++index) {
        status = mylite_select_eval_group_key(stmt, row, &stmt->select_plan.group_keys[index],
                                              callbacks, &values[index]);
        if (status != MYLITE_OK) {
            goto cleanup;
        }
    }

    for (size_t group_index = 0U; group_index < group_count; ++group_index) {
        bool matches = groups[group_index].group_value_count == value_count;

        for (size_t value_index = 0U; matches && value_index < value_count; ++value_index) {
            if (mylite_select_compare_values(&groups[group_index].group_values[value_index],
                                             &values[value_index]) != 0) {
                matches = false;
            }
        }
        if (matches) {
            *out_group = &groups[group_index];
            break;
        }
    }

cleanup:
    for (size_t index = 0U; index < value_count; ++index) {
        mylite_expression_value_deinit(&values[index]);
    }
    free(values);
    return status;
}

int mylite_select_group_update(mylite_stmt *stmt, struct mylite_table_select_group *group,
                               const struct mylite_table_select_row *row,
                               const struct mylite_select_eval_callbacks *callbacks)
{
    for (size_t index = 0U; index < stmt->select_plan.aggregate_binding_count; ++index) {
        int status = mylite_select_update_aggregate_state(
            stmt, &group->aggregate_states[index], &stmt->select_plan.aggregate_bindings[index],
            row, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_select_group_finalize(mylite_stmt *stmt, const struct mylite_table_select_group *group,
                                 struct mylite_table_select_row *out_row)
{
    size_t column_count = group->representative.value_count;

    out_row->values = calloc(column_count, sizeof(*out_row->values));
    if (out_row->values == NULL && column_count != 0U) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_row->value_count = column_count;
    for (size_t index = 0U; index < column_count; ++index) {
        if (mylite_expression_value_copy(&group->representative.values[index],
                                         &out_row->values[index]) != 0) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }

    out_row->aggregate_values =
        calloc(group->aggregate_state_count, sizeof(*out_row->aggregate_values));
    if (out_row->aggregate_values == NULL && group->aggregate_state_count != 0U) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_row->aggregate_value_count = group->aggregate_state_count;
    for (size_t index = 0U; index < group->aggregate_state_count; ++index) {
        int status = mylite_select_finalize_aggregate_state(
            stmt, &group->aggregate_states[index], &stmt->select_plan.aggregate_bindings[index],
            &out_row->aggregate_values[index]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_select_group_append_empty_implicit(mylite_stmt *stmt,
                                              struct mylite_table_select_group **groups,
                                              size_t *group_count)
{
    struct mylite_table_select_group *new_groups = calloc(1U, sizeof(*new_groups));

    if (new_groups == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    *groups = new_groups;
    *group_count = 1U;
    (*groups)[0].aggregate_states =
        calloc(stmt->select_plan.aggregate_binding_count, sizeof(*(*groups)[0].aggregate_states));
    if ((*groups)[0].aggregate_states == NULL && stmt->select_plan.aggregate_binding_count != 0U) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    (*groups)[0].aggregate_state_count = stmt->select_plan.aggregate_binding_count;
    return MYLITE_OK;
}

void mylite_select_groups_deinit(struct mylite_table_select_group *groups, size_t group_count)
{
    for (size_t index = 0U; index < group_count; ++index) {
        mylite_select_group_deinit(&groups[index]);
    }
    free(groups);
}

void mylite_select_group_deinit(struct mylite_table_select_group *group)
{
    if (group == NULL) {
        return;
    }

    mylite_select_row_deinit(&group->representative);
    for (size_t index = 0U; index < group->group_value_count; ++index) {
        mylite_expression_value_deinit(&group->group_values[index]);
    }
    for (size_t index = 0U; index < group->aggregate_state_count; ++index) {
        mylite_select_aggregate_state_deinit(&group->aggregate_states[index]);
    }
    free(group->group_values);
    free(group->aggregate_states);
    *group = (struct mylite_table_select_group){0};
}

static int initialize_table_select_group(mylite_stmt *stmt, struct mylite_table_select_group *group,
                                         const struct mylite_table_select_row *row,
                                         const struct mylite_select_eval_callbacks *callbacks)
{
    size_t column_count =
        row == NULL ? mylite_select_plan_column_count(&stmt->select_plan) : row->value_count;

    group->representative.value_count = column_count;
    if (column_count != 0U) {
        group->representative.values = calloc(column_count, sizeof(*group->representative.values));
        if (group->representative.values == NULL) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    if (row != NULL) {
        for (size_t index = 0U; index < column_count; ++index) {
            if (mylite_expression_value_copy(&row->values[index],
                                             &group->representative.values[index]) != 0) {
                (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
                return MYLITE_NOMEM;
            }
        }
        group->has_representative = true;
    }

    group->group_value_count = stmt->select_plan.group_key_count;
    group->group_values = calloc(group->group_value_count, sizeof(*group->group_values));
    if (group->group_values == NULL && group->group_value_count != 0U) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < group->group_value_count; ++index) {
        int status = mylite_select_eval_group_key(stmt, row, &stmt->select_plan.group_keys[index],
                                                  callbacks, &group->group_values[index]);

        if (status != MYLITE_OK) {
            return status;
        }
    }

    group->aggregate_state_count = stmt->select_plan.aggregate_binding_count;
    group->aggregate_states =
        calloc(group->aggregate_state_count, sizeof(*group->aggregate_states));
    if (group->aggregate_states == NULL && group->aggregate_state_count != 0U) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}
