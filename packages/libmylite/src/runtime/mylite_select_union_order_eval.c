#include "mylite_select_union.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_span.h"
#include "mylite_system_variables.h"
#include "mylite_user_variables.h"

#include <stdlib.h>

struct mylite_union_expression_context {
    mylite_stmt *stmt;
    const struct mylite_table_select_row *row;
    const struct mylite_select_union_callbacks *callbacks;
};

static int evaluate_union_order_key(mylite_stmt *stmt, const struct mylite_table_select_row *row,
                                    const struct mylite_select_order_key *order_key,
                                    struct mylite_expression_value *out_value,
                                    const struct mylite_select_union_callbacks *callbacks);
static int resolve_union_expression_identifier(void *user_data,
                                               const struct mylite_sql_ast_node *identifier,
                                               struct mylite_expression_value *out_value);
static int
evaluate_union_session_function(void *user_data, const struct mylite_sql_ast_node *function_call,
                                const struct mylite_expression_eval_context *expression_context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);

int mylite_select_union_evaluate_order_values(mylite_stmt *stmt,
                                              struct mylite_table_select_row *row,
                                              const struct mylite_select_union_callbacks *callbacks)
{
    row->order_values = calloc(stmt->select_plan.order_key_count, sizeof(*row->order_values));
    if (row->order_values == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    row->order_value_count = stmt->select_plan.order_key_count;

    for (size_t index = 0U; index < stmt->select_plan.order_key_count; ++index) {
        int status = evaluate_union_order_key(stmt, row, &stmt->select_plan.order_keys[index],
                                              &row->order_values[index], callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int evaluate_union_order_key(mylite_stmt *stmt, const struct mylite_table_select_row *row,
                                    const struct mylite_select_order_key *order_key,
                                    struct mylite_expression_value *out_value,
                                    const struct mylite_select_union_callbacks *callbacks)
{
    if (order_key->kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
        if (order_key->output_index >= row->output_value_count ||
            mylite_expression_value_copy(&row->output_values[order_key->output_index], out_value) !=
                0) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }

    {
        struct mylite_union_expression_context user_context = {
            .stmt = stmt,
            .row = row,
            .callbacks = callbacks,
        };
        struct mylite_expression_eval_context context = {
            .user_data = &user_context,
            .resolve_identifier = resolve_union_expression_identifier,
            .eval_session_function = evaluate_union_session_function,
        };
        int status = mylite_expression_eval_with_context(order_key->expression, &context,
                                                         &stmt->database->warnings, out_value);

        if (status == 0) {
            return MYLITE_OK;
        }
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        if (stmt->database->error_message != NULL) {
            return MYLITE_EXEC_ERROR;
        }
    }
    return callbacks->set_unsupported_order_error(stmt->database);
}

static int resolve_union_expression_identifier(void *user_data,
                                               const struct mylite_sql_ast_node *identifier,
                                               struct mylite_expression_value *out_value)
{
    struct mylite_union_expression_context *context = user_data;
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = MYLITE_OK;

    if (context == NULL || context->stmt == NULL || context->row == NULL) {
        return -1;
    }
    if (mylite_system_variable_identifier_is_system_variable(identifier)) {
        return mylite_system_variable_eval_identifier(context->stmt->database, identifier,
                                                      out_value);
    }
    if (mylite_user_variable_identifier_is_user_variable(identifier)) {
        return mylite_user_variable_eval_identifier(context->stmt->database, identifier, out_value);
    }

    status = mylite_copy_identifier_parts(identifier, parts, &part_count);
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(context->stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        return -1;
    }
    if (part_count == 1U) {
        size_t output_index = 0U;
        size_t output_matches =
            mylite_select_output_label_count(&context->stmt->select_plan, parts[0], &output_index);

        if (output_matches == 1U && output_index < context->row->output_value_count) {
            status = mylite_expression_value_copy(&context->row->output_values[output_index],
                                                  out_value) == 0
                         ? 0
                         : MYLITE_NOMEM;
        } else {
            status = -1;
        }
    } else {
        status = -1;
    }

    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(context->stmt->database, "out of memory");
    }
    return status;
}

static int
evaluate_union_session_function(void *user_data, const struct mylite_sql_ast_node *function_call,
                                const struct mylite_expression_eval_context *expression_context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    struct mylite_union_expression_context *context = user_data;

    if (context == NULL || context->callbacks == NULL ||
        context->callbacks->select_eval_callbacks == NULL ||
        context->callbacks->select_eval_callbacks->eval_session_function == NULL) {
        return MYLITE_EXEC_ERROR;
    }
    return context->callbacks->select_eval_callbacks->eval_session_function(
        context->stmt, function_call, expression_context, warnings, NULL, out_value);
}
