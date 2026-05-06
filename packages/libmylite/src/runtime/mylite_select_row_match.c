#include "mylite_select_row_match.h"

#include "mylite_expression.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_eval.h"
#include "mylite_select_join_cache.h"

#include <stddef.h>

static int evaluate_table_select_join_conditions(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
);

static int evaluate_table_select_using_conditions(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
);

static int evaluate_table_select_join_predicates(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
);

static int evaluate_table_select_join_stage_conditions_uncached(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    size_t available_table_count,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
);

static int evaluate_table_select_using_stage_conditions(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    size_t available_table_count,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
);

static int evaluate_table_select_join_stage_predicates(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    size_t available_table_count,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
);

static int evaluate_table_select_using_column(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    const struct mylite_select_join_using_column *column,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
);

int mylite_select_row_matches(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
) {
    *out_matches = true;
    int status = evaluate_table_select_join_conditions(stmt, row, out_matches, callbacks);

    if (status != MYLITE_OK || !*out_matches) {
        return status;
    }
    if (stmt->select_predicate == NULL || stmt->select_constant_predicate_evaluated) {
        return MYLITE_OK;
    }
    return mylite_select_eval_row_predicate(stmt, row, callbacks, out_matches);
}

int mylite_select_join_step_conditions_match(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_row *row,
    const struct mylite_select_join_step *step,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
) {
    size_t available_table_count = step->joined_range.first_table + step->joined_range.table_count;
    struct mylite_select_table_range cache_range = {0};
    struct mylite_table_select_join_condition_cache_lookup lookup = {
        .found = false,
        .matches = false,
    };
    int status = MYLITE_OK;

    if (!mylite_select_join_cache_stage_range(
            stmt->database,
            &stmt->select_plan,
            available_table_count,
            &cache_range
        )) {
        return evaluate_table_select_join_stage_conditions_uncached(
            stmt,
            row,
            available_table_count,
            out_matches,
            callbacks
        );
    }

    status =
        mylite_select_join_cache_lookup_row(&state->condition_cache, row, cache_range, &lookup);
    if (status != MYLITE_OK) {
        return status;
    }
    if (lookup.found) {
        *out_matches = lookup.matches;
        return MYLITE_OK;
    }

    status = evaluate_table_select_join_stage_conditions_uncached(
        stmt,
        row,
        available_table_count,
        out_matches,
        callbacks
    );
    if (status == MYLITE_OK) {
        status = mylite_select_join_cache_store_row(
            stmt->database,
            &state->condition_cache,
            row,
            cache_range,
            *out_matches
        );
    }
    return status;
}

int mylite_select_join_stage_conditions_match(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_join_scan_state *scan,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
) {
    struct mylite_select_table_range cache_range = {0};
    struct mylite_table_select_join_condition_cache_lookup lookup = {
        .found = false,
        .matches = false,
    };
    int status = MYLITE_OK;

    if (!mylite_select_join_cache_stage_range(
            stmt->database,
            &stmt->select_plan,
            scan->table_index + 1U,
            &cache_range
        )) {
        return evaluate_table_select_join_stage_conditions_uncached(
            stmt,
            scan->row,
            scan->table_index + 1U,
            out_matches,
            callbacks
        );
    }

    status =
        mylite_select_join_cache_lookup_scan(&state->condition_cache, scan, cache_range, &lookup);
    if (status != MYLITE_OK) {
        return status;
    }
    if (lookup.found) {
        *out_matches = lookup.matches;
        return MYLITE_OK;
    }

    status = evaluate_table_select_join_stage_conditions_uncached(
        stmt,
        scan->row,
        scan->table_index + 1U,
        out_matches,
        callbacks
    );
    if (status == MYLITE_OK) {
        status = mylite_select_join_cache_store_scan(
            stmt->database,
            &state->condition_cache,
            scan,
            cache_range,
            *out_matches
        );
    }
    return status;
}

