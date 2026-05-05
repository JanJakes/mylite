#include "mylite_select_subquery.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_select_eval.h"
#include "mylite_select_row_compare.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <stdint.h>
#include <stdlib.h>

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
static int evaluate_quantified_subquery_expression_inner(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left, struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value,
    const struct mylite_select_subquery_eval_callbacks *callbacks);
static int evaluate_row_subquery_expression_inner(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value,
    const struct mylite_select_subquery_eval_callbacks *callbacks);
static int evaluate_row_in_subquery_statement(mylite_stmt *stmt,
                                              const struct mylite_sql_ast_node *expression,
                                              mylite_stmt *subquery_stmt,
                                              const struct mylite_row_expression_values *left,
                                              struct mylite_expression_warnings *warnings,
                                              struct mylite_expression_value *out_value);
static int evaluate_row_scalar_subquery_statement(mylite_stmt *stmt,
                                                  const struct mylite_sql_ast_node *expression,
                                                  mylite_stmt *subquery_stmt,
                                                  const struct mylite_row_expression_values *left,
                                                  struct mylite_expression_value *out_value);
static int
prepare_row_subquery_statement(mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
                               size_t expected_width, mylite_stmt **out_subquery_stmt,
                               size_t *out_order_key_count, bool *out_restore_order_keys,
                               const struct mylite_select_subquery_eval_callbacks *callbacks);
static int scan_row_in_subquery_statement(const struct mylite_row_in_subquery_scan_context *context,
                                          struct mylite_row_in_subquery_scan_state *state);
static int
scan_row_in_subquery_statement_row(const struct mylite_row_in_subquery_scan_context *context,
                                   struct mylite_row_in_subquery_scan_state *state);
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
static int finish_in_subquery_expression(const struct mylite_sql_ast_node *expression,
                                         const struct mylite_expression_value *left, bool has_row,
                                         bool matched, bool saw_unknown,
                                         struct mylite_expression_value *out_value);
static int finish_row_in_subquery_expression(const struct mylite_sql_ast_node *expression,
                                             bool has_row, bool matched, bool saw_unknown,
                                             struct mylite_expression_value *out_value);
static int
finish_quantified_subquery_expression(enum mylite_sql_ast_subquery_quantifier quantifier,
                                      const struct mylite_quantified_subquery_scan_state *scan,
                                      struct mylite_expression_value *out_value);
static bool
quantified_comparison_result(const struct mylite_quantified_subquery_scan_context *context,
                             int comparison);
static int
evaluate_row_constructor_values(const struct mylite_sql_ast_node *row,
                                const struct mylite_expression_eval_context *expression_context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_row_expression_values *out_values);

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

