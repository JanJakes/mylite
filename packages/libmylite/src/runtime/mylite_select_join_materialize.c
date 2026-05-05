#include "mylite_select_join_materialize.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_eval.h"
#include "mylite_select_group.h"
#include "mylite_select_join_cache.h"
#include "mylite_select_join_outer_materialize.h"
#include "mylite_select_join_output.h"
#include "mylite_select_join_rows.h"
#include "mylite_select_materialize_common.h"
#include "mylite_select_row_loader.h"
#include "mylite_select_row_match.h"
#include "mylite_select_rowset.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

static int scan_joined_table_select_rows(mylite_stmt *stmt,
                                         struct mylite_table_select_join_materialize_state *state,
                                         struct mylite_table_select_row *row,
                                         const struct mylite_select_eval_callbacks *callbacks);
static int advance_joined_table_select_scan(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    struct mylite_table_select_join_scan_state *scan, bool *out_finished,
    const struct mylite_select_eval_callbacks *callbacks);
static int backtrack_joined_table_select_scan(mylite_stmt *stmt,
                                              struct mylite_table_select_join_scan_state *scan,
                                              bool *out_finished);
static int process_joined_table_select_scan_row(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    struct mylite_table_select_join_scan_state *scan, const struct mylite_select_table *table,
    const struct mylite_table_select_table_rowset *rowset,
    const struct mylite_select_eval_callbacks *callbacks);
static void clear_joined_table_select_scan_frame(struct mylite_table_select_join_scan_state *scan,
                                                 const struct mylite_select_table *table,
                                                 size_t table_index);
static void
clear_joined_table_select_scan_copies(mylite_stmt *stmt,
                                      const struct mylite_table_select_join_scan_state *scan);

