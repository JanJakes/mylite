#include "mylite_select_materialize.h"

#include "mylite_diagnostics.h"
#include "mylite_expression.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_eval.h"
#include "mylite_select_group.h"
#include "mylite_select_join_cache.h"
#include "mylite_select_row_loader.h"
#include "mylite_select_row_match.h"
#include "mylite_select_rowset.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static int
materialize_ordered_table_select_result(mylite_stmt *stmt,
                                        const struct mylite_select_eval_callbacks *callbacks);
static int
materialize_unordered_table_select_result(mylite_stmt *stmt,
                                          const struct mylite_select_eval_callbacks *callbacks);
static int
append_unordered_table_select_matched_row(mylite_stmt *stmt, struct mylite_table_select_row *row,
                                          struct mylite_unordered_table_select_append_state *state,
                                          bool distinct,
                                          const struct mylite_select_eval_callbacks *callbacks);
static int
append_unordered_table_select_distinct_row(mylite_stmt *stmt, struct mylite_table_select_row *row,
                                           const struct mylite_select_eval_callbacks *callbacks);
static int
append_unordered_table_select_limited_row(mylite_stmt *stmt, struct mylite_table_select_row *row,
                                          struct mylite_unordered_table_select_append_state *state);
static int
materialize_joined_table_select_result(mylite_stmt *stmt,
                                       const struct mylite_select_eval_callbacks *callbacks);
static int
materialize_outer_joined_table_select_result(mylite_stmt *stmt,
                                             const struct mylite_select_eval_callbacks *callbacks);
static int scan_joined_table_select_rows(mylite_stmt *stmt,
                                         struct mylite_table_select_join_materialize_state *state,
                                         struct mylite_table_select_row *row,
                                         const struct mylite_select_eval_callbacks *callbacks);
static int
scan_outer_joined_table_select_rows(mylite_stmt *stmt,
                                    struct mylite_table_select_join_materialize_state *state,
                                    const struct mylite_select_eval_callbacks *callbacks);
static int materialize_select_from_range_rowset(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    struct mylite_select_table_range range, struct mylite_table_select_table_rowset *out_rowset,
    const struct mylite_select_eval_callbacks *callbacks);
static int materialize_select_base_range_rowset(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    struct mylite_select_table_range range, struct mylite_table_select_table_rowset *out_rowset);
static int apply_select_join_step_to_rowset(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_join_step *step, struct mylite_table_select_table_rowset *rowset,
    const struct mylite_select_eval_callbacks *callbacks);
static int append_select_join_step_matches(mylite_stmt *stmt,
                                           struct mylite_table_select_join_materialize_state *state,
                                           const struct mylite_select_join_step *step,
                                           const struct mylite_table_select_table_rowset *left,
                                           bool *right_matched,
                                           struct mylite_table_select_table_rowset *out_rowset,
                                           const struct mylite_select_eval_callbacks *callbacks);
static int append_select_join_step_left_matches(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_join_step *step, const struct mylite_table_select_row *left_row,
    const struct mylite_table_select_table_rowset *right,
    struct mylite_select_join_match_tracking *tracking,
    struct mylite_table_select_table_rowset *out_rowset,
    const struct mylite_select_eval_callbacks *callbacks);
static int append_select_join_step_match(mylite_stmt *stmt,
                                         struct mylite_table_select_join_materialize_state *state,
                                         const struct mylite_select_join_step *step,
                                         const struct mylite_select_join_row_pair *rows,
                                         bool *out_matches,
                                         struct mylite_table_select_table_rowset *out_rowset,
                                         const struct mylite_select_eval_callbacks *callbacks);
static int
append_select_null_extended_left_row(mylite_stmt *stmt,
                                     const struct mylite_table_select_row *left_row,
                                     struct mylite_table_select_table_rowset *out_rowset);
static int append_select_null_extended_right_rows(
    mylite_stmt *stmt, const struct mylite_select_join_step *step,
    const struct mylite_table_select_table_rowset *right, const bool *right_matched,
    struct mylite_table_select_table_rowset *out_rowset);
static int
append_select_null_extended_right_row(mylite_stmt *stmt, const struct mylite_select_join_step *step,
                                      const struct mylite_table_select_row *right_row,
                                      size_t right_row_index,
                                      struct mylite_table_select_table_rowset *out_rowset);
static int append_empty_joined_table_select_row(mylite_stmt *stmt,
                                                struct mylite_table_select_table_rowset *rowset,
                                                struct mylite_table_select_row **out_row);
static int copy_select_base_table_row_values(mylite_db *database,
                                             struct mylite_table_select_row *row,
                                             const struct mylite_select_table *table,
                                             size_t table_index,
                                             const struct mylite_table_select_row *source,
                                             size_t source_row_index);
