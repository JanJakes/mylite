#include "mylite_select_projection.h"

#include "mylite_diagnostics.h"
#include "mylite_select.h"
#include "mylite_select_projection_wildcard_sequence.h"
#include "mylite_select_resolve.h"
#include "mylite_span.h"

#include <stdlib.h>
#include <string.h>

static int append_select_all_wildcard_outputs(mylite_db *database, struct mylite_select_plan *plan);
static int append_select_range_wildcard_outputs(mylite_db *database,
                                                struct mylite_select_plan *plan,
                                                struct mylite_select_table_range range);
static int append_select_table_wildcard_outputs(mylite_db *database,
                                                struct mylite_select_plan *plan,
                                                size_t table_index);
static int append_select_plan_column_output(mylite_db *database, struct mylite_select_plan *plan,
                                            size_t column_index);

int mylite_select_append_wildcard_outputs(mylite_db *database,
                                          const struct mylite_sql_ast_node *wildcard,
                                          struct mylite_select_plan *plan)
{
    bool all_tables = false;
    size_t table_index = mylite_select_plan_table_count(plan);
    int status =
        mylite_select_resolve_plan_wildcard(database, plan, wildcard, &table_index, &all_tables);

    if (status != MYLITE_OK) {
        return status;
    }
    if (!all_tables && table_index == mylite_select_plan_table_count(plan)) {
        char *qualifier = mylite_select_copy_wildcard_qualifier_name(wildcard);

        if (qualifier == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        status = mylite_select_set_unknown_table_error(database, qualifier);
        free(qualifier);
        return status;
    }

    if (!all_tables) {
        return append_select_table_wildcard_outputs(database, plan, table_index);
    }

    return append_select_all_wildcard_outputs(database, plan);
}

static int append_select_all_wildcard_outputs(mylite_db *database, struct mylite_select_plan *plan)
{
    if (plan->from_range_count == 0U) {
        return append_select_range_wildcard_outputs(
            database, plan,
            (struct mylite_select_table_range){
                .first_table = 0U,
                .table_count = mylite_select_plan_table_count(plan),
            });
    }

    for (size_t range_index = 0U; range_index < plan->from_range_count; ++range_index) {
        int status =
            append_select_range_wildcard_outputs(database, plan, plan->from_ranges[range_index]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int append_select_range_wildcard_outputs(mylite_db *database,
                                                struct mylite_select_plan *plan,
                                                struct mylite_select_table_range range)
{
    struct mylite_select_column_sequence sequence = {0};
    int status = mylite_select_build_wildcard_column_sequence(database, plan, range, &sequence);

    for (size_t index = 0U; status == MYLITE_OK && index < sequence.column_count; ++index) {
        status = append_select_plan_column_output(database, plan, sequence.column_indexes[index]);
    }

    mylite_select_column_sequence_deinit(&sequence);
    return status;
}

static int append_select_table_wildcard_outputs(mylite_db *database,
                                                struct mylite_select_plan *plan, size_t table_index)
{
    const struct mylite_select_table *table = mylite_select_plan_table_const(plan, table_index);

    if (table == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    for (size_t index = 0U; index < table->column_count; ++index) {
        const size_t column_index = table->first_column_index + index;
        int status = MYLITE_OK;

        if (!table->columns[index].visible) {
            continue;
        }
        status = append_select_plan_column_output(database, plan, column_index);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int append_select_plan_column_output(mylite_db *database, struct mylite_select_plan *plan,
                                            size_t column_index)
{
    const struct mylite_select_column *column =
        mylite_select_plan_column_const(plan, column_index, NULL);
    char *label = NULL;
    int status = MYLITE_OK;

    if (column == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    label = mylite_copy_span_text(column->name, strlen(column->name));
    if (label == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_select_plan_add_output_column(plan, &(const struct mylite_select_output_column){
                                                            .kind = MYLITE_SELECT_OUTPUT_COLUMN,
                                                            .column_index = column_index,
                                                            .label = label,
                                                        });
    if (status != MYLITE_OK) {
        free(label);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }
    return MYLITE_OK;
}
