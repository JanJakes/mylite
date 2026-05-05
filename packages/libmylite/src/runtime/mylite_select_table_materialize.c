#include "mylite_select_table_materialize.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_eval.h"
#include "mylite_select_materialize_common.h"
#include "mylite_select_row_loader.h"
#include "mylite_select_row_match.h"
#include "mylite_select_rowset.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>

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

int mylite_select_materialize_single_table_result(
    mylite_stmt *stmt, const struct mylite_select_eval_callbacks *callbacks)
{
    if (stmt->select_plan.order_key_count != 0U) {
        return materialize_ordered_table_select_result(stmt, callbacks);
    }
    return materialize_unordered_table_select_result(stmt, callbacks);
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
        stmt->found_rows = 0U;
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

            status = mylite_select_materialize_check_distinct_duplicate(stmt, &row, &duplicate,
                                                                        callbacks);
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
        stmt->found_rows = stmt->select_result.row_count;
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

    if (!stmt->select_plan.calc_found_rows && stmt->select_plan.limit.has_limit &&
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
        stmt->found_rows = stmt->select_result.row_count;
        return mylite_select_result_apply_limit(&stmt->select_result, &stmt->select_plan.limit);
    }
    stmt->found_rows = append_state.matched_row;
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
    int status =
        mylite_select_materialize_check_distinct_duplicate(stmt, row, &duplicate, callbacks);

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
    if (stmt->select_plan.calc_found_rows) {
        state->stop = false;
    } else {
        state->stop =
            mylite_select_limit_is_full(&stmt->select_plan.limit, stmt->select_result.row_count);
    }
    return MYLITE_OK;
}
