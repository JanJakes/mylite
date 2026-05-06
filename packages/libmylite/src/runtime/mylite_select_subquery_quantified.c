#include "mylite_select_subquery.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_span.h"

#include <stdint.h>

static int evaluate_quantified_subquery_expression_inner(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left, struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value,
    const struct mylite_select_subquery_eval_callbacks *callbacks);
static int prepare_quantified_subquery_statement(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
    mylite_stmt **out_subquery_stmt, size_t *out_order_key_count, bool *out_restore_order_keys,
    const struct mylite_select_subquery_eval_callbacks *callbacks);
static int
scan_quantified_subquery_statement(const struct mylite_quantified_subquery_scan_context *context,
                                   struct mylite_quantified_subquery_scan_state *state);
static int scan_quantified_subquery_statement_row(
    const struct mylite_quantified_subquery_scan_context *context,
    struct mylite_quantified_subquery_scan_state *state);
static int
finish_quantified_subquery_expression(enum mylite_sql_ast_subquery_quantifier quantifier,
                                      const struct mylite_quantified_subquery_scan_state *scan,
                                      struct mylite_expression_value *out_value);
static bool
quantified_comparison_result(const struct mylite_quantified_subquery_scan_context *context,
                             int comparison);

int mylite_select_subquery_eval_quantified(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left, struct mylite_expression_warnings *warnings,
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

    status = evaluate_quantified_subquery_expression_inner(stmt, expression, left, warnings,
                                                           out_value, callbacks);

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

static int evaluate_quantified_subquery_expression_inner(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left, struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value,
    const struct mylite_select_subquery_eval_callbacks *callbacks)
{
    mylite_stmt *subquery_stmt = NULL;
    size_t order_key_count = 0U;
    bool restore_order_keys = false;
    struct mylite_quantified_subquery_scan_state scan = {
        .has_row = false,
        .decided = false,
        .result = false,
        .saw_unknown = false,
    };
    int status = MYLITE_OK;

    status = prepare_quantified_subquery_statement(
        stmt, expression, &subquery_stmt, &order_key_count, &restore_order_keys, callbacks);
    if (status != MYLITE_OK) {
        return status;
    }

    struct mylite_quantified_subquery_scan_context scan_context = {
        .outer_stmt = stmt,
        .subquery_stmt = subquery_stmt,
        .left = left,
        .warnings = warnings,
        .operator_kind = expression->operator_kind,
        .quantifier = expression->subquery_quantifier,
    };

    status = scan_quantified_subquery_statement(&scan_context, &scan);
    if (restore_order_keys) {
        subquery_stmt->select_plan.order_key_count = order_key_count;
    }
    mylite_finalize(subquery_stmt);
    if (status != MYLITE_OK) {
        return status;
    }
    return finish_quantified_subquery_expression(expression->subquery_quantifier, &scan, out_value);
}

static int prepare_quantified_subquery_statement(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
    mylite_stmt **out_subquery_stmt, size_t *out_order_key_count, bool *out_restore_order_keys,
    const struct mylite_select_subquery_eval_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *select_statement = mylite_ast_child_at(expression, 1U);
    mylite_stmt *subquery_stmt = NULL;
    int status = MYLITE_OK;

    *out_subquery_stmt = NULL;
    *out_order_key_count = 0U;
    *out_restore_order_keys = false;
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_QUANTIFIED_COMPARISON ||
        !mylite_select_subquery_quantified_operator_is_supported(expression->operator_kind) ||
        expression->subquery_quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_NONE) {
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

static int
scan_quantified_subquery_statement(const struct mylite_quantified_subquery_scan_context *context,
                                   struct mylite_quantified_subquery_scan_state *state)
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
        status = scan_quantified_subquery_statement_row(context, state);
        if (status != MYLITE_OK || state->decided) {
            return status;
        }
    }
}

static int scan_quantified_subquery_statement_row(
    const struct mylite_quantified_subquery_scan_context *context,
    struct mylite_quantified_subquery_scan_state *state)
{
    struct mylite_expression_value right = {0};
    int comparison = 0;
    int status = MYLITE_OK;
    bool comparison_result = false;

    if (context->left->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        state->saw_unknown = true;
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

    comparison_result = quantified_comparison_result(context, comparison);
    if (context->quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ALL) {
        if (!comparison_result) {
            state->decided = true;
            state->result = false;
        }
        return MYLITE_OK;
    }
    if (comparison_result) {
        state->decided = true;
        state->result = true;
    }
    return MYLITE_OK;
}

static int
finish_quantified_subquery_expression(enum mylite_sql_ast_subquery_quantifier quantifier,
                                      const struct mylite_quantified_subquery_scan_state *scan,
                                      struct mylite_expression_value *out_value)
{
    if (!scan->has_row) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ALL ? 1 : 0};
        return MYLITE_OK;
    }
    if (scan->decided) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = (int64_t)scan->result};
        return MYLITE_OK;
    }
    if (scan->saw_unknown) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return MYLITE_OK;
    }
    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_INT64,
        .int64_value = quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ALL ? 1 : 0};
    return MYLITE_OK;
}

static bool
quantified_comparison_result(const struct mylite_quantified_subquery_scan_context *context,
                             int comparison)
{
    switch (context->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return comparison == 0;
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return comparison != 0;
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return comparison < 0;
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return comparison <= 0;
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return comparison > 0;
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return comparison >= 0;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BINARY_CAST:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_NOT_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        break;
    }
    return false;
}
