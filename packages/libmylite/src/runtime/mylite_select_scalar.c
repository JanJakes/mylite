#include "mylite_select_scalar.h"

#include "mylite_diagnostics.h"
#include "mylite_expression_validation.h"
#include "mylite_metadata.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_aggregate.h"
#include "mylite_select_resolve.h"
#include "mylite_select_scalar_order_validate.h"
#include "mylite_select_subquery.h"
#include "mylite_span.h"
#include "mylite_statement.h"

#include <stdint.h>
#include <stdlib.h>

struct mylite_select_scalar_expression_context {
    mylite_stmt *stmt;
    const struct mylite_select_scalar_eval_callbacks *callbacks;
};

static int bind_scalar_select_limit_clause(mylite_stmt *stmt,
                                           const struct mylite_sql_ast_node *limit_clause);
static int copy_scalar_select_item(mylite_stmt *stmt, const struct mylite_sql_ast_node *item,
                                   size_t index, const char *source_sql, size_t source_sql_length,
                                   const struct mylite_select_scalar_eval_callbacks *callbacks);
static int copy_scalar_select_item_expression(mylite_stmt *stmt,
                                              const struct mylite_sql_ast_node *expression,
                                              size_t index, const char *source_sql,
                                              size_t source_sql_length);
static int
evaluate_scalar_select_result(mylite_stmt *stmt,
                              const struct mylite_select_scalar_eval_callbacks *callbacks);
static int
evaluate_scalar_select_result_item(mylite_stmt *stmt, size_t index,
                                   const struct mylite_select_scalar_eval_callbacks *callbacks);
static int
evaluate_scalar_aggregate_expression(mylite_stmt *stmt,
                                     const struct mylite_sql_ast_node *expression,
                                     const struct mylite_select_scalar_eval_callbacks *callbacks,
                                     struct mylite_expression_value *out_value);
static int
evaluate_scalar_count_distinct_expression(mylite_stmt *stmt,
                                          const struct mylite_sql_ast_node *arguments,
                                          const struct mylite_expression_eval_context *context,
                                          struct mylite_expression_value *out_value);
static int evaluate_scalar_numeric_aggregate_expression(
    mylite_stmt *stmt, enum mylite_sql_ast_aggregate_kind aggregate_kind,
    const struct mylite_expression_value *argument, struct mylite_expression_value *out_value);
static struct mylite_expression_eval_context
scalar_expression_eval_context(struct mylite_select_scalar_expression_context *context);
static int scalar_context_eval_session_function(
    void *user_data, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value);
static int scalar_context_eval_subquery(void *user_data, const struct mylite_sql_ast_node *subquery,
                                        struct mylite_expression_warnings *warnings,
                                        struct mylite_expression_value *out_value);
static int scalar_context_eval_in_subquery(void *user_data,
                                           const struct mylite_sql_ast_node *expression,
                                           const struct mylite_expression_value *left,
                                           struct mylite_expression_warnings *warnings,
                                           struct mylite_expression_value *out_value);
static int scalar_context_eval_quantified_subquery(void *user_data,
                                                   const struct mylite_sql_ast_node *expression,
                                                   const struct mylite_expression_value *left,
                                                   struct mylite_expression_warnings *warnings,
                                                   struct mylite_expression_value *out_value);
static int
scalar_context_eval_row_subquery(void *user_data, const struct mylite_sql_ast_node *expression,
                                 const struct mylite_expression_eval_context *expression_context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *out_value);