int mylite_select_materialize_joined_table_result(
    mylite_stmt *stmt, const struct mylite_select_eval_callbacks *callbacks)
{
    size_t table_count = mylite_select_plan_table_count(&stmt->select_plan);
    struct mylite_table_select_join_materialize_state state = {0};
    struct mylite_table_select_row row = {0};
    bool aggregate_query = (stmt->select_plan.has_group_by || stmt->select_plan.has_aggregate ||
                            stmt->select_plan.has_having) != 0;
    bool distinct = mylite_select_duplicate_mode_is_distinct(stmt->select_plan.duplicate_mode);
    int status = MYLITE_OK;

    if (mylite_select_plan_has_outer_join(&stmt->select_plan)) {
        return mylite_select_materialize_outer_joined_table_result(stmt, callbacks);
    }

    if (!aggregate_query && stmt->select_plan.order_key_count == 0U &&
        stmt->select_plan.limit.has_limit && stmt->select_plan.limit.row_count == 0U) {
        return MYLITE_OK;
    }

    status = mylite_select_eval_constant_predicate(stmt, callbacks);
    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->select_constant_predicate_evaluated && !stmt->select_constant_predicate_matches) {
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

    row.value_count = mylite_select_plan_column_count(&stmt->select_plan);
    if (row.value_count != 0U) {
        row.values = calloc(row.value_count, sizeof(*row.values));
        if (row.values == NULL) {
            mylite_select_rowsets_deinit(state.rowsets, table_count);
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }

    status = mylite_select_load_join_rowsets(stmt, state.rowsets);
    if (status == MYLITE_OK) {
        status = scan_joined_table_select_rows(stmt, &state, &row, callbacks);
    }

    if (status == MYLITE_OK && aggregate_query && state.group_count == 0U &&
        !stmt->select_plan.has_group_by) {
        status = mylite_select_group_append_empty_implicit(stmt, &state.groups, &state.group_count);
    }
    if (status == MYLITE_OK && aggregate_query) {
        status = mylite_select_materialize_append_finalized_groups(stmt, state.groups,
                                                                   state.group_count, callbacks);
    }
    if (status == MYLITE_OK && stmt->select_plan.order_key_count != 0U) {
        status = mylite_select_result_sort_rows(stmt->database, &stmt->select_result,
                                                &stmt->select_plan);
    }
    if (status == MYLITE_OK &&
        (aggregate_query || stmt->select_plan.order_key_count != 0U || distinct)) {
        status = mylite_select_result_apply_limit(&stmt->select_result, &stmt->select_plan.limit);
    }

    mylite_select_groups_deinit(state.groups, state.group_count);
    mylite_select_row_deinit(&row);
    mylite_select_join_condition_cache_deinit(&state.condition_cache);
    mylite_select_rowsets_deinit(state.rowsets, table_count);
    return status;
}

static int scan_joined_table_select_rows(mylite_stmt *stmt,
                                         struct mylite_table_select_join_materialize_state *state,
                                         struct mylite_table_select_row *row,
                                         const struct mylite_select_eval_callbacks *callbacks)
{
    size_t table_count = mylite_select_plan_table_count(&stmt->select_plan);
    struct mylite_table_select_join_scan_state scan = {
        .row = row,
        .table_count = table_count,
    };
    bool finished = false;
    int status = MYLITE_OK;

    if (state->stop) {
        return MYLITE_OK;
    }

    if (table_count == 0U) {
        return mylite_select_join_materialize_row(stmt, state, row, callbacks);
    }

    scan.frames = calloc(table_count, sizeof(*scan.frames));
    if (scan.frames == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    while (!finished && !state->stop) {
        status = advance_joined_table_select_scan(stmt, state, &scan, &finished, callbacks);
        if (status != MYLITE_OK) {
            break;
        }
    }

    clear_joined_table_select_scan_copies(stmt, &scan);
    free(scan.frames);
    return status;
}

static int advance_joined_table_select_scan(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    struct mylite_table_select_join_scan_state *scan, bool *out_finished,
    const struct mylite_select_eval_callbacks *callbacks)
{
    const struct mylite_select_table *table =
        mylite_select_plan_table_const(&stmt->select_plan, scan->table_index);
    const struct mylite_table_select_table_rowset *rowset = &state->rowsets[scan->table_index];

    *out_finished = false;
    if (table == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (scan->frames[scan->table_index].row_index >= rowset->row_count) {
        return backtrack_joined_table_select_scan(stmt, scan, out_finished);
    }
    return process_joined_table_select_scan_row(stmt, state, scan, table, rowset, callbacks);
}

static int backtrack_joined_table_select_scan(mylite_stmt *stmt,
                                              struct mylite_table_select_join_scan_state *scan,
                                              bool *out_finished)
{
    const struct mylite_select_table *table = NULL;

    scan->frames[scan->table_index].row_index = 0U;
    if (scan->table_index == 0U) {
        *out_finished = true;
        return MYLITE_OK;
    }

    --scan->table_index;
    table = mylite_select_plan_table_const(&stmt->select_plan, scan->table_index);
    if (table == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    clear_joined_table_select_scan_frame(scan, table, scan->table_index);
    ++scan->frames[scan->table_index].row_index;
    return MYLITE_OK;
}

static int process_joined_table_select_scan_row(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    struct mylite_table_select_join_scan_state *scan, const struct mylite_select_table *table,
    const struct mylite_table_select_table_rowset *rowset,
    const struct mylite_select_eval_callbacks *callbacks)
{
    bool matches = false;
    int status = mylite_select_join_row_copy_table_values(
        scan->row, table, &rowset->rows[scan->frames[scan->table_index].row_index]);

    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        }
        return status;
    }

    scan->frames[scan->table_index].copied = true;
    status = mylite_select_join_stage_conditions_match(stmt, state, scan, &matches, callbacks);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!matches) {
        clear_joined_table_select_scan_frame(scan, table, scan->table_index);
        ++scan->frames[scan->table_index].row_index;
        return MYLITE_OK;
    }

    if (scan->table_index + 1U == scan->table_count) {
        status = mylite_select_join_materialize_row(stmt, state, scan->row, callbacks);
        clear_joined_table_select_scan_frame(scan, table, scan->table_index);
        ++scan->frames[scan->table_index].row_index;
        return status;
    }

    ++scan->table_index;
    scan->frames[scan->table_index].row_index = 0U;
    scan->frames[scan->table_index].copied = false;
    return MYLITE_OK;
}

static void clear_joined_table_select_scan_frame(struct mylite_table_select_join_scan_state *scan,
                                                 const struct mylite_select_table *table,
                                                 size_t table_index)
{
    if (!scan->frames[table_index].copied) {
        return;
    }
    mylite_select_join_row_clear_table_values(scan->row, table);
    scan->frames[table_index].copied = false;
}

static void
clear_joined_table_select_scan_copies(mylite_stmt *stmt,
                                      const struct mylite_table_select_join_scan_state *scan)
{
    for (size_t index = 0U; index < scan->table_count; ++index) {
        const struct mylite_select_table *table =
            mylite_select_plan_table_const(&stmt->select_plan, index);

        if (table != NULL && scan->frames[index].copied) {
            mylite_select_join_row_clear_table_values(scan->row, table);
        }
    }
}
