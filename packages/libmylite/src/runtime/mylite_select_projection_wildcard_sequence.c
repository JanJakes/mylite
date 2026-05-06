#include "mylite_select_projection_wildcard_sequence.h"

#include "mylite_diagnostics.h"
#include "mylite_select.h"

#include <stdbool.h>
#include <stdlib.h>

static int append_select_table_visible_columns_to_sequence(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    size_t table_index,
    struct mylite_select_column_sequence *sequence
);

static int apply_select_join_step_to_column_sequence(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_select_join_step *step,
    struct mylite_select_column_sequence *sequence
);

static int append_select_using_columns_to_sequence(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_select_join_step *step,
    bool right_preserved,
    const struct mylite_select_column_sequence *source,
    struct mylite_select_column_sequence *out_sequence
);

static int append_select_right_using_columns_to_sequence(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_select_join_step *step,
    struct mylite_select_column_sequence *out_sequence
);

static int append_select_left_using_columns_to_sequence(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_select_join_step *step,
    const struct mylite_select_column_sequence *source,
    struct mylite_select_column_sequence *out_sequence
);

static int append_select_non_using_columns_to_sequence(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_select_column_sequence *source,
    const struct mylite_select_join_step *step,
    struct mylite_select_column_sequence *out_sequence
);

static int append_select_table_non_using_columns_to_sequence(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    size_t table_index,
    const struct mylite_select_join_step *step,
    struct mylite_select_column_sequence *out_sequence
);

static int append_select_column_to_sequence(
    mylite_db *database,
    struct mylite_select_column_sequence *sequence,
    size_t column_index
);

static bool select_column_index_is_step_using_column(
    const struct mylite_select_plan *plan,
    const struct mylite_select_join_step *step,
    size_t column_index
);

int mylite_select_build_wildcard_column_sequence(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    struct mylite_select_table_range range,
    struct mylite_select_column_sequence *out_sequence
) {
    if (range.table_count == 0U) {
        return MYLITE_OK;
    }

    int status = append_select_table_visible_columns_to_sequence(
        database,
        plan,
        range.first_table,
        out_sequence
    );

    for (size_t index = 0U; status == MYLITE_OK && index < plan->join_step_count; ++index) {
        if (!mylite_select_join_step_is_in_range(&plan->join_steps[index], range)) {
            continue;
        }
        status = apply_select_join_step_to_column_sequence(
            database,
            plan,
            &plan->join_steps[index],
            out_sequence
        );
    }
    return status;
}

static int append_select_table_visible_columns_to_sequence(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    size_t table_index,
    struct mylite_select_column_sequence *sequence
) {
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

static int apply_select_join_step_to_column_sequence(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_select_join_step *step,
    struct mylite_select_column_sequence *sequence
) {
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
            database,
            plan,
            step->right_range.first_table,
            sequence
        );
    }

    status = append_select_using_columns_to_sequence(
        database,
        plan,
        step,
        right_preserved,
        sequence,
        &next
    );
    if (status == MYLITE_OK && right_preserved) {
        status = append_select_table_non_using_columns_to_sequence(
            database,
            plan,
            step->right_range.first_table,
            step,
            &next
        );
    }
    if (status == MYLITE_OK) {
        status = append_select_non_using_columns_to_sequence(database, plan, sequence, step, &next);
    }
    if (status == MYLITE_OK && !right_preserved) {
        status = append_select_table_non_using_columns_to_sequence(
            database,
            plan,
            step->right_range.first_table,
            step,
            &next
        );
    }
    if (status != MYLITE_OK) {
        mylite_select_column_sequence_deinit(&next);
        return status;
    }

    mylite_select_column_sequence_deinit(sequence);
    *sequence = next;
    return MYLITE_OK;
}

static int append_select_using_columns_to_sequence(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_select_join_step *step,
    bool right_preserved,
    const struct mylite_select_column_sequence *source,
    struct mylite_select_column_sequence *out_sequence
) {
    if (right_preserved) {
        return append_select_right_using_columns_to_sequence(database, plan, step, out_sequence);
    }
    return append_select_left_using_columns_to_sequence(database, plan, step, source, out_sequence);
}

static int append_select_right_using_columns_to_sequence(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_select_join_step *step,
    struct mylite_select_column_sequence *out_sequence
) {
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
                int status = append_select_column_to_sequence(
                    database,
                    out_sequence,
                    using_column->coalesced_column_index
                );

                if (status != MYLITE_OK) {
                    return status;
                }
            }
        }
    }
    return MYLITE_OK;
}

static int append_select_left_using_columns_to_sequence(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_select_join_step *step,
    const struct mylite_select_column_sequence *source,
    struct mylite_select_column_sequence *out_sequence
) {
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
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_select_column_sequence *source,
    const struct mylite_select_join_step *step,
    struct mylite_select_column_sequence *out_sequence
) {
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
    mylite_db *database,
    const struct mylite_select_plan *plan,
    size_t table_index,
    const struct mylite_select_join_step *step,
    struct mylite_select_column_sequence *out_sequence
) {
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

static int append_select_column_to_sequence(
    mylite_db *database,
    struct mylite_select_column_sequence *sequence,
    size_t column_index
) {
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

static bool select_column_index_is_step_using_column(
    const struct mylite_select_plan *plan,
    const struct mylite_select_join_step *step,
    size_t column_index
) {
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
