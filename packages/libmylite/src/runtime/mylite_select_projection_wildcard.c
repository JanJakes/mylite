#include "mylite_select_projection.h"

#include "mylite_diagnostics.h"
#include "mylite_select.h"
#include "mylite_select_resolve.h"
#include "mylite_span.h"

#include <stdlib.h>
#include <string.h>

static int append_select_all_wildcard_outputs(mylite_db *database, struct mylite_select_plan *plan);
static int append_select_range_wildcard_outputs(mylite_db *database,
                                                struct mylite_select_plan *plan,
                                                struct mylite_select_table_range range);
static int build_select_range_column_sequence(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              struct mylite_select_table_range range,
                                              struct mylite_select_column_sequence *sequence);
static int append_select_table_visible_columns_to_sequence(
    mylite_db *database, const struct mylite_select_plan *plan, size_t table_index,
    struct mylite_select_column_sequence *sequence);
static int apply_select_join_step_to_column_sequence(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_select_join_step *step, struct mylite_select_column_sequence *sequence);
static int
append_select_using_columns_to_sequence(mylite_db *database, const struct mylite_select_plan *plan,
                                        const struct mylite_select_join_step *step,
                                        bool right_preserved,
                                        const struct mylite_select_column_sequence *source,
                                        struct mylite_select_column_sequence *out_sequence);
static int append_select_right_using_columns_to_sequence(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_select_join_step *step, struct mylite_select_column_sequence *out_sequence);
static int append_select_left_using_columns_to_sequence(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_select_join_step *step, const struct mylite_select_column_sequence *source,
    struct mylite_select_column_sequence *out_sequence);
static int append_select_non_using_columns_to_sequence(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_select_column_sequence *source, const struct mylite_select_join_step *step,
    struct mylite_select_column_sequence *out_sequence);
static int append_select_table_non_using_columns_to_sequence(
    mylite_db *database, const struct mylite_select_plan *plan, size_t table_index,
    const struct mylite_select_join_step *step, struct mylite_select_column_sequence *out_sequence);
static int append_select_column_to_sequence(mylite_db *database,
                                            struct mylite_select_column_sequence *sequence,
                                            size_t column_index);
static bool select_column_index_is_step_using_column(const struct mylite_select_plan *plan,
                                                     const struct mylite_select_join_step *step,
                                                     size_t column_index);
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
    int status = build_select_range_column_sequence(database, plan, range, &sequence);

    for (size_t index = 0U; status == MYLITE_OK && index < sequence.column_count; ++index) {
        status = append_select_plan_column_output(database, plan, sequence.column_indexes[index]);
    }

    mylite_select_column_sequence_deinit(&sequence);
    return status;
}

static int build_select_range_column_sequence(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              struct mylite_select_table_range range,
                                              struct mylite_select_column_sequence *sequence)
{
    if (range.table_count == 0U) {
        return MYLITE_OK;
    }

    int status = append_select_table_visible_columns_to_sequence(database, plan, range.first_table,
                                                                 sequence);

    for (size_t index = 0U; status == MYLITE_OK && index < plan->join_step_count; ++index) {
        if (!mylite_select_join_step_is_in_range(&plan->join_steps[index], range)) {
            continue;
        }
        status = apply_select_join_step_to_column_sequence(database, plan, &plan->join_steps[index],
                                                           sequence);
    }
    return status;
}

static int append_select_table_visible_columns_to_sequence(
    mylite_db *database, const struct mylite_select_plan *plan, size_t table_index,
    struct mylite_select_column_sequence *sequence)
{
    const struct mylite_select_table *table = mylite_select_plan_table_const(plan, table_index);