int mylite_select_subquery_eval_row(mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
                                    const struct mylite_expression_eval_context *expression_context,
                                    struct mylite_expression_warnings *warnings,
                                    struct mylite_expression_value *out_value,
                                    const struct mylite_select_subquery_eval_callbacks *callbacks)
{
    struct mylite_expression_warnings saved_warnings = {0};
    struct mylite_expression_warnings subquery_warnings = {0};
    int status = MYLITE_UNSUPPORTED;

    if (stmt == NULL || stmt->database == NULL || expression == NULL ||
        expression_context == NULL || callbacks == NULL ||
        callbacks->prepare_select_subquery == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    saved_warnings = stmt->database->warnings;
    stmt->database->warnings = (struct mylite_expression_warnings){0};

    status = evaluate_row_subquery_expression_inner(stmt, expression, expression_context, warnings,
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

static int evaluate_row_subquery_expression_inner(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value,
    const struct mylite_select_subquery_eval_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *left_expression =
        mylite_sql_ast_unwrap_parenthesized_expression(mylite_ast_child_at(expression, 0U));
    struct mylite_row_expression_values left = {0};
    mylite_stmt *subquery_stmt = NULL;
    size_t order_key_count = 0U;
    bool restore_order_keys = false;
    int status = MYLITE_OK;

    if (mylite_select_subquery_quantified_comparison_has_row_left(expression) &&
        !mylite_select_subquery_quantified_comparison_is_row_alias(expression)) {
        return mylite_select_subquery_set_row_quantified_non_alias_error(stmt->database,
                                                                         expression);
    }

    status = evaluate_row_constructor_values(left_expression, expression_context, warnings, &left);
    if (status != MYLITE_OK) {
        mylite_select_subquery_row_values_deinit(&left);
        return status;
    }
    status = prepare_row_subquery_statement(stmt, expression, left.count, &subquery_stmt,
                                            &order_key_count, &restore_order_keys, callbacks);
    if (status != MYLITE_OK) {
        mylite_select_subquery_row_values_deinit(&left);
        return status;
    }

    if (mylite_select_subquery_row_expression_is_membership(expression)) {
        status = evaluate_row_in_subquery_statement(stmt, expression, subquery_stmt, &left,
                                                    warnings, out_value);
    } else {
        status = evaluate_row_scalar_subquery_statement(stmt, expression, subquery_stmt, &left,
                                                        out_value);
    }
    if (restore_order_keys) {
        subquery_stmt->select_plan.order_key_count = order_key_count;
    }
    mylite_finalize(subquery_stmt);
    mylite_select_subquery_row_values_deinit(&left);
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
    }
    return status;
}

static int evaluate_row_in_subquery_statement(mylite_stmt *stmt,
                                              const struct mylite_sql_ast_node *expression,
                                              mylite_stmt *subquery_stmt,
                                              const struct mylite_row_expression_values *left,
                                              struct mylite_expression_warnings *warnings,
                                              struct mylite_expression_value *out_value)
{
    struct mylite_expression_warnings comparison_warnings = {0};
    struct mylite_row_in_subquery_scan_state scan = {
        .has_row = false,
        .matched = false,
        .saw_unknown = false,
    };
    struct mylite_row_in_subquery_scan_context scan_context = {
        .outer_stmt = stmt,
        .subquery_stmt = subquery_stmt,
        .left = left,
        .warnings = &comparison_warnings,
        .left_has_null = mylite_select_row_expression_values_has_null(left),
    };
    int status = scan_row_in_subquery_statement(&scan_context, &scan);

    if (status != MYLITE_OK) {
        mylite_expression_warnings_deinit(&comparison_warnings);
        return status;
    }
    if (mylite_select_subquery_append_warnings(warnings, &comparison_warnings) != MYLITE_OK) {
        mylite_expression_warnings_deinit(&comparison_warnings);
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    mylite_expression_warnings_deinit(&comparison_warnings);
    return finish_row_in_subquery_expression(expression, scan.has_row, scan.matched,
                                             scan.saw_unknown, out_value);
}

static int evaluate_row_scalar_subquery_statement(mylite_stmt *stmt,
                                                  const struct mylite_sql_ast_node *expression,
                                                  mylite_stmt *subquery_stmt,
                                                  const struct mylite_row_expression_values *left,
                                                  struct mylite_expression_value *out_value)
{
    struct mylite_row_expression_values right = {0};
    int truth = -1;
    int status = mylite_step(subquery_stmt);

    if (status == MYLITE_DONE) {
        if (expression->operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 0};
        } else {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        }
        return MYLITE_OK;
    }
    if (status != MYLITE_ROW) {
        return status;
    }

    status = mylite_select_subquery_copy_row_values(subquery_stmt, left->count, &right);
    if (status == MYLITE_OK) {
        /* MySQL suppresses tuple element conversion warnings here, unlike row IN. */
        status =
            mylite_select_compare_row_values(expression->operator_kind, left, &right, NULL, &truth);
    }
    mylite_select_subquery_row_values_deinit(&right);
    if (status != MYLITE_OK) {
        return status;
    }

    status = mylite_step(subquery_stmt);
    if (status == MYLITE_ROW) {
        return mylite_select_subquery_set_scalar_cardinality_error(stmt->database);
    }
    if (status != MYLITE_DONE) {
        return status;
    }
    if (truth < 0) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
    } else {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = truth};
    }
    return MYLITE_OK;
}