static bool
scalar_eval_callbacks_are_valid(const struct mylite_select_scalar_eval_callbacks *callbacks);

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_select_scalar_copy_statement(const struct mylite_sql_ast_node *statement,
                                        mylite_stmt *stmt,
                                        const struct mylite_select_scalar_eval_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *order_by_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    const struct mylite_sql_ast_node *limit_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    size_t column_count = 0U;

    if (stmt == NULL || stmt->database == NULL || !scalar_eval_callbacks_are_valid(callbacks)) {
        return MYLITE_UNSUPPORTED;
    }
    for (const struct mylite_sql_ast_node *item = select_list == NULL ? NULL
                                                                      : select_list->first_child;
         item != NULL; item = item->next_sibling) {
        ++column_count;
    }
    if (column_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    stmt->scalar_select_sql_text =
        mylite_copy_span_text(statement->span.text, statement->span.length);
    stmt->scalar_result.values = calloc(column_count, sizeof(*stmt->scalar_result.values));
    stmt->scalar_result.texts = (char **)calloc(column_count, sizeof(*stmt->scalar_result.texts));
    stmt->scalar_result.expressions = (const struct mylite_sql_ast_node **)calloc(
        column_count, sizeof(*stmt->scalar_result.expressions));
    stmt->result_metadata.columns = calloc(column_count, sizeof(*stmt->result_metadata.columns));
    if (stmt->scalar_select_sql_text == NULL || stmt->scalar_result.values == NULL ||
        stmt->scalar_result.texts == NULL || stmt->scalar_result.expressions == NULL ||
        stmt->result_metadata.columns == NULL) {
        return MYLITE_NOMEM;
    }
    stmt->scalar_result.value_count = column_count;
    stmt->result_metadata.column_count = column_count;
    stmt->affected_rows = -1;
    stmt->scalar_result.row_available = true;

    if (limit_clause != NULL) {
        int status = bind_scalar_select_limit_clause(stmt, limit_clause);

        if (status != MYLITE_OK) {
            return status;
        }
    }

    size_t index = 0U;
    for (const struct mylite_sql_ast_node *item = select_list->first_child; item != NULL;
         item = item->next_sibling, ++index) {
        int status = copy_scalar_select_item(stmt, item, index, statement->span.text,
                                             statement->span.length, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (order_by_clause != NULL) {
        return mylite_select_scalar_validate_order_by_clause(stmt->database, order_by_clause,
                                                             &stmt->result_metadata, callbacks);
    }
    return MYLITE_OK;
}

static int bind_scalar_select_limit_clause(mylite_stmt *stmt,
                                           const struct mylite_sql_ast_node *limit_clause)
{
    int status = mylite_select_bind_limit_clause(limit_clause, &stmt->select_plan);

    if (status != MYLITE_OK) {
        return status;
    }
    stmt->scalar_result.row_available = mylite_select_limit_row_is_kept(
        &stmt->select_plan.limit, (struct mylite_select_limit_position){
                                      .matched_row = 0U,
                                      .kept_count = 0U,
                                  });
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int copy_scalar_select_item(mylite_stmt *stmt, const struct mylite_sql_ast_node *item,
                                   size_t index, const char *source_sql, size_t source_sql_length,
                                   const struct mylite_select_scalar_eval_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *expression = mylite_ast_child_at(item, 0U);
    const struct mylite_sql_ast_node *alias = mylite_ast_child_at(item, 1U);
    const struct mylite_expression_value *descriptor_value = NULL;
    bool defer_expression = false;
    int status = MYLITE_OK;

    if (stmt->scalar_result.row_available) {
        const bool supported_no_table = mylite_expression_is_supported_no_table(expression);
        const bool cacheable_no_table = mylite_expression_is_cacheable_no_table(expression);

        if (supported_no_table && !cacheable_no_table) {
            defer_expression = true;
        }
    }
    if (stmt->scalar_result.row_available && !defer_expression) {
        status = mylite_select_scalar_evaluate_expression(stmt, expression, callbacks,
                                                          &stmt->scalar_result.values[index]);
        if (status != MYLITE_OK) {
            int warning_status = mylite_select_scalar_append_warnings_to_database(stmt);

            return warning_status != MYLITE_OK ? warning_status : status;
        }
        stmt->scalar_result.texts[index] =
            mylite_expression_value_to_text(&stmt->scalar_result.values[index]);
        if (stmt->scalar_result.values[index].kind != MYLITE_EXPRESSION_VALUE_NULL &&
            stmt->scalar_result.texts[index] == NULL) {
            return MYLITE_NOMEM;
        }
        descriptor_value = &stmt->scalar_result.values[index];
    } else if (stmt->scalar_result.row_available) {
        status = copy_scalar_select_item_expression(stmt, expression, index, source_sql,
                                                    source_sql_length);
        if (status != MYLITE_OK) {
            return status;
        }
    }

    if (alias != NULL) {
        stmt->result_metadata.columns[index].name = mylite_select_copy_alias(alias);
    } else {
        stmt->result_metadata.columns[index].name =
            mylite_copy_span_text(expression->span.text, expression->span.length);
    }
    if (stmt->result_metadata.columns[index].name == NULL) {
        return MYLITE_NOMEM;
    }
    return callbacks->infer_expression_descriptor(stmt->database, expression, descriptor_value,
                                                  &stmt->result_metadata.columns[index].descriptor);
}

static int copy_scalar_select_item_expression(mylite_stmt *stmt,
                                              const struct mylite_sql_ast_node *expression,
                                              size_t index, const char *source_sql,
                                              size_t source_sql_length)
{
    struct mylite_sql_ast_node *clone = NULL;
    int status = mylite_statement_clone_sql_ast_subtree(&stmt->scalar_select_ast, expression,
                                                        source_sql, stmt->scalar_select_sql_text,
                                                        source_sql_length, &clone);

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
    }
    if (status == MYLITE_OK) {
        stmt->scalar_result.expressions[index] = clone;
    }
    return status;
}

int mylite_select_scalar_execute_statement(
    mylite_stmt *stmt, const struct mylite_select_scalar_eval_callbacks *callbacks)
{
    int status = MYLITE_OK;

    if (stmt == NULL || !scalar_eval_callbacks_are_valid(callbacks)) {
        return MYLITE_UNSUPPORTED;
    }
    if (!stmt->scalar_result.row_available || stmt->scalar_result.has_row) {
        return MYLITE_DONE;
    }

    status = evaluate_scalar_select_result(stmt, callbacks);
    if (status != MYLITE_OK) {
        return status;
    }

    stmt->executed = true;
    stmt->database->warnings = stmt->scalar_result.warnings;
    stmt->scalar_result.warnings = (struct mylite_expression_warnings){0};
    stmt->affected_rows = -1;
    stmt->scalar_result.has_row = true;
    return MYLITE_ROW;
}

static int
evaluate_scalar_select_result(mylite_stmt *stmt,
                              const struct mylite_select_scalar_eval_callbacks *callbacks)
{
    for (size_t index = 0U; index < stmt->scalar_result.value_count; ++index) {
        int status = evaluate_scalar_select_result_item(stmt, index, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int
evaluate_scalar_select_result_item(mylite_stmt *stmt, size_t index,
                                   const struct mylite_select_scalar_eval_callbacks *callbacks)
{
    int status = MYLITE_OK;

    if (stmt->scalar_result.expressions[index] == NULL) {
        return MYLITE_OK;
    }

    mylite_expression_value_deinit(&stmt->scalar_result.values[index]);
    free(stmt->scalar_result.texts[index]);
    stmt->scalar_result.texts[index] = NULL;

    status =
        mylite_select_scalar_evaluate_expression(stmt, stmt->scalar_result.expressions[index],
                                                 callbacks, &stmt->scalar_result.values[index]);
    if (status != MYLITE_OK) {
        int warning_status = mylite_select_scalar_append_warnings_to_database(stmt);

        return warning_status != MYLITE_OK ? warning_status : status;
    }
    stmt->scalar_result.texts[index] =
        mylite_expression_value_to_text(&stmt->scalar_result.values[index]);
    if (stmt->scalar_result.values[index].kind != MYLITE_EXPRESSION_VALUE_NULL &&
        stmt->scalar_result.texts[index] == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

int mylite_select_scalar_append_warnings_to_database(mylite_stmt *stmt)
{
    int status = MYLITE_OK;

    if (stmt == NULL || stmt->database == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (stmt->scalar_result.warnings.count == 0U) {
        return MYLITE_OK;
    }

    status = mylite_select_subquery_append_warnings(&stmt->database->warnings,
                                                    &stmt->scalar_result.warnings);
    if (status != MYLITE_OK) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
    }
    return status;
}

int mylite_select_scalar_evaluate_expression(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
    const struct mylite_select_scalar_eval_callbacks *callbacks,
    struct mylite_expression_value *out_value)
{
    struct mylite_select_scalar_expression_context scalar_context = {
        .stmt = stmt,
        .callbacks = callbacks,
    };
    struct mylite_expression_eval_context context = scalar_expression_eval_context(&scalar_context);
    int status = MYLITE_OK;

    if (stmt == NULL || stmt->database == NULL || out_value == NULL ||
        !scalar_eval_callbacks_are_valid(callbacks)) {
        return MYLITE_UNSUPPORTED;
    }
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_AGGREGATE_CALL) {
        return evaluate_scalar_aggregate_expression(stmt, expression, callbacks, out_value);
    }

    status = mylite_expression_eval_with_context(expression, &context,
                                                 &stmt->scalar_result.warnings, out_value);
    if (status == 0) {
        return MYLITE_OK;
    }
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (stmt->database->error_message != NULL) {
        return status > 0 ? status : MYLITE_EXEC_ERROR;
    }
    for (size_t index = 0U; index < stmt->scalar_result.warnings.count; ++index) {
        const struct mylite_expression_warning *warning =
            &stmt->scalar_result.warnings.items[index];

        if (warning->level == MYLITE_EXPRESSION_WARNING_LEVEL_ERROR) {
            int error_status =
                mylite_diagnostics_set_error_message(stmt->database, warning->message);

            return error_status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
        }
    }
    return MYLITE_UNSUPPORTED;
}

static int
evaluate_scalar_aggregate_expression(mylite_stmt *stmt,
                                     const struct mylite_sql_ast_node *expression,
                                     const struct mylite_select_scalar_eval_callbacks *callbacks,
                                     struct mylite_expression_value *out_value)
{
    struct mylite_expression_value argument = {0};
    struct mylite_select_scalar_expression_context scalar_context = {
        .stmt = stmt,
        .callbacks = callbacks,
    };
    struct mylite_expression_eval_context context = scalar_expression_eval_context(&scalar_context);
    int status = 0;

    if (expression->aggregate_kind == MYLITE_SQL_AST_AGGREGATE_COUNT &&
        expression->aggregate_argument == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_STAR) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = 1};
        return MYLITE_OK;
    }
    if (expression->aggregate_argument != MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION) {
        if (expression->aggregate_kind == MYLITE_SQL_AST_AGGREGATE_COUNT &&
            expression->aggregate_argument ==
                MYLITE_SQL_AST_AGGREGATE_ARGUMENT_DISTINCT_EXPRESSION_LIST) {
            return evaluate_scalar_count_distinct_expression(
                stmt, mylite_ast_child_at(expression, 1U), &context, out_value);
        }
        return MYLITE_UNSUPPORTED;
    }

    status = mylite_expression_eval_with_context(mylite_ast_child_at(expression, 1U), &context,
                                                 &stmt->scalar_result.warnings, &argument);
    if (status != 0) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        if (stmt->database->error_message != NULL) {
            return status > 0 ? status : MYLITE_EXEC_ERROR;
        }
        return MYLITE_UNSUPPORTED;
    }

    if (expression->aggregate_kind == MYLITE_SQL_AST_AGGREGATE_COUNT) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = argument.kind == MYLITE_EXPRESSION_VALUE_NULL ? 0 : 1,
        };
        mylite_expression_value_deinit(&argument);
        return MYLITE_OK;
    }
    if (argument.kind == MYLITE_EXPRESSION_VALUE_NULL) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        mylite_expression_value_deinit(&argument);
        return MYLITE_OK;
    }

    switch (expression->aggregate_kind) {
    case MYLITE_SQL_AST_AGGREGATE_SUM:
    case MYLITE_SQL_AST_AGGREGATE_AVG:
        status = evaluate_scalar_numeric_aggregate_expression(stmt, expression->aggregate_kind,
                                                              &argument, out_value);
        if (status != MYLITE_OK) {
            mylite_expression_value_deinit(&argument);
            return status;
        }
        break;
    case MYLITE_SQL_AST_AGGREGATE_MIN:
    case MYLITE_SQL_AST_AGGREGATE_MAX:
        if (mylite_expression_value_copy(&argument, out_value) != 0) {
            mylite_expression_value_deinit(&argument);
            return MYLITE_NOMEM;
        }
        break;
    case MYLITE_SQL_AST_AGGREGATE_COUNT:
    case MYLITE_SQL_AST_AGGREGATE_NONE:
        mylite_expression_value_deinit(&argument);
        return MYLITE_UNSUPPORTED;
    }

    mylite_expression_value_deinit(&argument);
    return MYLITE_OK;
}

static int evaluate_scalar_count_distinct_expression(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *arguments,
    const struct mylite_expression_eval_context *context, struct mylite_expression_value *out_value)
{
    bool has_null = false;

    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_EXPRESSION_LIST ||
        arguments->first_child == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *argument_node = arguments->first_child;
         argument_node != NULL; argument_node = argument_node->next_sibling) {
        struct mylite_expression_value argument = {0};
        int status = mylite_expression_eval_with_context(argument_node, context,
                                                         &stmt->scalar_result.warnings, &argument);

        if (status != 0) {
            mylite_expression_value_deinit(&argument);
            if (status == MYLITE_NOMEM) {
                (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
                return MYLITE_NOMEM;
            }
            if (stmt->database->error_message != NULL) {
                return status > 0 ? status : MYLITE_EXEC_ERROR;
            }
            return MYLITE_UNSUPPORTED;
        }
        if (argument.kind == MYLITE_EXPRESSION_VALUE_NULL) {
            has_null = true;
        }
        mylite_expression_value_deinit(&argument);
    }

    int64_t count = 1;

    if (has_null) {
        count = 0;
    }
    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_INT64,
        .int64_value = count,
    };
    return MYLITE_OK;
}

