#include "mylite_select_eval.h"

#include "mylite_diagnostics.h"
#include "mylite_expression.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_eval_expression.h"
#include "mylite_select_rowset.h"

#include <stdlib.h>

int mylite_select_eval_group_key(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    const struct mylite_select_group_key *group_key,
    const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_expression_value *out_value
) {
    if (group_key->kind == MYLITE_SELECT_GROUP_KEY_OUTPUT) {
        return mylite_select_eval_cached_output_value(
            stmt,
            row,
            group_key->output_index,
            callbacks,
            out_value
        );
    }

    struct mylite_table_select_eval_context user_context = {0};
    struct mylite_expression_eval_context context = {0};
    int status = 0;

    mylite_select_eval_context_init(&user_context, stmt, row, callbacks, false, false);
    mylite_select_eval_expression_context_init(&context, &user_context);
    status = mylite_expression_eval_with_context(
        group_key->expression,
        &context,
        &stmt->database->warnings,
        out_value
    );
    if (status != 0) {
        return mylite_select_eval_map_expression_status(stmt, status, callbacks);
    }
    return MYLITE_OK;
}

int mylite_select_eval_having(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    bool *out_matches
) {
    struct mylite_expression_value value = {0};
    int truth = -1;
    int status = 0;

    *out_matches = true;
    if (stmt->select_plan.having_expression == NULL) {
        return MYLITE_OK;
    }

    struct mylite_table_select_eval_context user_context = {0};
    struct mylite_expression_eval_context context = {0};

    mylite_select_eval_context_init(&user_context, stmt, row, callbacks, false, true);
    mylite_select_eval_expression_context_init(&context, &user_context);
    status = mylite_expression_eval_with_context(
        stmt->select_plan.having_expression,
        &context,
        &stmt->database->warnings,
        &value
    );
    if (status == 0) {
        status = mylite_expression_value_truth(&value, &stmt->database->warnings, &truth);
    }
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return mylite_select_eval_map_expression_status(stmt, status, callbacks);
    }

    *out_matches = truth == 1;
    return MYLITE_OK;
}

int mylite_select_eval_aggregate_argument(
    mylite_stmt *stmt,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_expression_value *out_value
) {
    struct mylite_table_select_eval_context user_context = {0};
    struct mylite_expression_eval_context context = {0};
    int status = 0;

    mylite_select_eval_context_init(&user_context, stmt, row, callbacks, false, false);
    mylite_select_eval_expression_context_init(&context, &user_context);
    status = mylite_expression_eval_with_context(
        binding->argument,
        &context,
        &stmt->database->warnings,
        out_value
    );
    if (status != 0) {
        return mylite_select_eval_map_expression_status(stmt, status, callbacks);
    }
    return MYLITE_OK;
}