static int
prepare_row_subquery_statement(mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
                               size_t expected_width, mylite_stmt **out_subquery_stmt,
                               size_t *out_order_key_count, bool *out_restore_order_keys,
                               const struct mylite_select_subquery_eval_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *select_statement =
        mylite_select_subquery_row_select_statement(expression);
    mylite_stmt *subquery_stmt = NULL;
    int status = MYLITE_OK;

    *out_subquery_stmt = NULL;
    *out_order_key_count = 0U;
    *out_restore_order_keys = false;
    if (!mylite_select_subquery_row_expression_is_supported(expression) || expected_width < 2U) {
        return MYLITE_UNSUPPORTED;
    }
    if (mylite_select_subquery_row_expression_is_membership(expression) &&
        mylite_ast_find_child_kind(select_statement, MYLITE_SQL_AST_LIMIT_CLAUSE) != NULL) {
        return mylite_select_subquery_set_in_limit_error(stmt->database);
    }

    status = mylite_select_subquery_validate_row_select_columns(stmt->database, select_statement,
                                                                expected_width);
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
    status = mylite_select_subquery_validate_row_prepared_columns(stmt->database, subquery_stmt,
                                                                  expected_width);
    if (status != MYLITE_OK) {
        mylite_finalize(subquery_stmt);
        return status;
    }
    if (mylite_select_subquery_row_expression_is_membership(expression) &&
        subquery_stmt->kind == MYLITE_STMT_TABLE_SELECT &&
        subquery_stmt->select_plan.order_key_count != 0U) {
        *out_order_key_count = subquery_stmt->select_plan.order_key_count;
        *out_restore_order_keys = true;
        subquery_stmt->select_plan.order_key_count = 0U;
    }
    *out_subquery_stmt = subquery_stmt;
    return MYLITE_OK;
}

static int scan_row_in_subquery_statement(const struct mylite_row_in_subquery_scan_context *context,
                                          struct mylite_row_in_subquery_scan_state *state)
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
        status = scan_row_in_subquery_statement_row(context, state);
        if (status != MYLITE_OK || state->matched ||
            (context->left_has_null && state->saw_unknown)) {
            return status;
        }
    }
}

static int
scan_row_in_subquery_statement_row(const struct mylite_row_in_subquery_scan_context *context,
                                   struct mylite_row_in_subquery_scan_state *state)
{
    struct mylite_row_expression_values right = {0};
    int truth = -1;
    int status = mylite_select_subquery_copy_row_values(context->subquery_stmt,
                                                        context->left->count, &right);

    if (status == MYLITE_OK) {
        status = mylite_select_compare_row_values(MYLITE_SQL_AST_OPERATOR_EQUAL, context->left,
                                                  &right, context->warnings, &truth);
    }
    mylite_select_subquery_row_values_deinit(&right);
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(context->outer_stmt->database,
                                                       "out of memory");
        }
        return status;
    }
    if (truth == 1) {
        state->matched = true;
    } else if (truth < 0) {
        state->saw_unknown = true;
    }
    return MYLITE_OK;
}

static int finish_row_in_subquery_expression(const struct mylite_sql_ast_node *expression,
                                             bool has_row, bool matched, bool saw_unknown,
                                             struct mylite_expression_value *out_value)
{
    bool positive = mylite_select_subquery_row_expression_is_positive_membership(expression);
    int64_t matched_value = 0;
    int64_t unmatched_value = 1;

    if (positive) {
        matched_value = 1;
        unmatched_value = 0;
    }

    if (matched) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = matched_value,
        };
        return MYLITE_OK;
    }
    if (!has_row) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = unmatched_value,
        };
        return MYLITE_OK;
    }
    if (saw_unknown) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return MYLITE_OK;
    }
    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_INT64,
        .int64_value = unmatched_value,
    };
    return MYLITE_OK;
}

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

static int
evaluate_row_constructor_values(const struct mylite_sql_ast_node *row,
                                const struct mylite_expression_eval_context *expression_context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_row_expression_values *out_values)
{
    size_t width = mylite_select_subquery_row_constructor_width(row);
    size_t index = 0U;

    *out_values = (struct mylite_row_expression_values){0};
    if (row == NULL || row->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR || width < 2U) {
        return MYLITE_UNSUPPORTED;
    }
    out_values->items = calloc(width, sizeof(*out_values->items));
    if (out_values->items == NULL) {
        return MYLITE_NOMEM;
    }
    out_values->count = width;
    for (const struct mylite_sql_ast_node *child = row->first_child; child != NULL;
         child = child->next_sibling, ++index) {
        int status = mylite_expression_eval_with_context(child, expression_context, warnings,
                                                         &out_values->items[index]);

        if (status != 0) {
            return status > 0 ? status : MYLITE_UNSUPPORTED;
        }
    }
    return MYLITE_OK;
}
