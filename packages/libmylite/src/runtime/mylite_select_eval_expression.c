#include "mylite_select_eval_expression.h"

#include "mylite_diagnostics.h"
#include "mylite_expression.h"
#include "mylite_field_descriptor.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_resolve.h"

#include <stdlib.h>

static int copy_table_select_row_value(const struct mylite_table_select_row *row,
                                       size_t column_index,
                                       struct mylite_expression_value *out_value);
static int evaluate_table_select_cached_constant_expression(
    void *user_data, const struct mylite_sql_ast_node *expression,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value);
static int evaluate_table_select_session_function(
    void *user_data, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value);
static int evaluate_table_select_aggregate_call(void *user_data,
                                                const struct mylite_sql_ast_node *aggregate,
                                                struct mylite_expression_value *out_value);
static int evaluate_table_select_subquery_expression(void *user_data,
                                                     const struct mylite_sql_ast_node *subquery,
                                                     struct mylite_expression_warnings *warnings,
                                                     struct mylite_expression_value *out_value);
static int evaluate_table_select_in_subquery_expression(
    void *user_data, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left, struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value);
static int evaluate_table_select_quantified_subquery_expression(
    void *user_data, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left, struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value);
static int evaluate_table_select_row_subquery_expression(
    void *user_data, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value);
static int resolve_table_select_expression_identifier(void *user_data,
                                                      const struct mylite_sql_ast_node *identifier,
                                                      struct mylite_expression_value *out_value);
static int copy_table_select_column_value(mylite_stmt *stmt, size_t column_index,
                                          const struct mylite_select_eval_callbacks *callbacks,
                                          struct mylite_expression_value *out_value);

void mylite_select_eval_context_init(struct mylite_table_select_eval_context *context,
                                     mylite_stmt *stmt, const struct mylite_table_select_row *row,
                                     const struct mylite_select_eval_callbacks *callbacks,
                                     bool order_resolution, bool having_resolution)
{
    *context = (struct mylite_table_select_eval_context){
        .stmt = stmt,
        .row = row,
        .callbacks = callbacks,
        .order_resolution = order_resolution,
        .having_resolution = having_resolution,
    };
}

void mylite_select_eval_expression_context_init(
    struct mylite_expression_eval_context *expression_context,
    struct mylite_table_select_eval_context *user_context)
{
    *expression_context = (struct mylite_expression_eval_context){
        .user_data = user_context,
        .resolve_identifier = resolve_table_select_expression_identifier,
        .eval_constant = evaluate_table_select_cached_constant_expression,
        .eval_aggregate = evaluate_table_select_aggregate_call,
        .eval_subquery = evaluate_table_select_subquery_expression,
        .eval_in_subquery = evaluate_table_select_in_subquery_expression,
        .eval_quantified_subquery = evaluate_table_select_quantified_subquery_expression,
        .eval_row_subquery = evaluate_table_select_row_subquery_expression,
        .eval_session_function = evaluate_table_select_session_function,
    };
}

int mylite_select_eval_cached_output_value(mylite_stmt *stmt,
                                           const struct mylite_table_select_row *row,
                                           size_t output_index,
                                           const struct mylite_select_eval_callbacks *callbacks,
                                           struct mylite_expression_value *out_value)
{
    if (row != NULL && row->output_value_count == stmt->select_plan.output_count &&
        output_index < row->output_value_count) {
        if (mylite_expression_value_copy(&row->output_values[output_index], out_value) != 0) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }
    return mylite_select_eval_output_value(stmt, row, output_index, callbacks, out_value);
}

int mylite_select_eval_output_value(mylite_stmt *stmt, const struct mylite_table_select_row *row,
                                    size_t output_index,
                                    const struct mylite_select_eval_callbacks *callbacks,
                                    struct mylite_expression_value *out_value)
{
    const struct mylite_select_output_column *output = &stmt->select_plan.outputs[output_index];

    if (output->kind == MYLITE_SELECT_OUTPUT_COLUMN) {
        if (copy_table_select_row_value(row, output->column_index, out_value) != 0) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }

    struct mylite_table_select_eval_context user_context = {0};
    struct mylite_expression_eval_context context = {0};
    int status = 0;

    mylite_select_eval_context_init(&user_context, stmt, row, callbacks, false, false);
    mylite_select_eval_expression_context_init(&context, &user_context);