static int evaluate_scalar_numeric_aggregate_expression(
    mylite_stmt *stmt, enum mylite_sql_ast_aggregate_kind aggregate_kind,
    const struct mylite_expression_value *argument, struct mylite_expression_value *out_value)
{
    struct mylite_aggregate_numeric_value numeric = {0};
    int status =
        mylite_select_aggregate_value_to_double(&stmt->scalar_result.warnings, argument, &numeric);

    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        }
        return status;
    }
    if (aggregate_kind == MYLITE_SQL_AST_AGGREGATE_SUM && numeric.integral) {
        if (numeric.unsigned_value) {
            *out_value = (struct mylite_expression_value){
                .kind = MYLITE_EXPRESSION_VALUE_UINT64,
                .uint64_value = (uint64_t)numeric.value,
            };
            return MYLITE_OK;
        }
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = (int64_t)numeric.value,
        };
        return MYLITE_OK;
    }
    if (aggregate_kind == MYLITE_SQL_AST_AGGREGATE_AVG && numeric.integral) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_REAL,
            .real_value = numeric.value,
        };
        return MYLITE_OK;
    }

    status = mylite_select_aggregate_format_double(numeric.value, out_value);
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
    }
    return status;
}

static struct mylite_expression_eval_context
scalar_expression_eval_context(struct mylite_select_scalar_expression_context *context)
{
    return (struct mylite_expression_eval_context){
        .user_data = context,
        .eval_subquery = scalar_context_eval_subquery,
        .eval_in_subquery = scalar_context_eval_in_subquery,
        .eval_quantified_subquery = scalar_context_eval_quantified_subquery,
        .eval_row_subquery = scalar_context_eval_row_subquery,
        .eval_session_function = scalar_context_eval_session_function,
    };
}

