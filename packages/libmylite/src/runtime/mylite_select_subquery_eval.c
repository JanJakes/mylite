#include "mylite_select_subquery.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_span.h"

#include <stdint.h>

static int evaluate_in_subquery_expression_inner(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left, struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value,
    const struct mylite_select_subquery_eval_callbacks *callbacks);
static int
prepare_in_subquery_statement(mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
                              mylite_stmt **out_subquery_stmt, size_t *out_order_key_count,
                              bool *out_restore_order_keys,
                              const struct mylite_select_subquery_eval_callbacks *callbacks);
static int scan_in_subquery_statement(const struct mylite_in_subquery_scan_context *context,
                                      struct mylite_in_subquery_scan_state *state);
static int scan_in_subquery_statement_row(const struct mylite_in_subquery_scan_context *context,
                                          struct mylite_in_subquery_scan_state *state);
static int finish_in_subquery_expression(const struct mylite_sql_ast_node *expression,
                                         const struct mylite_expression_value *left, bool has_row,
                                         bool matched, bool saw_unknown,
                                         struct mylite_expression_value *out_value);

int mylite_select_subquery_eval_in(mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
                                   const struct mylite_expression_value *left,
                                   struct mylite_expression_warnings *warnings,
                                   struct mylite_expression_value *out_value,
                                   const struct mylite_select_subquery_eval_callbacks *callbacks)
{
    struct mylite_expression_warnings saved_warnings = {0};
    struct mylite_expression_warnings subquery_warnings = {0};
    int status = MYLITE_UNSUPPORTED;

    if (stmt == NULL || stmt->database == NULL || expression == NULL || left == NULL ||
        callbacks == NULL || callbacks->prepare_select_subquery == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    saved_warnings = stmt->database->warnings;
    stmt->database->warnings = (struct mylite_expression_warnings){0};

    status = evaluate_in_subquery_expression_inner(stmt, expression, left, warnings, out_value,
                                                   callbacks);

    subquery_warnings = stmt->database->warnings;
    stmt->database->warnings = saved_warnings;
    if (mylite_select_subquery_append_warnings(warnings, &subquery_warnings) != MYLITE_OK) {
        mylite_expression_value_deinit(out_value);
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        status = MYLITE_NOMEM;
    }
    mylite_expression_warnings_deinit(&subquery_warnings);
    return status;
}

static int evaluate_in_subquery_expression_inner(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left, struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value,
    const struct mylite_select_subquery_eval_callbacks *callbacks)
{
    mylite_stmt *subquery_stmt = NULL;
    size_t order_key_count = 0U;
    bool restore_order_keys = false;
    struct mylite_in_subquery_scan_state scan = {
        .has_row = false,
        .matched = false,
        .saw_unknown = false,
    };
    int status = MYLITE_OK;

    status = prepare_in_subquery_statement(stmt, expression, &subquery_stmt, &order_key_count,
                                           &restore_order_keys, callbacks);
    if (status != MYLITE_OK) {
        return status;
    }

    struct mylite_in_subquery_scan_context scan_context = {
        .outer_stmt = stmt,
        .subquery_stmt = subquery_stmt,
        .left = left,
        .warnings = warnings,
    };

    status = scan_in_subquery_statement(&scan_context, &scan);
    if (restore_order_keys) {
        subquery_stmt->select_plan.order_key_count = order_key_count;
    }
    mylite_finalize(subquery_stmt);
    if (status != MYLITE_OK) {
        return status;
    }
    return finish_in_subquery_expression(expression, left, scan.has_row, scan.matched,
                                         scan.saw_unknown, out_value);
}

static int
prepare_in_subquery_statement(mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
                              mylite_stmt **out_subquery_stmt, size_t *out_order_key_count,
                              bool *out_restore_order_keys,
                              const struct mylite_select_subquery_eval_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *select_statement = mylite_ast_child_at(expression, 1U);
    mylite_stmt *subquery_stmt = NULL;
    int status = MYLITE_OK;

    *out_subquery_stmt = NULL;
    *out_order_key_count = 0U;
    *out_restore_order_keys = false;
    if (!mylite_select_subquery_binary_expression_is_in(expression)) {
        return MYLITE_UNSUPPORTED;
    }

    status = mylite_select_subquery_validate_in_select(stmt->database, select_statement);
    if (status != MYLITE_OK) {
        return status;
    }

    status = callbacks->prepare_select_subquery(stmt->database, select_statement, &subquery_stmt);
    if (status != MYLITE_OK) {
        return status;
    }
    if (subquery_stmt == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    status = mylite_select_subquery_validate_in_prepared_columns(stmt->database, subquery_stmt);
    if (status != MYLITE_OK) {
        mylite_finalize(subquery_stmt);
        return status;
    }
    if (subquery_stmt->kind == MYLITE_STMT_TABLE_SELECT &&
        subquery_stmt->select_plan.order_key_count != 0U) {
        *out_order_key_count = subquery_stmt->select_plan.order_key_count;
        *out_restore_order_keys = true;
        subquery_stmt->select_plan.order_key_count = 0U;
    }
    *out_subquery_stmt = subquery_stmt;
    return MYLITE_OK;
}

static int scan_in_subquery_statement(const struct mylite_in_subquery_scan_context *context,
                                      struct mylite_in_subquery_scan_state *state)
{
    for (;;) {
        int status = mylite_step(context->subquery_stmt);

        if (status == MYLITE_DONE) {
            return MYLITE_OK;
        }
        if (status != MYLITE_ROW) {
            return status;
        }
        state->has_row = true;
        status = scan_in_subquery_statement_row(context, state);
        if (status != MYLITE_OK || state->matched) {
            return status;
        }
    }
}

static int scan_in_subquery_statement_row(const struct mylite_in_subquery_scan_context *context,
                                          struct mylite_in_subquery_scan_state *state)
{
    struct mylite_expression_value right = {0};
    int comparison = 0;
    int status = MYLITE_OK;

    if (context->left->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return MYLITE_OK;
    }

    status = mylite_select_subquery_copy_column_value(context->subquery_stmt, &right);
    if (status != MYLITE_OK) {
        mylite_expression_value_deinit(&right);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(context->outer_stmt->database,
                                                       "out of memory");
        }
        return status;
    }
    if (right.kind == MYLITE_EXPRESSION_VALUE_NULL) {
        state->saw_unknown = true;
        mylite_expression_value_deinit(&right);
        return MYLITE_OK;
    }
    status = mylite_expression_value_compare(context->left, &right, context->warnings, &comparison);
    mylite_expression_value_deinit(&right);
    if (status != 0) {
        return status;
    }
    if (comparison == 0) {
        state->matched = true;
    }
    return MYLITE_OK;
}

static int finish_in_subquery_expression(const struct mylite_sql_ast_node *expression,
                                         const struct mylite_expression_value *left, bool has_row,
                                         bool matched, bool saw_unknown,
                                         struct mylite_expression_value *out_value)
{
    if (matched) {
        int64_t result = expression->operator_kind == MYLITE_SQL_AST_OPERATOR_IN ? 1 : 0;

        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = result};
        return MYLITE_OK;
    }
    if (!has_row) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = expression->operator_kind == MYLITE_SQL_AST_OPERATOR_IN ? 0 : 1};
        return MYLITE_OK;
    }
    if (left->kind == MYLITE_EXPRESSION_VALUE_NULL || saw_unknown) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return MYLITE_OK;
    }
    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_INT64,
        .int64_value = expression->operator_kind == MYLITE_SQL_AST_OPERATOR_IN ? 0 : 1};
    return MYLITE_OK;
}
