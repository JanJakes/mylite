#include "mylite_select_join_outer_materialize.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_eval.h"
#include "mylite_select_group.h"
#include "mylite_select_join_cache.h"
#include "mylite_select_join_output.h"
#include "mylite_select_join_range_rowset.h"
#include "mylite_select_join_rows.h"
#include "mylite_select_materialize_common.h"
#include "mylite_select_row_loader.h"
#include "mylite_select_rowset.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

static int scan_outer_joined_table_select_rows(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_eval_callbacks *callbacks
);

static int process_outer_joined_table_range_rows(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_table_rowset *range_rowsets,
    size_t range_count,
    const struct mylite_select_eval_callbacks *callbacks
);

static int process_outer_joined_table_range_row(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_table_rowset *range_rowsets,
    const size_t *row_indexes,
    size_t range_count,
    const struct mylite_select_eval_callbacks *callbacks
);

int mylite_select_materialize_outer_joined_table_result(
    mylite_stmt *stmt,
    const struct mylite_select_eval_callbacks *callbacks
) {
    size_t table_count = mylite_select_plan_table_count(&stmt->select_plan);
    struct mylite_table_select_join_materialize_state state = {0};
    bool aggregate_query = (stmt->select_plan.has_group_by || stmt->select_plan.has_aggregate ||
                            stmt->select_plan.has_having) != 0;
    bool distinct = mylite_select_duplicate_mode_is_distinct(stmt->select_plan.duplicate_mode);
    int status = MYLITE_OK;

    if (!stmt->select_plan.calc_found_rows && !aggregate_query &&
        stmt->select_plan.order_key_count == 0U && stmt->select_plan.limit.has_limit &&
        stmt->select_plan.limit.row_count == 0U) {
        stmt->found_rows = 0U;
        return MYLITE_OK;
    }

    status = mylite_select_eval_constant_predicate(stmt, callbacks);
    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->select_constant_predicate_evaluated && !stmt->select_constant_predicate_matches) {
        stmt->found_rows = 0U;
        return MYLITE_OK;
    }
    if (table_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    state.rowsets = calloc(table_count, sizeof(*state.rowsets));
    if (state.rowsets == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_select_load_join_rowsets(stmt, state.rowsets);
    if (status == MYLITE_OK) {
        status = scan_outer_joined_table_select_rows(stmt, &state, callbacks);
    }

    if (status == MYLITE_OK && aggregate_query && state.group_count == 0U &&
        !stmt->select_plan.has_group_by) {
        status = mylite_select_group_append_empty_implicit(stmt, &state.groups, &state.group_count);
    }
    if (status == MYLITE_OK && aggregate_query) {
        status = mylite_select_materialize_append_finalized_groups(
            stmt,
            state.groups,
            state.group_count,
            callbacks
        );
    }
    if (status == MYLITE_OK && stmt->select_plan.order_key_count != 0U) {
        status = mylite_select_result_sort_rows(
            stmt->database,
            &stmt->select_result,
            &stmt->select_plan
        );
    }
    if (status == MYLITE_OK &&
        (aggregate_query || stmt->select_plan.order_key_count != 0U || distinct)) {
        stmt->found_rows = stmt->select_result.row_count;
        status = mylite_select_result_apply_limit(&stmt->select_result, &stmt->select_plan.limit);
    } else if (status == MYLITE_OK) {
        stmt->found_rows = state.matched_row;
    }

    mylite_select_groups_deinit(state.groups, state.group_count);
    mylite_select_join_condition_cache_deinit(&state.condition_cache);
    mylite_select_rowsets_deinit(state.rowsets, table_count);
    return status;
}

static int scan_outer_joined_table_select_rows(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_eval_callbacks *callbacks
) {
    size_t range_count = stmt->select_plan.from_range_count;
    struct mylite_table_select_table_rowset *range_rowsets = NULL;
    int status = MYLITE_OK;

    if (range_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }
    range_rowsets = calloc(range_count, sizeof(*range_rowsets));
    if (range_rowsets == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    for (size_t range_index = 0U; status == MYLITE_OK && range_index < range_count; ++range_index) {
        struct mylite_select_table_range range = stmt->select_plan.from_ranges[range_index];

        status = mylite_select_join_range_rowset_materialize(
            stmt,
            state,
            &range,
            &range_rowsets[range_index],
            callbacks
        );
    }
    if (status == MYLITE_OK) {
        status = process_outer_joined_table_range_rows(
            stmt,
            state,
            range_rowsets,
            range_count,
            callbacks
        );
    }

    for (size_t index = 0U; index < range_count; ++index) {
        mylite_select_rowset_deinit(&range_rowsets[index]);
    }
    free(range_rowsets);
    return status;
}

static int process_outer_joined_table_range_rows(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_table_rowset *range_rowsets,
    size_t range_count,
    const struct mylite_select_eval_callbacks *callbacks
) {
    size_t *row_indexes = NULL;
    bool finished = false;
    int status = MYLITE_OK;

    for (size_t range_index = 0U; range_index < range_count; ++range_index) {
        if (range_rowsets[range_index].row_count == 0U) {
            return MYLITE_OK;
        }
    }

    row_indexes = calloc(range_count, sizeof(*row_indexes));
    if (row_indexes == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    while (!finished && !state->stop) {
        status = process_outer_joined_table_range_row(
            stmt,
            state,
            range_rowsets,
            row_indexes,
            range_count,
            callbacks
        );
        if (status != MYLITE_OK) {
            break;
        }

        for (size_t index = range_count; index > 0U; --index) {
            size_t range_index = index - 1U;

            ++row_indexes[range_index];
            if (row_indexes[range_index] < range_rowsets[range_index].row_count) {
                break;
            }
            row_indexes[range_index] = 0U;
            if (range_index == 0U) {
                finished = true;
            }
        }
    }

    free(row_indexes);
    return status;
}

static int process_outer_joined_table_range_row(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_table_rowset *range_rowsets,
    const size_t *row_indexes,
    size_t range_count,
    const struct mylite_select_eval_callbacks *callbacks
) {
    struct mylite_table_select_row row = {0};
    int status = MYLITE_OK;

    row.value_count = mylite_select_plan_column_count(&stmt->select_plan);
    row.source_row_index_count = mylite_select_plan_table_count(&stmt->select_plan);
    row.source_rowid_count = mylite_select_plan_table_count(&stmt->select_plan);
    if (row.value_count != 0U) {
        row.values = calloc(row.value_count, sizeof(*row.values));
        if (row.values == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK && row.source_row_index_count != 0U) {
        row.source_row_indexes =
            calloc(row.source_row_index_count, sizeof(*row.source_row_indexes));
        if (row.source_row_indexes == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK && row.source_rowid_count != 0U) {
        row.source_rowids = calloc(row.source_rowid_count, sizeof(*row.source_rowids));
        if (row.source_rowids == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status != MYLITE_OK) {
        mylite_select_row_deinit(&row);
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return status;
    }
    for (size_t index = 0U; index < row.source_row_index_count; ++index) {
        row.source_row_indexes[index] = SIZE_MAX;
    }

    for (size_t range_index = 0U; status == MYLITE_OK && range_index < range_count; ++range_index) {
        struct mylite_select_table_range range = stmt->select_plan.from_ranges[range_index];

        status = mylite_select_join_row_copy_range_values(
            &row,
            &range_rowsets[range_index].rows[row_indexes[range_index]],
            range,
            &stmt->select_plan
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_select_join_materialize_row(stmt, state, &row, callbacks);
    }
    mylite_select_row_deinit(&row);
    return status;
}