    status = mylite_expression_eval_with_context(output->expression, &context,
                                                 &stmt->database->warnings, out_value);
    if (status != 0) {
        return mylite_select_eval_map_expression_status(stmt, status, callbacks);
    }
    return MYLITE_OK;
}

int mylite_select_eval_map_expression_status(mylite_stmt *stmt, int status,
                                             const struct mylite_select_eval_callbacks *callbacks)
{
    if (status == 0) {
        return MYLITE_OK;
    }
    if (status == MYLITE_NOMEM) {
        if (stmt != NULL && stmt->database != NULL && stmt->database->error_message == NULL) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        }
        return MYLITE_NOMEM;
    }
    if (stmt != NULL && stmt->database != NULL && stmt->database->error_message != NULL) {
        return status > 0 ? status : MYLITE_EXEC_ERROR;
    }
    if (stmt == NULL || stmt->database == NULL || callbacks == NULL ||
        callbacks->set_expression_eval_error == NULL) {
        return MYLITE_EXEC_ERROR;
    }
    return callbacks->set_expression_eval_error(stmt);
}

static int copy_table_select_row_value(const struct mylite_table_select_row *row,
                                       size_t column_index,
                                       struct mylite_expression_value *out_value)
{
    if (row == NULL || column_index >= row->value_count) {
        return -1;
    }
    return mylite_expression_value_copy(&row->values[column_index], out_value);
}

static int evaluate_table_select_cached_constant_expression(
    void *user_data, const struct mylite_sql_ast_node *expression,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value)
{
    struct mylite_table_select_eval_context *context = user_data;
    mylite_stmt *stmt = context == NULL ? NULL : context->stmt;
    struct mylite_cached_expression_value *entry = NULL;

    if (stmt == NULL || expression == NULL) {
        return -1;
    }

    for (size_t index = 0U; index < stmt->select_constant_value_count; ++index) {
        if (stmt->select_constant_values[index].expression == expression) {
            entry = &stmt->select_constant_values[index];
            break;
        }
    }
    if (entry == NULL) {
        struct mylite_cached_expression_value *values =
            realloc(stmt->select_constant_values, (stmt->select_constant_value_count + 1U) *
                                                      sizeof(*stmt->select_constant_values));

        if (values == NULL) {
            return -1;
        }
        stmt->select_constant_values = values;
        entry = &stmt->select_constant_values[stmt->select_constant_value_count++];
        *entry = (struct mylite_cached_expression_value){.expression = expression};
    }

    if (!entry->evaluated) {
        entry->status = mylite_expression_eval(expression, warnings, &entry->value);
        entry->evaluated = true;
    }
    if (entry->status != 0) {
        return entry->status;
    }
    return mylite_expression_value_copy(&entry->value, out_value);
}

static int evaluate_table_select_session_function(
    void *user_data, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value)
{
    struct mylite_table_select_eval_context *context = user_data;

    return context == NULL || context->callbacks == NULL ||
                   context->callbacks->eval_session_function == NULL
               ? MYLITE_UNSUPPORTED
               : context->callbacks->eval_session_function(
                     context->stmt, function_call, expression_context, warnings, NULL, out_value);
}

static int evaluate_table_select_aggregate_call(void *user_data,
                                                const struct mylite_sql_ast_node *aggregate,
                                                struct mylite_expression_value *out_value)
{
    struct mylite_table_select_eval_context *context = user_data;

    if (context == NULL || context->stmt == NULL || context->row == NULL || aggregate == NULL) {
        return -1;
    }

    for (size_t index = 0U; index < context->stmt->select_plan.aggregate_binding_count; ++index) {
        if (context->stmt->select_plan.aggregate_bindings[index].call == aggregate) {
            if (index >= context->row->aggregate_value_count) {
                return -1;
            }
            return mylite_expression_value_copy(&context->row->aggregate_values[index], out_value);
        }
    }
    return -1;
}

static int evaluate_table_select_subquery_expression(void *user_data,
                                                     const struct mylite_sql_ast_node *subquery,
                                                     struct mylite_expression_warnings *warnings,
                                                     struct mylite_expression_value *out_value)
{
    struct mylite_table_select_eval_context *context = user_data;

