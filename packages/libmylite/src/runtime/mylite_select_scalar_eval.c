#include "mylite_select_scalar.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_select.h"
#include "mylite_select_aggregate.h"
#include "mylite_select_order_resolve.h"
#include "mylite_select_subquery.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "sql/mylite_ast.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    mylite_scalar_group_concat_max_len = 1024U,
    mylite_scalar_group_concat_warning_size = 64U,
};

struct mylite_select_scalar_expression_context {
    mylite_stmt *stmt;
    const struct mylite_select_scalar_eval_callbacks *callbacks;
};

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
static int
evaluate_scalar_group_concat_expression(mylite_stmt *stmt,
                                        const struct mylite_sql_ast_node *expression,
                                        const struct mylite_expression_eval_context *context,
                                        struct mylite_expression_value *out_value);
static int validate_scalar_group_concat_order_by(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *expression, size_t argument_count,
    const struct mylite_expression_eval_context *context);
static int validate_scalar_group_concat_order_item(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *order_item, size_t argument_count,
    const struct mylite_expression_eval_context *context);
static int
set_scalar_group_concat_unknown_order_column_error(mylite_stmt *stmt,
                                                   const struct mylite_sql_ast_node *expression);
static int append_scalar_group_concat_value(mylite_stmt *stmt, char **result, size_t *result_length,
                                            const struct mylite_expression_value *value,
                                            bool *truncated);
static int append_scalar_group_concat_truncation_warning(mylite_stmt *stmt);
static size_t scalar_group_concat_value_text_length(const struct mylite_expression_value *value,
                                                    const char *text);
static int evaluate_scalar_numeric_aggregate_expression(
    mylite_stmt *stmt, enum mylite_sql_ast_aggregate_kind aggregate_kind,
    const struct mylite_expression_value *argument, struct mylite_expression_value *out_value);
static struct mylite_expression_eval_context
scalar_expression_eval_context(struct mylite_select_scalar_expression_context *context);
static int scalar_context_eval_session_function(
    void *user_data, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value);
static int scalar_context_eval_default_function(void *user_data,
                                                const struct mylite_sql_ast_node *function_call,
                                                struct mylite_expression_value *out_value);
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
    if (expression->aggregate_kind == MYLITE_SQL_AST_AGGREGATE_GROUP_CONCAT) {
        return evaluate_scalar_group_concat_expression(stmt, expression, &context, out_value);
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
    case MYLITE_SQL_AST_AGGREGATE_GROUP_CONCAT:
    case MYLITE_SQL_AST_AGGREGATE_COUNT:
    case MYLITE_SQL_AST_AGGREGATE_NONE:
        mylite_expression_value_deinit(&argument);
        return MYLITE_UNSUPPORTED;
    }

    mylite_expression_value_deinit(&argument);
    return MYLITE_OK;
}