static int evaluate_table_select_join_conditions(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
) {
    int status = evaluate_table_select_using_conditions(stmt, row, out_matches, callbacks);

    if (status != MYLITE_OK || !*out_matches) {
        return status;
    }
    return evaluate_table_select_join_predicates(stmt, row, out_matches, callbacks);
}

static int evaluate_table_select_using_conditions(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
) {
    *out_matches = true;
    for (size_t index = 0U; index < stmt->select_plan.using_column_count; ++index) {
        const struct mylite_select_join_using_column *column =
            &stmt->select_plan.using_columns[index];
        const struct mylite_expression_value *left = NULL;
        const struct mylite_expression_value *right = NULL;

        if (row == NULL || column->left_column_index >= row->value_count ||
            column->right_column_index >= row->value_count) {
            return callbacks->set_expression_eval_error(stmt);
        }
        left = &row->values[column->left_column_index];
        right = &row->values[column->right_column_index];
        if (left->kind == MYLITE_EXPRESSION_VALUE_NULL ||
            right->kind == MYLITE_EXPRESSION_VALUE_NULL ||
            mylite_select_compare_values(left, right) != 0) {
            *out_matches = false;
            return MYLITE_OK;
        }
    }
    return MYLITE_OK;
}

static int evaluate_table_select_join_predicates(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
) {
    *out_matches = true;
    for (size_t index = 0U; index < stmt->select_plan.join_predicate_count; ++index) {
        int status = mylite_select_eval_expression_predicate(
            stmt,
            row,
            stmt->select_plan.join_predicates[index].expression,
            callbacks,
            out_matches
        );

        if (status != MYLITE_OK || !*out_matches) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int evaluate_table_select_join_stage_conditions_uncached(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    size_t available_table_count,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
) {
    int status = evaluate_table_select_using_stage_conditions(
        stmt,
        row,
        available_table_count,
        out_matches,
        callbacks
    );

    if (status != MYLITE_OK || !*out_matches) {
        return status;
    }
    return evaluate_table_select_join_stage_predicates(
        stmt,
        row,
        available_table_count,
        out_matches,
        callbacks
    );
}

static int evaluate_table_select_using_stage_conditions(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    size_t available_table_count,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
) {
    *out_matches = true;
    for (size_t index = 0U; index < stmt->select_plan.using_column_count; ++index) {
        const struct mylite_select_join_using_column *column =
            &stmt->select_plan.using_columns[index];

        if (column->first_table + column->table_count != available_table_count) {
            continue;
        }
        int status = evaluate_table_select_using_column(stmt, row, column, out_matches, callbacks);

        if (status != MYLITE_OK || !*out_matches) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int evaluate_table_select_join_stage_predicates(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    size_t available_table_count,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
) {
    *out_matches = true;
    for (size_t index = 0U; index < stmt->select_plan.join_predicate_count; ++index) {
        const struct mylite_select_join_predicate *predicate =
            &stmt->select_plan.join_predicates[index];

        if (predicate->first_table + predicate->table_count != available_table_count) {
            continue;
        }
        int status = mylite_select_eval_expression_predicate(
            stmt,
            row,
            predicate->expression,
            callbacks,
            out_matches
        );

        if (status != MYLITE_OK || !*out_matches) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int evaluate_table_select_using_column(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    const struct mylite_select_join_using_column *column,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
) {
    const struct mylite_expression_value *left = NULL;
    const struct mylite_expression_value *right = NULL;
    int comparison = 0;

    *out_matches = false;
    if (row == NULL || column->left_column_index >= row->value_count ||
        column->right_column_index >= row->value_count) {
        return callbacks->set_expression_eval_error(stmt);
    }

    left = &row->values[column->left_column_index];
    right = &row->values[column->right_column_index];
    if (left->kind == MYLITE_EXPRESSION_VALUE_NULL || right->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return MYLITE_OK;
    }
    if (mylite_expression_value_compare(left, right, &stmt->database->warnings, &comparison) != 0) {
        return callbacks->set_expression_eval_error(stmt);
    }
    *out_matches = comparison == 0;
    return MYLITE_OK;
}