static int copy_select_row_range_values(struct mylite_table_select_row *target,
                                        const struct mylite_table_select_row *source,
                                        struct mylite_select_table_range range,
                                        const struct mylite_select_plan *plan);
static int process_outer_joined_table_range_rows(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_table_rowset *range_rowsets, size_t range_count,
    const struct mylite_select_eval_callbacks *callbacks);
static int process_outer_joined_table_range_row(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_table_rowset *range_rowsets, const size_t *row_indexes,
    size_t range_count, const struct mylite_select_eval_callbacks *callbacks);
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
static int copy_joined_table_select_row_values(struct mylite_table_select_row *row,
                                               const struct mylite_select_table *table,
                                               const struct mylite_table_select_row *source);
static void clear_joined_table_select_row_values(struct mylite_table_select_row *row,
                                                 const struct mylite_select_table *table);
static int
process_joined_table_select_full_row(mylite_stmt *stmt,
                                     struct mylite_table_select_join_materialize_state *state,
                                     const struct mylite_table_select_row *row,
                                     const struct mylite_select_eval_callbacks *callbacks);
static int process_joined_table_select_nonaggregate_row(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks);
static int
materialize_aggregate_table_select_result(mylite_stmt *stmt,
                                          const struct mylite_select_eval_callbacks *callbacks);
static int scan_aggregate_table_select_groups(mylite_stmt *stmt,
                                              struct mylite_table_select_group **groups,
                                              size_t *group_count,
                                              const struct mylite_select_eval_callbacks *callbacks);
static int
append_finalized_table_select_groups(mylite_stmt *stmt, struct mylite_table_select_group *groups,
                                     size_t group_count,
                                     const struct mylite_select_eval_callbacks *callbacks);
static int
append_finalized_table_select_group(mylite_stmt *stmt, struct mylite_table_select_group *group,
                                    const struct mylite_select_eval_callbacks *callbacks);
static int
check_table_select_distinct_duplicate(mylite_stmt *stmt, struct mylite_table_select_row *row,
                                      bool *out_duplicate,
                                      const struct mylite_select_eval_callbacks *callbacks);
int mylite_select_materialize_table_result(mylite_stmt *stmt,
                                           const struct mylite_select_eval_callbacks *callbacks)
{
    int status = MYLITE_OK;

    if (stmt->select_result.materialized) {
        return MYLITE_OK;
    }
    if (mylite_select_plan_table_count(&stmt->select_plan) > 1U) {
        status = materialize_joined_table_select_result(stmt, callbacks);
    } else if (stmt->select_plan.has_group_by || stmt->select_plan.has_aggregate ||
               stmt->select_plan.has_having) {
        status = materialize_aggregate_table_select_result(stmt, callbacks);
    } else if (stmt->select_plan.order_key_count != 0U) {
        status = materialize_ordered_table_select_result(stmt, callbacks);
    } else {
        status = materialize_unordered_table_select_result(stmt, callbacks);
    }
    if (status == MYLITE_OK) {
        stmt->select_result.materialized = true;
    }
    return status;
}

static int
materialize_ordered_table_select_result(mylite_stmt *stmt,
                                        const struct mylite_select_eval_callbacks *callbacks)
{
    int status = mylite_select_eval_constant_predicate(stmt, callbacks);
    int rc = SQLITE_OK;

    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->select_constant_predicate_evaluated && !stmt->select_constant_predicate_matches) {
        return MYLITE_OK;
    }

    while ((rc = sqlite3_step(stmt->sqlite_stmt)) == SQLITE_ROW) {
        struct mylite_table_select_row row = {0};
        bool matches = false;

        status = mylite_select_copy_sqlite_row(stmt, &row);
        if (status != MYLITE_OK) {
            return status;
        }
        status = mylite_select_row_matches(stmt, &row, &matches, callbacks);
        if (status != MYLITE_OK) {
            mylite_select_row_deinit(&row);
            return status;
        }
        if (!matches) {
            mylite_select_row_deinit(&row);
            continue;
        }

        if (mylite_select_duplicate_mode_is_distinct(stmt->select_plan.duplicate_mode)) {
            bool duplicate = false;

            status = check_table_select_distinct_duplicate(stmt, &row, &duplicate, callbacks);
            if (status != MYLITE_OK) {
                mylite_select_row_deinit(&row);
                return status;
            }
            if (duplicate) {
                mylite_select_row_deinit(&row);
                continue;
            }
        }
        status = mylite_select_eval_order_values(stmt, &row, callbacks);
        if (status == MYLITE_OK) {
            status = mylite_select_result_append_row(stmt->database, &stmt->select_result, &row);
        }
        if (status != MYLITE_OK) {
            mylite_select_row_deinit(&row);
            return status;
        }
    }
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(stmt->database);
    }

    status =
        mylite_select_result_sort_rows(stmt->database, &stmt->select_result, &stmt->select_plan);
    if (status == MYLITE_OK) {
        status = mylite_select_result_apply_limit(&stmt->select_result, &stmt->select_plan.limit);
    }
    return status;
}

