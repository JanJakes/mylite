#include "mylite_select_join_range_rowset.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_join_rows.h"
#include "mylite_select_row_match.h"
#include "mylite_select_rowset.h"

#include <stdbool.h>
#include <stdlib.h>

static int materialize_select_base_range_rowset(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_table_range *range,
    struct mylite_table_select_table_rowset *out_rowset
);

static int apply_select_join_step_to_rowset(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_join_step *step,
    struct mylite_table_select_table_rowset *rowset,
    const struct mylite_select_eval_callbacks *callbacks
);

static int append_select_join_step_matches(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_join_step *step,
    const struct mylite_table_select_table_rowset *left,
    bool *right_matched,
    struct mylite_table_select_table_rowset *out_rowset,
    const struct mylite_select_eval_callbacks *callbacks
);

static int append_select_join_step_left_matches(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_join_step *step,
    const struct mylite_table_select_row *left_row,
    const struct mylite_table_select_table_rowset *right,
    struct mylite_select_join_match_tracking *tracking,
    struct mylite_table_select_table_rowset *out_rowset,
    const struct mylite_select_eval_callbacks *callbacks
);

static int append_select_join_step_match(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_join_step *step,
    const struct mylite_select_join_row_pair *rows,
    bool *out_matches,
    struct mylite_table_select_table_rowset *out_rowset,
    const struct mylite_select_eval_callbacks *callbacks
);

int mylite_select_join_range_rowset_materialize(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_table_range *range,
    struct mylite_table_select_table_rowset *out_rowset,
    const struct mylite_select_eval_callbacks *callbacks
) {
    int status = materialize_select_base_range_rowset(stmt, state, range, out_rowset);

    for (size_t index = 0U; status == MYLITE_OK && index < stmt->select_plan.join_step_count;
         ++index) {
        const struct mylite_select_join_step *step = &stmt->select_plan.join_steps[index];

        if (!mylite_select_join_step_is_in_range(step, *range)) {
            continue;
        }
        status = apply_select_join_step_to_rowset(stmt, state, step, out_rowset, callbacks);
    }
    return status;
}

static int materialize_select_base_range_rowset(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_table_range *range,
    struct mylite_table_select_table_rowset *out_rowset
) {
    const struct mylite_select_table *table =
        mylite_select_plan_table_const(&stmt->select_plan, range->first_table);

    if (table == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    for (size_t row_index = 0U; row_index < state->rowsets[range->first_table].row_count;
         ++row_index) {
        struct mylite_table_select_row *row = NULL;
        int status = mylite_select_join_rowset_append_empty(stmt, out_rowset, &row);

        if (status == MYLITE_OK) {
            status = mylite_select_join_row_copy_base_table_values(
                stmt->database,
                row,
                table,
                range->first_table,
                &state->rowsets[range->first_table].rows[row_index],
                row_index
            );
        }
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int apply_select_join_step_to_rowset(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_join_step *step,
    struct mylite_table_select_table_rowset *rowset,
    const struct mylite_select_eval_callbacks *callbacks
) {
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
        status = mylite_select_join_rowset_append_null_extended_right_unmatched(
            stmt,
            step,
            right,
            right_matched,
            &next
        );
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

static int append_select_join_step_matches(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_join_step *step,
    const struct mylite_table_select_table_rowset *left,
    bool *right_matched,
    struct mylite_table_select_table_rowset *out_rowset,
    const struct mylite_select_eval_callbacks *callbacks
) {
    const struct mylite_table_select_table_rowset *right =
        &state->rowsets[step->right_range.first_table];

    for (size_t left_index = 0U; left_index < left->row_count; ++left_index) {
        struct mylite_select_join_match_tracking tracking = {
            .left_matched = false,
            .right_matched = right_matched,
        };
        int status = append_select_join_step_left_matches(
            stmt,
            state,
            step,
            &left->rows[left_index],
            right,
            &tracking,
            out_rowset,
            callbacks
        );

        if (status != MYLITE_OK) {
            return status;
        }
        if (!tracking.left_matched && step->join_type == MYLITE_SQL_AST_JOIN_LEFT) {
            status = mylite_select_join_rowset_append_null_extended_left(
                stmt,
                &left->rows[left_index],
                out_rowset
            );
            if (status != MYLITE_OK) {
                return status;
            }
        }
    }
    return MYLITE_OK;
}

static int append_select_join_step_left_matches(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_join_step *step,
    const struct mylite_table_select_row *left_row,
    const struct mylite_table_select_table_rowset *right,
    struct mylite_select_join_match_tracking *tracking,
    struct mylite_table_select_table_rowset *out_rowset,
    const struct mylite_select_eval_callbacks *callbacks
) {
    tracking->left_matched = false;
    for (size_t right_index = 0U; right_index < right->row_count; ++right_index) {
        struct mylite_select_join_row_pair rows = {
            .left = left_row,
            .right = &right->rows[right_index],
            .right_index = right_index,
        };
        bool matches = false;
        int status = append_select_join_step_match(
            stmt,
            state,
            step,
            &rows,
            &matches,
            out_rowset,
            callbacks
        );

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

static int append_select_join_step_match(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_join_step *step,
    const struct mylite_select_join_row_pair *rows,
    bool *out_matches,
    struct mylite_table_select_table_rowset *out_rowset,
    const struct mylite_select_eval_callbacks *callbacks
) {
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

    status = mylite_select_join_row_copy_base_table_values(
        stmt->database,
        &candidate,
        right_table,
        step->right_range.first_table,
        rows->right,
        rows->right_index
    );
    if (status == MYLITE_OK) {
        status = mylite_select_join_step_conditions_match(
            stmt,
            state,
            &candidate,
            step,
            out_matches,
            callbacks
        );
    }
    if (status == MYLITE_OK && *out_matches) {
        status = mylite_select_rowset_append_row(stmt->database, out_rowset, &candidate);
    }
    mylite_select_row_deinit(&candidate);
    return status;
}