int mylite_select_eval_order_values(
    mylite_stmt *stmt,
    struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks
) {
    size_t order_key_count = stmt->select_plan.order_key_count;

    row->order_values = calloc(order_key_count, sizeof(*row->order_values));
    if (row->order_values == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    row->order_value_count = order_key_count;

    for (size_t index = 0U; index < order_key_count; ++index) {
        const struct mylite_select_order_key *order_key = &stmt->select_plan.order_keys[index];
        int status = MYLITE_OK;

        if (order_key->kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
            status = mylite_select_eval_cached_output_value(
                stmt,
                row,
                order_key->output_index,
                callbacks,
                &row->order_values[index]
            );
        } else {
            struct mylite_table_select_eval_context user_context = {0};
            struct mylite_expression_eval_context context = {0};
            int eval_status = 0;

            mylite_select_eval_context_init(&user_context, stmt, row, callbacks, true, false);
            mylite_select_eval_expression_context_init(&context, &user_context);
            eval_status = mylite_expression_eval_with_context(
                order_key->expression,
                &context,
                &stmt->database->warnings,
                &row->order_values[index]
            );
            if (eval_status != 0) {
                status = mylite_select_eval_map_expression_status(stmt, eval_status, callbacks);
            }
        }

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_select_eval_materialize_output_values(
    mylite_stmt *stmt,
    struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks
) {
    if (row->output_value_count == stmt->select_plan.output_count) {
        return MYLITE_OK;
    }

    row->output_values = calloc(stmt->select_plan.output_count, sizeof(*row->output_values));
    if (row->output_values == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    row->output_value_count = stmt->select_plan.output_count;

    for (size_t index = 0U; index < stmt->select_plan.output_count; ++index) {
        int status = mylite_select_eval_output_value(
            stmt,
            row,
            index,
            callbacks,
            &row->output_values[index]
        );

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_select_eval_set_current_row(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks
) {
    mylite_select_result_current_values_deinit(&stmt->select_result);

    stmt->select_result.current_values =
        calloc(stmt->select_plan.output_count, sizeof(*stmt->select_result.current_values));
    stmt->select_result.current_texts =
        (char **)calloc(stmt->select_plan.output_count, sizeof(*stmt->select_result.current_texts));
    if (stmt->select_result.current_values == NULL || stmt->select_result.current_texts == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    stmt->select_result.current_value_count = stmt->select_plan.output_count;

    for (size_t index = 0U; index < stmt->select_plan.output_count; ++index) {
        int status = mylite_select_eval_cached_output_value(
            stmt,
            row,
            index,
            callbacks,
            &stmt->select_result.current_values[index]
        );

        if (status != MYLITE_OK) {
            return status;
        }
        if (stmt->select_result.current_values[index].kind != MYLITE_EXPRESSION_VALUE_NULL) {
            stmt->select_result.current_texts[index] =
                mylite_expression_value_to_text(&stmt->select_result.current_values[index]);
            if (stmt->select_result.current_texts[index] == NULL) {
                (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
                return MYLITE_NOMEM;
            }
        }
    }
    stmt->select_result.has_current_row = true;
    return MYLITE_OK;
}

int mylite_select_eval_constant_predicate(
    mylite_stmt *stmt,
    const struct mylite_select_eval_callbacks *callbacks
) {
    struct mylite_expression_value value = {0};
    int truth = -1;
    int status = 0;

    if (stmt->select_constant_predicate_evaluated ||
        !mylite_expression_is_cacheable_no_table(stmt->select_predicate)) {
        return MYLITE_OK;
    }

    status = mylite_expression_eval(stmt->select_predicate, &stmt->database->warnings, &value);
    if (status == 0) {
        status = mylite_expression_value_truth(&value, &stmt->database->warnings, &truth);
    }
    if (status != 0) {
        mylite_expression_value_deinit(&value);
        return callbacks->set_expression_eval_error(stmt);
    }

    stmt->select_constant_predicate_evaluated = true;
    stmt->select_constant_predicate_matches = truth == 1;
    mylite_expression_value_deinit(&value);
    return MYLITE_OK;
}

int mylite_select_eval_expression_predicate(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    const struct mylite_sql_ast_node *predicate,
    const struct mylite_select_eval_callbacks *callbacks,
    bool *out_matches
) {
    struct mylite_table_select_eval_context user_context = {0};
    struct mylite_expression_eval_context context = {0};
    struct mylite_expression_value value = {0};
    int truth = 0;
    int status = 0;

    *out_matches = false;
    mylite_select_eval_context_init(&user_context, stmt, row, callbacks, false, false);
    mylite_select_eval_expression_context_init(&context, &user_context);
    status =
        mylite_expression_eval_with_context(predicate, &context, &stmt->database->warnings, &value);
    if (status == 0) {
        status = mylite_expression_value_truth(&value, &stmt->database->warnings, &truth);
    }
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return mylite_select_eval_map_expression_status(stmt, status, callbacks);
    }

    *out_matches = truth == 1;
    return MYLITE_OK;
}

int mylite_select_eval_row_predicate(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    bool *out_matches
) {
    return mylite_select_eval_expression_predicate(
        stmt,
        row,
        stmt->select_predicate,
        callbacks,
        out_matches
    );
}