    if (table == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (!table->columns[index].visible) {
            continue;
        }
        int status =
            append_select_column_to_sequence(database, sequence, table->first_column_index + index);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int apply_select_join_step_to_column_sequence(mylite_db *database,
                                                     const struct mylite_select_plan *plan,
                                                     const struct mylite_select_join_step *step,
                                                     struct mylite_select_column_sequence *sequence)
{
    bool has_using_columns = false;
    bool right_preserved = step->join_type == MYLITE_SQL_AST_JOIN_RIGHT;
    struct mylite_select_column_sequence next = {0};
    int status = MYLITE_OK;

    for (size_t index = 0U; index < plan->using_column_count; ++index) {
        if (plan->using_columns[index].first_table == step->joined_range.first_table &&
            plan->using_columns[index].table_count == step->joined_range.table_count) {
            has_using_columns = true;
            break;
        }
    }
    if (!has_using_columns) {
        return append_select_table_visible_columns_to_sequence(
            database, plan, step->right_range.first_table, sequence);
    }

    status = append_select_using_columns_to_sequence(database, plan, step, right_preserved,
                                                     sequence, &next);
    if (status == MYLITE_OK && right_preserved) {
        status = append_select_table_non_using_columns_to_sequence(
            database, plan, step->right_range.first_table, step, &next);
    }
    if (status == MYLITE_OK) {
        status = append_select_non_using_columns_to_sequence(database, plan, sequence, step, &next);
    }
    if (status == MYLITE_OK && !right_preserved) {
        status = append_select_table_non_using_columns_to_sequence(
            database, plan, step->right_range.first_table, step, &next);
    }
    if (status != MYLITE_OK) {
        mylite_select_column_sequence_deinit(&next);
        return status;
    }

    mylite_select_column_sequence_deinit(sequence);
    *sequence = next;
    return MYLITE_OK;
}

static int
append_select_using_columns_to_sequence(mylite_db *database, const struct mylite_select_plan *plan,
                                        const struct mylite_select_join_step *step,
                                        bool right_preserved,
                                        const struct mylite_select_column_sequence *source,
                                        struct mylite_select_column_sequence *out_sequence)
{
    if (right_preserved) {
        return append_select_right_using_columns_to_sequence(database, plan, step, out_sequence);
    }
    return append_select_left_using_columns_to_sequence(database, plan, step, source, out_sequence);
}

static int append_select_right_using_columns_to_sequence(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_select_join_step *step, struct mylite_select_column_sequence *out_sequence)
{
    const struct mylite_select_table *table =
        mylite_select_plan_table_const(plan, step->right_range.first_table);

    if (table == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    for (size_t table_column = 0U; table_column < table->column_count; ++table_column) {
        size_t column_index = table->first_column_index + table_column;

        for (size_t using_index = 0U; using_index < plan->using_column_count; ++using_index) {
            const struct mylite_select_join_using_column *using_column =
                &plan->using_columns[using_index];

            if (using_column->first_table == step->joined_range.first_table &&
                using_column->table_count == step->joined_range.table_count &&
                using_column->right_column_index == column_index) {
                int status = append_select_column_to_sequence(database, out_sequence,
                                                              using_column->coalesced_column_index);

                if (status != MYLITE_OK) {
                    return status;
                }
            }
        }
    }
    return MYLITE_OK;
}

static int append_select_left_using_columns_to_sequence(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_select_join_step *step, const struct mylite_select_column_sequence *source,
    struct mylite_select_column_sequence *out_sequence)
{
    for (size_t source_index = 0U; source_index < source->column_count; ++source_index) {
        size_t column_index = source->column_indexes[source_index];

        for (size_t using_index = 0U; using_index < plan->using_column_count; ++using_index) {
            const struct mylite_select_join_using_column *using_column =
                &plan->using_columns[using_index];

            if (using_column->first_table == step->joined_range.first_table &&
                using_column->table_count == step->joined_range.table_count &&
                using_column->coalesced_column_index == column_index) {
                int status = append_select_column_to_sequence(database, out_sequence, column_index);

                if (status != MYLITE_OK) {
                    return status;
                }
            }
        }
    }
    return MYLITE_OK;
}

static int append_select_non_using_columns_to_sequence(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_select_column_sequence *source, const struct mylite_select_join_step *step,
    struct mylite_select_column_sequence *out_sequence)
{
    for (size_t index = 0U; index < source->column_count; ++index) {
        size_t column_index = source->column_indexes[index];

        if (select_column_index_is_step_using_column(plan, step, column_index)) {
            continue;
        }
        int status = append_select_column_to_sequence(database, out_sequence, column_index);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int append_select_table_non_using_columns_to_sequence(
    mylite_db *database, const struct mylite_select_plan *plan, size_t table_index,
    const struct mylite_select_join_step *step, struct mylite_select_column_sequence *out_sequence)
{
    const struct mylite_select_table *table = mylite_select_plan_table_const(plan, table_index);

    if (table == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    for (size_t index = 0U; index < table->column_count; ++index) {
        size_t column_index = table->first_column_index + index;

        if (!table->columns[index].visible ||
            select_column_index_is_step_using_column(plan, step, column_index)) {
            continue;
        }
        int status = append_select_column_to_sequence(database, out_sequence, column_index);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int append_select_column_to_sequence(mylite_db *database,
                                            struct mylite_select_column_sequence *sequence,
                                            size_t column_index)
{
    size_t *columns =
        realloc(sequence->column_indexes, (sequence->column_count + 1U) * sizeof(*columns));

    if (columns == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    sequence->column_indexes = columns;
    sequence->column_indexes[sequence->column_count++] = column_index;
    return MYLITE_OK;
}

static bool select_column_index_is_step_using_column(const struct mylite_select_plan *plan,
                                                     const struct mylite_select_join_step *step,
                                                     size_t column_index)
{
    for (size_t index = 0U; index < plan->using_column_count; ++index) {
        const struct mylite_select_join_using_column *column = &plan->using_columns[index];

        if (column->first_table == step->joined_range.first_table &&
            column->table_count == step->joined_range.table_count &&
            (column->left_column_index == column_index ||
             column->right_column_index == column_index ||
             column->coalesced_column_index == column_index)) {
            return true;
        }
    }
    return false;
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