static int scalar_context_eval_session_function(
    void *user_data, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value)
{
    struct mylite_select_scalar_expression_context *context = user_data;

    if (context == NULL || context->callbacks == NULL ||
        context->callbacks->eval_session_function == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    return context->callbacks->eval_session_function(context->stmt, function_call,
                                                     expression_context, warnings, out_value);
}

static int scalar_context_eval_subquery(void *user_data, const struct mylite_sql_ast_node *subquery,
                                        struct mylite_expression_warnings *warnings,
                                        struct mylite_expression_value *out_value)
{
    struct mylite_select_scalar_expression_context *context = user_data;

    if (context == NULL || context->callbacks == NULL ||
        context->callbacks->eval_subquery == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    return context->callbacks->eval_subquery(context->stmt, subquery, warnings, out_value);
}

static int scalar_context_eval_in_subquery(void *user_data,
                                           const struct mylite_sql_ast_node *expression,
                                           const struct mylite_expression_value *left,
                                           struct mylite_expression_warnings *warnings,
                                           struct mylite_expression_value *out_value)
{
    struct mylite_select_scalar_expression_context *context = user_data;

    if (context == NULL || context->callbacks == NULL ||
        context->callbacks->eval_in_subquery == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    return context->callbacks->eval_in_subquery(context->stmt, expression, left, warnings,
                                                out_value);
}

static int scalar_context_eval_quantified_subquery(void *user_data,
                                                   const struct mylite_sql_ast_node *expression,
                                                   const struct mylite_expression_value *left,
                                                   struct mylite_expression_warnings *warnings,
                                                   struct mylite_expression_value *out_value)
{
    struct mylite_select_scalar_expression_context *context = user_data;

    if (context == NULL || context->callbacks == NULL ||
        context->callbacks->eval_quantified_subquery == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    return context->callbacks->eval_quantified_subquery(context->stmt, expression, left, warnings,
                                                        out_value);
}

static int
scalar_context_eval_row_subquery(void *user_data, const struct mylite_sql_ast_node *expression,
                                 const struct mylite_expression_eval_context *expression_context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *out_value)
{
    struct mylite_select_scalar_expression_context *context = user_data;

    if (context == NULL || context->callbacks == NULL ||
        context->callbacks->eval_row_subquery == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    return context->callbacks->eval_row_subquery(context->stmt, expression, expression_context,
                                                 warnings, out_value);
}

static bool
scalar_eval_callbacks_are_valid(const struct mylite_select_scalar_eval_callbacks *callbacks)
{
    return (callbacks != NULL && callbacks->infer_expression_descriptor != NULL &&
            callbacks->eval_session_function != NULL && callbacks->eval_subquery != NULL &&
            callbacks->eval_in_subquery != NULL && callbacks->eval_quantified_subquery != NULL &&
            callbacks->eval_row_subquery != NULL &&
            callbacks->set_unsupported_order_error != NULL &&
            callbacks->set_ambiguous_order_column_error != NULL) != 0;
}
