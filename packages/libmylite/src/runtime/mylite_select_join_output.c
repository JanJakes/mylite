#include "mylite_select_join_output.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_eval.h"
#include "mylite_select_group.h"
#include "mylite_select_materialize_common.h"
#include "mylite_select_rowset.h"

#include <stdbool.h>
#include <stdint.h>

static int mylite_select_join_materialize_nonaggregate_row(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks
);

int mylite_select_join_materialize_row(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks
) {
    bool matches = false;
    bool aggregate_query = (stmt->select_plan.has_group_by || stmt->select_plan.has_aggregate) != 0;
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
        status = mylite_select_eval_having(stmt, row, callbacks, &matches);
        if (status != MYLITE_OK || !matches) {
            return status;
        }
        return mylite_select_join_materialize_nonaggregate_row(stmt, state, row, callbacks);
    }

    struct mylite_table_select_group *group = NULL;

    status =
        mylite_select_group_find(stmt, state->groups, state->group_count, row, callbacks, &group);
    if (status == MYLITE_OK && group == NULL) {
        status = mylite_select_group_append(
            stmt,
            &state->groups,
            &state->group_count,
            row,
            callbacks,
            &group
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_select_group_update(stmt, group, row, callbacks);
    }
    return status;
}

static int mylite_select_join_materialize_nonaggregate_row(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks
) {
    bool distinct = mylite_select_duplicate_mode_is_distinct(stmt->select_plan.duplicate_mode);

    if (stmt->select_plan.order_key_count != 0U || distinct) {
        struct mylite_table_select_row copy = {0};
        int status = mylite_select_row_copy(row, &copy);
        bool duplicate = false;

        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        }
        if (status == MYLITE_OK && distinct) {
            status = mylite_select_materialize_check_distinct_duplicate(
                stmt,
                &copy,
                &duplicate,
                callbacks
            );
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

    if (mylite_select_limit_row_is_kept(
            &stmt->select_plan.limit,
            (struct mylite_select_limit_position){
                .matched_row = state->matched_row,
                .kept_count = stmt->select_result.row_count,
            }
        )) {
        int status =
            mylite_select_result_append_row_copy(stmt->database, &stmt->select_result, row);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (state->matched_row != UINT64_MAX) {
        ++state->matched_row;
    }
    if (!stmt->select_plan.calc_found_rows &&
        mylite_select_limit_is_full(&stmt->select_plan.limit, stmt->select_result.row_count)) {
        state->stop = true;
    }
    return MYLITE_OK;
}