static int
materialize_unordered_table_select_result(mylite_stmt *stmt,
                                          const struct mylite_select_eval_callbacks *callbacks)
{
    struct mylite_unordered_table_select_append_state append_state = {0};
    bool distinct = mylite_select_duplicate_mode_is_distinct(stmt->select_plan.duplicate_mode);
    int status = MYLITE_OK;
    int rc = SQLITE_OK;

    if (stmt->select_plan.limit.has_limit && stmt->select_plan.limit.row_count == 0U) {
        return MYLITE_OK;
    }

    status = mylite_select_eval_constant_predicate(stmt, callbacks);
    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->select_constant_predicate_evaluated && !stmt->select_constant_predicate_matches) {
        return MYLITE_OK;
    }

    while ((rc = sqlite3_step(stmt->sqlite_stmt)) == SQLITE_ROW) {
        struct mylite_table_select_row row = {0};
        bool matches = false;

        status = mylite_select_copy_sqlite_row(stmt, &row);
        if (status != MYLITE_OK) {
            return status;
        }
        status = mylite_select_row_matches(stmt, &row, &matches, callbacks);
        if (status != MYLITE_OK) {
            mylite_select_row_deinit(&row);
            return status;
        }
        if (!matches) {
            mylite_select_row_deinit(&row);
            continue;
        }
        status = append_unordered_table_select_matched_row(stmt, &row, &append_state, distinct,
                                                           callbacks);
        if (status != MYLITE_OK) {
            mylite_select_row_deinit(&row);
            return status;
        }
        if (append_state.stop) {
            break;
        }
    }
    if (rc != SQLITE_DONE &&
        !mylite_select_limit_is_full(&stmt->select_plan.limit, stmt->select_result.row_count)) {
        return mylite_diagnostics_set_sqlite_error(stmt->database);
    }
    if (distinct) {
        return mylite_select_result_apply_limit(&stmt->select_result, &stmt->select_plan.limit);
    }
    return MYLITE_OK;
}

static int
append_unordered_table_select_matched_row(mylite_stmt *stmt, struct mylite_table_select_row *row,
                                          struct mylite_unordered_table_select_append_state *state,
                                          bool distinct,
                                          const struct mylite_select_eval_callbacks *callbacks)
{
    if (distinct) {
        return append_unordered_table_select_distinct_row(stmt, row, callbacks);
    }
    return append_unordered_table_select_limited_row(stmt, row, state);
}

static int
append_unordered_table_select_distinct_row(mylite_stmt *stmt, struct mylite_table_select_row *row,
                                           const struct mylite_select_eval_callbacks *callbacks)
{
    bool duplicate = false;
    int status = check_table_select_distinct_duplicate(stmt, row, &duplicate, callbacks);

    if (status != MYLITE_OK) {
        return status;
    }
    if (duplicate) {
        mylite_select_row_deinit(row);
        return MYLITE_OK;
    }
    return mylite_select_result_append_row(stmt->database, &stmt->select_result, row);
}

static int
append_unordered_table_select_limited_row(mylite_stmt *stmt, struct mylite_table_select_row *row,
                                          struct mylite_unordered_table_select_append_state *state)
{
    if (mylite_select_limit_row_is_kept(&stmt->select_plan.limit,
                                        (struct mylite_select_limit_position){
                                            .matched_row = state->matched_row,
                                            .kept_count = stmt->select_result.row_count,
                                        })) {
        int status = mylite_select_result_append_row(stmt->database, &stmt->select_result, row);

        if (status != MYLITE_OK) {
            return status;
        }
    } else {
        mylite_select_row_deinit(row);
    }
    if (state->matched_row != UINT64_MAX) {
        ++state->matched_row;
    }
    state->stop =
        mylite_select_limit_is_full(&stmt->select_plan.limit, stmt->select_result.row_count);
    return MYLITE_OK;
}