static int evaluate_scalar_group_concat_expression(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_eval_context *context, struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    size_t argument_count = mylite_sql_ast_node_child_count(arguments);
    struct mylite_expression_value *values = NULL;
    char *result = NULL;
    size_t result_length = 0U;
    bool has_null = false;
    bool truncated = false;
    int status = MYLITE_OK;

    if (arguments == NULL ||
        (arguments->kind != MYLITE_SQL_AST_EXPRESSION_LIST &&
         arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) ||
        argument_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    values = calloc(argument_count, sizeof(*values));
    if (values == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = validate_scalar_group_concat_order_by(stmt, expression, argument_count, context);
    if (status != MYLITE_OK) {
        goto cleanup;
    }

    size_t index = 0U;
    for (const struct mylite_sql_ast_node *argument = arguments->first_child; argument != NULL;
         argument = argument->next_sibling) {
        int eval_status = mylite_expression_eval_with_context(
            argument, context, &stmt->scalar_result.warnings, &values[index]);

        if (eval_status != 0) {
            if (eval_status == MYLITE_NOMEM) {
                (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
                status = MYLITE_NOMEM;
            } else if (stmt->database->error_message != NULL) {
                status = eval_status > 0 ? eval_status : MYLITE_EXEC_ERROR;
            } else {
                status = MYLITE_UNSUPPORTED;
            }
            goto cleanup;
        }
        if (values[index].kind == MYLITE_EXPRESSION_VALUE_NULL) {
            has_null = true;
        }
        ++index;
    }

    if (has_null) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    for (index = 0U; status == MYLITE_OK && index < argument_count; ++index) {
        status = append_scalar_group_concat_value(stmt, &result, &result_length, &values[index],
                                                  &truncated);
    }
    if (status != MYLITE_OK) {
        goto cleanup;
    }
    if (result == NULL) {
        result = mylite_copy_span_text("", 0U);
        if (result == NULL) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            status = MYLITE_NOMEM;
            goto cleanup;
        }
    }

    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_TEXT,
        .text_value = result,
        .text_length = result_length,
    };
    result = NULL;

cleanup:
    for (size_t cleanup_index = 0U; cleanup_index < argument_count; ++cleanup_index) {
        mylite_expression_value_deinit(&values[cleanup_index]);
    }
    free(values);
    free(result);
    return status;
}

static int validate_scalar_group_concat_order_by(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *expression, size_t argument_count,
    const struct mylite_expression_eval_context *context)
{
    const struct mylite_sql_ast_node *order_by =
        mylite_ast_find_child_kind(expression, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    const struct mylite_sql_ast_node *items =
        order_by == NULL ? NULL : mylite_ast_child_at(order_by, 0U);

    for (const struct mylite_sql_ast_node *item = items == NULL ? NULL : items->first_child;
         item != NULL; item = item->next_sibling) {
        int status = validate_scalar_group_concat_order_item(stmt, item, argument_count, context);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int validate_scalar_group_concat_order_item(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *order_item, size_t argument_count,
    const struct mylite_expression_eval_context *context)
{
    const struct mylite_sql_ast_node *expression = mylite_ast_child_at(order_item, 0U);
    struct mylite_expression_value value = {0};
    int eval_status = 0;

    if (expression == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        expression->literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        uint64_t ordinal = 0U;

        if (!mylite_select_parse_uint64_span(expression->span, &ordinal) || ordinal == 0U ||
            ordinal > argument_count) {
            return set_scalar_group_concat_unknown_order_column_error(stmt, expression);
        }
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return set_scalar_group_concat_unknown_order_column_error(stmt, expression);
    }

    eval_status = mylite_expression_eval_with_context(expression, context,
                                                      &stmt->scalar_result.warnings, &value);
    mylite_expression_value_deinit(&value);
    if (eval_status == 0) {
        return MYLITE_OK;
    }
    if (eval_status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (stmt->database->error_message != NULL) {
        return eval_status > 0 ? eval_status : MYLITE_EXEC_ERROR;
    }
    return MYLITE_UNSUPPORTED;
}

static int
set_scalar_group_concat_unknown_order_column_error(mylite_stmt *stmt,
                                                   const struct mylite_sql_ast_node *expression)
{
    char *reference = mylite_copy_span_text(expression->span.text, expression->span.length);
    int status = MYLITE_OK;

    if (reference == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_select_set_unknown_order_column_error(stmt->database, reference);
    free(reference);
    return status;
}

static int append_scalar_group_concat_value(mylite_stmt *stmt, char **result, size_t *result_length,
                                            const struct mylite_expression_value *value,
                                            bool *truncated)
{
    char *text = mylite_expression_value_to_text(value);
    size_t text_length = scalar_group_concat_value_text_length(value, text);
    size_t remaining = 0U;
    size_t append_length = text_length;
    char *buffer = NULL;
    int status = MYLITE_OK;

    if (text == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (*truncated || text_length == 0U) {
        free(text);
        return MYLITE_OK;
    }
    if (*result_length >= mylite_scalar_group_concat_max_len) {
        free(text);
        *truncated = true;
        return append_scalar_group_concat_truncation_warning(stmt);
    }

    remaining = mylite_scalar_group_concat_max_len - *result_length;
    if (append_length > remaining) {
        append_length = remaining;
        *truncated = true;
    }

    buffer = realloc(*result, *result_length + append_length + 1U);
    if (buffer == NULL) {
        free(text);
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    *result = buffer;
    if (append_length > 0U) {
        memcpy(*result + *result_length, text, append_length);
        *result_length += append_length;
    }
    (*result)[*result_length] = '\0';

    if (*truncated) {
        status = append_scalar_group_concat_truncation_warning(stmt);
    }
    free(text);
    return status;
}

static int append_scalar_group_concat_truncation_warning(mylite_stmt *stmt)
{
    char message[mylite_scalar_group_concat_warning_size] = {0};
    int length = snprintf(message, sizeof(message), "Row 1 was cut by GROUP_CONCAT()");

    if (length < 0 || (size_t)length >= sizeof(message) ||
        mylite_expression_warnings_append(&stmt->scalar_result.warnings,
                                          MYLITE_MYSQL_ER_CUT_VALUE_GROUP_CONCAT, message) != 0) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static size_t scalar_group_concat_value_text_length(const struct mylite_expression_value *value,
                                                    const char *text)
{
    if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return value->text_length;
    }
    return text == NULL ? 0U : strlen(text);
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
        .eval_default_function = scalar_context_eval_default_function,
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

static int scalar_context_eval_default_function(void *user_data,
                                                const struct mylite_sql_ast_node *function_call,
                                                struct mylite_expression_value *out_value)
{
    struct mylite_select_scalar_expression_context *context = user_data;
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(function_call, 1U);
    const struct mylite_sql_ast_node *identifier =
        arguments == NULL ? NULL : mylite_ast_child_at(arguments, 0U);
    char *reference = NULL;
    int status = MYLITE_OK;

    (void)out_value;
    if (context == NULL || context->stmt == NULL || context->stmt->database == NULL ||
        identifier == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    reference = mylite_copy_span_text(identifier->span.text, identifier->span.length);
    if (reference == NULL) {
        (void)mylite_diagnostics_set_error_message(context->stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message_parts(context->stmt->database, "Unknown column '",
                                                        reference, "' in 'field list'");
    free(reference);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(context->stmt->database,
                                                 MYLITE_MYSQL_ER_BAD_FIELD_ERROR,
                                                 mylite_error_message(context->stmt->database));
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
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