    if (context == NULL || context->callbacks == NULL ||
        context->callbacks->eval_subquery == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    return context->callbacks->eval_subquery(context->stmt, subquery, warnings, out_value);
}

static int evaluate_table_select_in_subquery_expression(
    void *user_data, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left, struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value)
{
    struct mylite_table_select_eval_context *context = user_data;

    if (context == NULL || context->callbacks == NULL ||
        context->callbacks->eval_in_subquery == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    return context->callbacks->eval_in_subquery(context->stmt, expression, left, warnings,
                                                out_value);
}

static int evaluate_table_select_quantified_subquery_expression(
    void *user_data, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left, struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value)
{
    struct mylite_table_select_eval_context *context = user_data;

    if (context == NULL || context->callbacks == NULL ||
        context->callbacks->eval_quantified_subquery == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    return context->callbacks->eval_quantified_subquery(context->stmt, expression, left, warnings,
                                                        out_value);
}

static int evaluate_table_select_row_subquery_expression(
    void *user_data, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value)
{
    struct mylite_table_select_eval_context *context = user_data;

    if (context == NULL || context->callbacks == NULL ||
        context->callbacks->eval_row_subquery == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    return context->callbacks->eval_row_subquery(context->stmt, expression, expression_context,
                                                 warnings, out_value);
}

static int resolve_table_select_expression_identifier(void *user_data,
                                                      const struct mylite_sql_ast_node *identifier,
                                                      struct mylite_expression_value *out_value)
{
    struct mylite_table_select_eval_context *context = user_data;
    const struct mylite_select_eval_callbacks *callbacks =
        context == NULL ? NULL : context->callbacks;
    size_t column_index = 0U;
    int status = MYLITE_OK;

    if (context == NULL || context->stmt == NULL || callbacks == NULL) {
        return -1;
    }

    if (context->having_resolution) {
        enum mylite_select_order_key_kind kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
        size_t index = 0U;

        status = callbacks->resolve_having_reference(
            context->stmt->database, &context->stmt->select_plan, identifier, &kind, &index, false);
        if (status != MYLITE_OK) {
            return -1;
        }
        if (kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
            return mylite_select_eval_cached_output_value(context->stmt, context->row, index,
                                                          callbacks, out_value) == MYLITE_OK
                       ? 0
                       : -1;
        }
    }

    if (context->order_resolution) {
        enum mylite_select_order_key_kind kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
        size_t index = 0U;

        status = callbacks->resolve_order_reference(
            context->stmt->database, &context->stmt->select_plan, identifier, &kind, &index);
        if (status != MYLITE_OK) {
            return -1;
        }
        if (kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
            return mylite_select_eval_cached_output_value(context->stmt, context->row, index,
                                                          callbacks, out_value) == MYLITE_OK
                       ? 0
                       : -1;
        }
    }

    status = mylite_select_resolve_plan_column_reference(context->stmt->database,
                                                         &context->stmt->select_plan, identifier,
                                                         "field list", &column_index);
    if (status != MYLITE_OK ||
        column_index == mylite_select_plan_column_count(&context->stmt->select_plan)) {
        return -1;
    }
    if (context->row != NULL) {
        return copy_table_select_row_value(context->row, column_index, out_value);
    }
    return copy_table_select_column_value(context->stmt, column_index, callbacks, out_value);
}

static int copy_table_select_column_value(mylite_stmt *stmt, size_t column_index,
                                          const struct mylite_select_eval_callbacks *callbacks,
                                          struct mylite_expression_value *out_value)
{
    const struct mylite_select_column *column = NULL;
    int status = 0;

    if (callbacks == NULL || callbacks->copy_column_value == NULL ||
        column_index >= mylite_select_plan_column_count(&stmt->select_plan)) {
        return -1;
    }

    status = callbacks->copy_column_value(stmt, column_index, out_value);
    if (status == 0) {
        column = mylite_select_plan_column_const(&stmt->select_plan, column_index, NULL);
        out_value->preserve_temporal_fraction_digits =
            mylite_field_descriptor_preserves_temporal_fraction_digits(
                column == NULL ? NULL : &column->descriptor);
        out_value->temporal_type = mylite_field_descriptor_expression_temporal_type(
            column == NULL ? NULL : &column->descriptor);
    }
    return status;
}