static int
materialize_joined_table_select_result(mylite_stmt *stmt,
                                       const struct mylite_select_eval_callbacks *callbacks)
{
    size_t table_count = mylite_select_plan_table_count(&stmt->select_plan);
    struct mylite_table_select_join_materialize_state state = {0};
    struct mylite_table_select_row row = {0};
    bool aggregate_query = (stmt->select_plan.has_group_by || stmt->select_plan.has_aggregate ||
                            stmt->select_plan.has_having) != 0;
    bool distinct = mylite_select_duplicate_mode_is_distinct(stmt->select_plan.duplicate_mode);
    int status = MYLITE_OK;

    if (mylite_select_plan_has_outer_join(&stmt->select_plan)) {
        return materialize_outer_joined_table_select_result(stmt, callbacks);
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
        status =
            append_finalized_table_select_groups(stmt, state.groups, state.group_count, callbacks);
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

static int
materialize_outer_joined_table_select_result(mylite_stmt *stmt,
                                             const struct mylite_select_eval_callbacks *callbacks)
{
    size_t table_count = mylite_select_plan_table_count(&stmt->select_plan);
    struct mylite_table_select_join_materialize_state state = {0};
    bool aggregate_query = (stmt->select_plan.has_group_by || stmt->select_plan.has_aggregate ||
                            stmt->select_plan.has_having) != 0;
    bool distinct = mylite_select_duplicate_mode_is_distinct(stmt->select_plan.duplicate_mode);
    int status = MYLITE_OK;

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

    status = mylite_select_load_join_rowsets(stmt, state.rowsets);
    if (status == MYLITE_OK) {
        status = scan_outer_joined_table_select_rows(stmt, &state, callbacks);
    }

    if (status == MYLITE_OK && aggregate_query && state.group_count == 0U &&
        !stmt->select_plan.has_group_by) {
        status = mylite_select_group_append_empty_implicit(stmt, &state.groups, &state.group_count);
    }
    if (status == MYLITE_OK && aggregate_query) {
        status =
            append_finalized_table_select_groups(stmt, state.groups, state.group_count, callbacks);
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
        return process_joined_table_select_full_row(stmt, state, row, callbacks);
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

static int
scan_outer_joined_table_select_rows(mylite_stmt *stmt,
                                    struct mylite_table_select_join_materialize_state *state,
                                    const struct mylite_select_eval_callbacks *callbacks)
{
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

        status = materialize_select_from_range_rowset(stmt, state, range,
                                                      &range_rowsets[range_index], callbacks);
    }
    if (status == MYLITE_OK) {
        status = process_outer_joined_table_range_rows(stmt, state, range_rowsets, range_count,
                                                       callbacks);
    }

    for (size_t index = 0U; index < range_count; ++index) {
        mylite_select_rowset_deinit(&range_rowsets[index]);
    }
    free(range_rowsets);
    return status;
}

static int materialize_select_from_range_rowset(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    struct mylite_select_table_range range, struct mylite_table_select_table_rowset *out_rowset,
    const struct mylite_select_eval_callbacks *callbacks)
{
    int status = materialize_select_base_range_rowset(stmt, state, range, out_rowset);

    for (size_t index = 0U; status == MYLITE_OK && index < stmt->select_plan.join_step_count;
         ++index) {
        const struct mylite_select_join_step *step = &stmt->select_plan.join_steps[index];

        if (!mylite_select_join_step_is_in_range(step, range)) {
            continue;
        }
        status = apply_select_join_step_to_rowset(stmt, state, step, out_rowset, callbacks);
    }
    return status;
}

static int materialize_select_base_range_rowset(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    struct mylite_select_table_range range, struct mylite_table_select_table_rowset *out_rowset)
{
    const struct mylite_select_table *table =
        mylite_select_plan_table_const(&stmt->select_plan, range.first_table);

    if (table == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    for (size_t row_index = 0U; row_index < state->rowsets[range.first_table].row_count;
         ++row_index) {
        struct mylite_table_select_row *row = NULL;
        int status = append_empty_joined_table_select_row(stmt, out_rowset, &row);

        if (status == MYLITE_OK) {
            status = copy_select_base_table_row_values(
                stmt->database, row, table, range.first_table,
                &state->rowsets[range.first_table].rows[row_index], row_index);
        }
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int apply_select_join_step_to_rowset(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_join_step *step, struct mylite_table_select_table_rowset *rowset,
    const struct mylite_select_eval_callbacks *callbacks)
{
    const struct mylite_table_select_table_rowset *right =
        &state->rowsets[step->right_range.first_table];
    struct mylite_table_select_table_rowset next = {0};
    bool *right_matched = NULL;
    int status = MYLITE_OK;

    if (step->join_type == MYLITE_SQL_AST_JOIN_RIGHT && right->row_count != 0U) {
        right_matched = calloc(right->row_count, sizeof(*right_matched));
        if (right_matched == NULL) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }

    status =
        append_select_join_step_matches(stmt, state, step, rowset, right_matched, &next, callbacks);
    if (status == MYLITE_OK && step->join_type == MYLITE_SQL_AST_JOIN_RIGHT) {
        status = append_select_null_extended_right_rows(stmt, step, right, right_matched, &next);
    }

    free(right_matched);
    if (status != MYLITE_OK) {
        mylite_select_rowset_deinit(&next);
        return status;
    }
    mylite_select_rowset_deinit(rowset);
    *rowset = next;
    return MYLITE_OK;
}

static int append_select_join_step_matches(mylite_stmt *stmt,
                                           struct mylite_table_select_join_materialize_state *state,
                                           const struct mylite_select_join_step *step,
                                           const struct mylite_table_select_table_rowset *left,
                                           bool *right_matched,
                                           struct mylite_table_select_table_rowset *out_rowset,
                                           const struct mylite_select_eval_callbacks *callbacks)
{
    const struct mylite_table_select_table_rowset *right =
        &state->rowsets[step->right_range.first_table];

    for (size_t left_index = 0U; left_index < left->row_count; ++left_index) {
        struct mylite_select_join_match_tracking tracking = {
            .left_matched = false,
            .right_matched = right_matched,
        };
        int status = append_select_join_step_left_matches(
            stmt, state, step, &left->rows[left_index], right, &tracking, out_rowset, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
        if (!tracking.left_matched && step->join_type == MYLITE_SQL_AST_JOIN_LEFT) {
            status =
                append_select_null_extended_left_row(stmt, &left->rows[left_index], out_rowset);
            if (status != MYLITE_OK) {
                return status;
            }
        }
    }
    return MYLITE_OK;
}

static int append_select_join_step_left_matches(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_join_step *step, const struct mylite_table_select_row *left_row,
    const struct mylite_table_select_table_rowset *right,
    struct mylite_select_join_match_tracking *tracking,
    struct mylite_table_select_table_rowset *out_rowset,
    const struct mylite_select_eval_callbacks *callbacks)
{
    tracking->left_matched = false;
    for (size_t right_index = 0U; right_index < right->row_count; ++right_index) {
        struct mylite_select_join_row_pair rows = {
            .left = left_row,
            .right = &right->rows[right_index],
            .right_index = right_index,
        };
        bool matches = false;
        int status = append_select_join_step_match(stmt, state, step, &rows, &matches, out_rowset,
                                                   callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
        if (matches) {
            tracking->left_matched = true;
            if (tracking->right_matched != NULL) {
                tracking->right_matched[right_index] = true;
            }
        }
    }
    return MYLITE_OK;
}

static int append_select_join_step_match(mylite_stmt *stmt,
                                         struct mylite_table_select_join_materialize_state *state,
                                         const struct mylite_select_join_step *step,
                                         const struct mylite_select_join_row_pair *rows,
                                         bool *out_matches,
                                         struct mylite_table_select_table_rowset *out_rowset,
                                         const struct mylite_select_eval_callbacks *callbacks)
{
    const struct mylite_select_table *right_table =
        mylite_select_plan_table_const(&stmt->select_plan, step->right_range.first_table);
    struct mylite_table_select_row candidate = {0};
    int status = mylite_select_row_copy(rows->left, &candidate);

    *out_matches = false;
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (right_table == NULL) {
        mylite_select_row_deinit(&candidate);
        return MYLITE_UNSUPPORTED;
    }

    status = copy_select_base_table_row_values(stmt->database, &candidate, right_table,
                                               step->right_range.first_table, rows->right,
                                               rows->right_index);
    if (status == MYLITE_OK) {
        status = mylite_select_join_step_conditions_match(stmt, state, &candidate, step,
                                                          out_matches, callbacks);
    }
    if (status == MYLITE_OK && *out_matches) {
        status = mylite_select_rowset_append_row(stmt->database, out_rowset, &candidate);
    }
    mylite_select_row_deinit(&candidate);
    return status;
}

static int append_select_null_extended_left_row(mylite_stmt *stmt,
                                                const struct mylite_table_select_row *left_row,
                                                struct mylite_table_select_table_rowset *out_rowset)
{
    return mylite_select_rowset_append_row_copy(stmt->database, out_rowset, left_row);
}

static int append_select_null_extended_right_rows(
    mylite_stmt *stmt, const struct mylite_select_join_step *step,
    const struct mylite_table_select_table_rowset *right, const bool *right_matched,
    struct mylite_table_select_table_rowset *out_rowset)
{
    for (size_t right_index = 0U; right_index < right->row_count; ++right_index) {
        if (right_matched != NULL && right_matched[right_index]) {
            continue;
        }
        int status = append_select_null_extended_right_row(stmt, step, &right->rows[right_index],
                                                           right_index, out_rowset);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int
append_select_null_extended_right_row(mylite_stmt *stmt, const struct mylite_select_join_step *step,
                                      const struct mylite_table_select_row *right_row,
                                      size_t right_row_index,
                                      struct mylite_table_select_table_rowset *out_rowset)
{
    const struct mylite_select_table *right_table =
        mylite_select_plan_table_const(&stmt->select_plan, step->right_range.first_table);
    struct mylite_table_select_row *row = NULL;
    int status = append_empty_joined_table_select_row(stmt, out_rowset, &row);

    if (status == MYLITE_OK && right_table == NULL) {
        status = MYLITE_UNSUPPORTED;
    }
    if (status == MYLITE_OK) {
        status = copy_select_base_table_row_values(stmt->database, row, right_table,
                                                   step->right_range.first_table, right_row,
                                                   right_row_index);
    }
    return status;
}

static int append_empty_joined_table_select_row(mylite_stmt *stmt,
                                                struct mylite_table_select_table_rowset *rowset,
                                                struct mylite_table_select_row **out_row)
{
    struct mylite_table_select_row row = {
        .value_count = mylite_select_plan_column_count(&stmt->select_plan),
        .source_row_index_count = mylite_select_plan_table_count(&stmt->select_plan),
    };
    int status = MYLITE_OK;

    *out_row = NULL;
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
    if (status != MYLITE_OK) {
        mylite_select_row_deinit(&row);
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return status;
    }
    for (size_t index = 0U; index < row.source_row_index_count; ++index) {
        row.source_row_indexes[index] = SIZE_MAX;
    }

    status = mylite_select_rowset_append_row(stmt->database, rowset, &row);
    if (status != MYLITE_OK) {
        mylite_select_row_deinit(&row);
        return status;
    }
    *out_row = &rowset->rows[rowset->row_count - 1U];
    return MYLITE_OK;
}

static int copy_select_base_table_row_values(mylite_db *database,
                                             struct mylite_table_select_row *row,
                                             const struct mylite_select_table *table,
                                             size_t table_index,
                                             const struct mylite_table_select_row *source,
                                             size_t source_row_index)
{
    if (table == NULL || row->source_row_index_count <= table_index) {
        return MYLITE_UNSUPPORTED;
    }
    for (size_t index = 0U; index < table->column_count; ++index) {
        size_t column_index = table->first_column_index + index;

        if (column_index >= row->value_count || index >= source->value_count) {
            return MYLITE_UNSUPPORTED;
        }
        mylite_expression_value_deinit(&row->values[column_index]);
        if (mylite_expression_value_copy(&source->values[index], &row->values[column_index]) != 0) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    row->source_row_indexes[table_index] = source_row_index;
    return MYLITE_OK;
}

static int copy_select_row_range_values(struct mylite_table_select_row *target,
                                        const struct mylite_table_select_row *source,
                                        struct mylite_select_table_range range,
                                        const struct mylite_select_plan *plan)
{
    size_t last_table = range.first_table + range.table_count;

    for (size_t table_index = range.first_table; table_index < last_table; ++table_index) {
        const struct mylite_select_table *table = mylite_select_plan_table_const(plan, table_index);

        if (table == NULL || table_index >= target->source_row_index_count ||
            table_index >= source->source_row_index_count) {
            return MYLITE_UNSUPPORTED;
        }
        target->source_row_indexes[table_index] = source->source_row_indexes[table_index];
        for (size_t column = 0U; column < table->column_count; ++column) {
            size_t column_index = table->first_column_index + column;

            if (column_index >= target->value_count || column_index >= source->value_count) {
                return MYLITE_UNSUPPORTED;
            }
            mylite_expression_value_deinit(&target->values[column_index]);
            if (mylite_expression_value_copy(&source->values[column_index],
                                             &target->values[column_index]) != 0) {
                return MYLITE_NOMEM;
            }
        }
    }
    return MYLITE_OK;
}

static int process_outer_joined_table_range_rows(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_table_rowset *range_rowsets, size_t range_count,
    const struct mylite_select_eval_callbacks *callbacks)
{
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
        status = process_outer_joined_table_range_row(stmt, state, range_rowsets, row_indexes,
                                                      range_count, callbacks);
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
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_table_rowset *range_rowsets, const size_t *row_indexes,
    size_t range_count, const struct mylite_select_eval_callbacks *callbacks)
{
    struct mylite_table_select_row row = {0};
    int status = MYLITE_OK;

    row.value_count = mylite_select_plan_column_count(&stmt->select_plan);
    row.source_row_index_count = mylite_select_plan_table_count(&stmt->select_plan);
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

        status = copy_select_row_range_values(
            &row, &range_rowsets[range_index].rows[row_indexes[range_index]], range,
            &stmt->select_plan);
    }
    if (status == MYLITE_OK) {
        status = process_joined_table_select_full_row(stmt, state, &row, callbacks);
    }
    mylite_select_row_deinit(&row);
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
    int status = copy_joined_table_select_row_values(
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
        status = process_joined_table_select_full_row(stmt, state, scan->row, callbacks);
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
    clear_joined_table_select_row_values(scan->row, table);
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
            clear_joined_table_select_row_values(scan->row, table);
        }
    }
}

static int copy_joined_table_select_row_values(struct mylite_table_select_row *row,
                                               const struct mylite_select_table *table,
                                               const struct mylite_table_select_row *source)
{
    for (size_t index = 0U; index < table->column_count; ++index) {
        size_t column_index = table->first_column_index + index;

        if (column_index >= row->value_count || index >= source->value_count ||
            mylite_expression_value_copy(&source->values[index], &row->values[column_index]) != 0) {
            for (size_t copied = 0U; copied < index; ++copied) {
                mylite_expression_value_deinit(&row->values[table->first_column_index + copied]);
            }
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

static void clear_joined_table_select_row_values(struct mylite_table_select_row *row,
                                                 const struct mylite_select_table *table)
{
    for (size_t index = 0U; index < table->column_count; ++index) {
        size_t column_index = table->first_column_index + index;

        if (column_index < row->value_count) {
            mylite_expression_value_deinit(&row->values[column_index]);
        }
    }
}

static int process_joined_table_select_full_row(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_row *row, const struct mylite_select_eval_callbacks *callbacks)
{
    bool matches = false;
    bool aggregate_query = (stmt->select_plan.has_group_by || stmt->select_plan.has_aggregate ||
                            stmt->select_plan.has_having) != 0;
    int status = MYLITE_OK;

    if (stmt->select_predicate == NULL || stmt->select_constant_predicate_evaluated) {
        matches = true;
    } else {
        status = mylite_select_eval_row_predicate(stmt, row, callbacks, &matches);
    }
    if (status != MYLITE_OK || !matches) {
        return status;
    }
    if (!aggregate_query) {
        return process_joined_table_select_nonaggregate_row(stmt, state, row, callbacks);
    }

    struct mylite_table_select_group *group = NULL;

    status =
        mylite_select_group_find(stmt, state->groups, state->group_count, row, callbacks, &group);
    if (status == MYLITE_OK && group == NULL) {
        status = mylite_select_group_append(stmt, &state->groups, &state->group_count, row,
                                            callbacks, &group);
    }
    if (status == MYLITE_OK) {
        status = mylite_select_group_update(stmt, group, row, callbacks);
    }
    return status;
}

static int process_joined_table_select_nonaggregate_row(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_row *row, const struct mylite_select_eval_callbacks *callbacks)
{
    bool distinct = mylite_select_duplicate_mode_is_distinct(stmt->select_plan.duplicate_mode);

    if (stmt->select_plan.order_key_count != 0U || distinct) {
        struct mylite_table_select_row copy = {0};
        int status = mylite_select_row_copy(row, &copy);
        bool duplicate = false;

        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        }
        if (status == MYLITE_OK && distinct) {
            status = check_table_select_distinct_duplicate(stmt, &copy, &duplicate, callbacks);
        }
        if (status == MYLITE_OK && duplicate) {
            mylite_select_row_deinit(&copy);
            return MYLITE_OK;
        }
        if (status == MYLITE_OK && stmt->select_plan.order_key_count != 0U) {
            status = mylite_select_eval_order_values(stmt, &copy, callbacks);
        }
        if (status == MYLITE_OK) {
            status = mylite_select_result_append_row(stmt->database, &stmt->select_result, &copy);
        }
        mylite_select_row_deinit(&copy);
        return status;
    }

    if (mylite_select_limit_row_is_kept(&stmt->select_plan.limit,
                                        (struct mylite_select_limit_position){
                                            .matched_row = state->matched_row,
                                            .kept_count = stmt->select_result.row_count,
                                        })) {
        int status =
            mylite_select_result_append_row_copy(stmt->database, &stmt->select_result, row);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (state->matched_row != UINT64_MAX) {
        ++state->matched_row;
    }
    if (mylite_select_limit_is_full(&stmt->select_plan.limit, stmt->select_result.row_count)) {
        state->stop = true;
    }
    return MYLITE_OK;
}

static int
materialize_aggregate_table_select_result(mylite_stmt *stmt,
                                          const struct mylite_select_eval_callbacks *callbacks)
{
    struct mylite_table_select_group *groups = NULL;
    size_t group_count = 0U;
    int status = scan_aggregate_table_select_groups(stmt, &groups, &group_count, callbacks);

    if (status == MYLITE_OK && group_count == 0U && !stmt->select_plan.has_group_by) {
        status = mylite_select_group_append_empty_implicit(stmt, &groups, &group_count);
    }
    if (status == MYLITE_OK) {
        status = append_finalized_table_select_groups(stmt, groups, group_count, callbacks);
    }
    if (status == MYLITE_OK && stmt->select_plan.order_key_count != 0U) {
        status = mylite_select_result_sort_rows(stmt->database, &stmt->select_result,
                                                &stmt->select_plan);
    }
    if (status == MYLITE_OK) {
        status = mylite_select_result_apply_limit(&stmt->select_result, &stmt->select_plan.limit);
    }

    mylite_select_groups_deinit(groups, group_count);
    return status;
}

static int scan_aggregate_table_select_groups(mylite_stmt *stmt,
                                              struct mylite_table_select_group **groups,
                                              size_t *group_count,
                                              const struct mylite_select_eval_callbacks *callbacks)
{
    int status = mylite_select_eval_constant_predicate(stmt, callbacks);
    int rc = SQLITE_OK;

    *groups = NULL;
    *group_count = 0U;
    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->select_constant_predicate_evaluated && !stmt->select_constant_predicate_matches) {
        return MYLITE_OK;
    }

    while ((rc = sqlite3_step(stmt->sqlite_stmt)) == SQLITE_ROW) {
        struct mylite_table_select_group *group = NULL;
        struct mylite_table_select_row row = {0};
        bool matches = false;

        status = mylite_select_copy_sqlite_row(stmt, &row);
        if (status != MYLITE_OK) {
            return status;
        }
        status = mylite_select_row_matches(stmt, &row, &matches, callbacks);
        if (status != MYLITE_OK) {
            mylite_select_row_deinit(&row);
            return status;
        }
        if (!matches) {
            mylite_select_row_deinit(&row);
            continue;
        }

        status = mylite_select_group_find(stmt, *groups, *group_count, &row, callbacks, &group);
        if (status == MYLITE_OK && group == NULL) {
            status = mylite_select_group_append(stmt, groups, group_count, &row, callbacks, &group);
        }
        if (status == MYLITE_OK) {
            status = mylite_select_group_update(stmt, group, &row, callbacks);
        }
        mylite_select_row_deinit(&row);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (rc != SQLITE_DONE) {
        status = mylite_diagnostics_set_sqlite_error(stmt->database);
    }
    return status;
}

static int
append_finalized_table_select_groups(mylite_stmt *stmt, struct mylite_table_select_group *groups,
                                     size_t group_count,
                                     const struct mylite_select_eval_callbacks *callbacks)
{
    for (size_t index = 0U; index < group_count; ++index) {
        int status = append_finalized_table_select_group(stmt, &groups[index], callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int append_finalized_table_select_group(mylite_stmt *stmt,
                                               struct mylite_table_select_group *group,
                                               const struct mylite_select_eval_callbacks *callbacks)
{
    struct mylite_table_select_row row = {0};
    bool having_matches = true;
    bool duplicate = false;
    int status = mylite_select_group_finalize(stmt, group, &row);

    if (status == MYLITE_OK) {
        status = mylite_select_eval_having(stmt, &row, callbacks, &having_matches);
    }
    if (status == MYLITE_OK && having_matches &&
        mylite_select_duplicate_mode_is_distinct(stmt->select_plan.duplicate_mode)) {
        status = check_table_select_distinct_duplicate(stmt, &row, &duplicate, callbacks);
    }
    if (status == MYLITE_OK && duplicate) {
        mylite_select_row_deinit(&row);
        return MYLITE_OK;
    }
    if (status == MYLITE_OK && having_matches && stmt->select_plan.order_key_count != 0U) {
        status = mylite_select_eval_order_values(stmt, &row, callbacks);
    }
    if (status == MYLITE_OK && having_matches) {
        status = mylite_select_result_append_row(stmt->database, &stmt->select_result, &row);
    }
    mylite_select_row_deinit(&row);
    return status;
}

static int
check_table_select_distinct_duplicate(mylite_stmt *stmt, struct mylite_table_select_row *row,
                                      bool *out_duplicate,
                                      const struct mylite_select_eval_callbacks *callbacks)
{
    int status = mylite_select_eval_materialize_output_values(stmt, row, callbacks);

    *out_duplicate = false;
    if (status != MYLITE_OK) {
        return status;
    }
    *out_duplicate = mylite_select_result_distinct_row_exists(
        &stmt->select_result, &stmt->select_plan, &stmt->result_metadata, row);
    return MYLITE_OK;
}
