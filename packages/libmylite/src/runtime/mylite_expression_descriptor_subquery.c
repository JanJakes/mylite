#include "mylite_expression_descriptor_subquery.h"

#include "mylite_expression_descriptor.h"
#include "mylite_metadata.h"
#include "mylite_select_subquery.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

static int infer_scalar_subquery_expression_descriptor(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_subquery_callbacks *callbacks);
static int infer_in_subquery_expression_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_subquery_callbacks *callbacks);
static int infer_row_subquery_expression_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_subquery_callbacks *callbacks);
static int infer_quantified_subquery_expression_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_subquery_callbacks *callbacks);
static int validate_subquery_descriptor_callbacks(
    const struct mylite_expression_descriptor_subquery_callbacks *callbacks);

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_expression_descriptor_infer_subquery_expression(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_subquery_callbacks *callbacks)
{
    int status = validate_subquery_descriptor_callbacks(callbacks);

    if (status != MYLITE_OK) {
        return status;
    }
    if (expression == NULL) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
        return infer_scalar_subquery_expression_descriptor(database, expression, out_descriptor,
                                                           callbacks);
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
        *out_descriptor = mylite_expression_descriptor_boolean(false);
        return MYLITE_OK;
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
        return infer_quantified_subquery_expression_descriptor(database, plan, expression,
                                                               out_descriptor, callbacks);
    default:
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_expression_descriptor_infer_binary_subquery_expression(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_subquery_callbacks *callbacks)
{
    if (mylite_select_subquery_binary_expression_is_row(expression)) {
        int status = validate_subquery_descriptor_callbacks(callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
        return infer_row_subquery_expression_descriptor(database, plan, expression, out_descriptor,
                                                        callbacks);
    }
    if (mylite_select_subquery_binary_expression_is_in(expression)) {
        int status = validate_subquery_descriptor_callbacks(callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
        return infer_in_subquery_expression_descriptor(database, plan, expression, out_descriptor,
                                                       callbacks);
    }

    *out_descriptor = mylite_expression_descriptor_defaults();
    return MYLITE_UNSUPPORTED;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_scalar_subquery_expression_descriptor(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_subquery_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *select_statement = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(select_statement, 0U);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(select_statement, 1U);
    const struct mylite_sql_ast_node *select_item = NULL;
    const struct mylite_sql_ast_node *select_expression = NULL;
    mylite_stmt *subquery_stmt = NULL;
    int status = mylite_select_subquery_validate_scalar_select_list(database, select_statement);

    if (status != MYLITE_OK) {
        return status;
    }

    select_item = select_list == NULL ? NULL : select_list->first_child;
    select_expression = mylite_ast_child_at(select_item, 0U);
    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        return callbacks->infer_expression_descriptor(database, NULL, select_expression, NULL,
                                                      out_descriptor);
    }

    status = callbacks->prepare_select_subquery(database, select_statement, &subquery_stmt);
    if (status != MYLITE_OK) {
        return status;
    }
    if (subquery_stmt == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    const struct mylite_result_column_metadata *metadata =
        mylite_result_metadata_column(subquery_stmt, 0);
    if (metadata != NULL) {
        *out_descriptor = metadata->descriptor;
    } else if (mylite_column_count(subquery_stmt) == 1) {
        *out_descriptor = mylite_expression_descriptor_defaults();
    } else {
        status = mylite_select_subquery_set_operand_columns_error(database);
    }
    mylite_finalize(subquery_stmt);
    if (status == MYLITE_OK) {
        mylite_expression_descriptor_set_scalar_subquery_nullable(out_descriptor);
    }
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_in_subquery_expression_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_subquery_callbacks *callbacks)
{
    struct mylite_field_descriptor left = mylite_expression_descriptor_defaults();
    const struct mylite_sql_ast_node *left_expression = mylite_ast_child_at(expression, 0U);
    int status = MYLITE_OK;

    if (!mylite_select_subquery_binary_expression_is_in(expression)) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }
    if (left_expression == NULL || left_expression->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }

    status = callbacks->infer_expression_descriptor(database, plan, left_expression, NULL, &left);
    if (status != MYLITE_OK) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status;
    }
    status = mylite_select_subquery_bind_in_expression(database, expression, plan,
                                                       callbacks->bind_callbacks);
    if (status != MYLITE_OK) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status;
    }

    *out_descriptor = mylite_expression_descriptor_boolean(true);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_row_subquery_expression_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_subquery_callbacks *callbacks)
{
    int status = MYLITE_OK;

    if (!mylite_select_subquery_binary_expression_is_row(expression)) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }

    status = mylite_select_subquery_bind_row_expression(database, expression, plan,
                                                        callbacks->bind_callbacks);
    if (status != MYLITE_OK) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status;
    }

    *out_descriptor = mylite_expression_descriptor_boolean(expression->operator_kind !=
                                                           MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_quantified_subquery_expression_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_subquery_callbacks *callbacks)
{
    struct mylite_field_descriptor left = mylite_expression_descriptor_defaults();
    const struct mylite_sql_ast_node *left_expression = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *unwrapped_left =
        mylite_sql_ast_unwrap_parenthesized_expression(left_expression);
    int status = MYLITE_OK;

    if (mylite_select_subquery_quantified_comparison_is_row_alias(expression)) {
        status = mylite_select_subquery_bind_row_expression(database, expression, plan,
                                                            callbacks->bind_callbacks);
        if (status != MYLITE_OK) {
            *out_descriptor = mylite_expression_descriptor_defaults();
            return status;
        }

        *out_descriptor = mylite_expression_descriptor_boolean(true);
        return MYLITE_OK;
    }

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_QUANTIFIED_COMPARISON ||
        !mylite_select_subquery_quantified_operator_is_supported(expression->operator_kind)) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }
    if (unwrapped_left == NULL) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }
    if (unwrapped_left->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return mylite_select_subquery_set_row_quantified_non_alias_error(database, expression);
    }

    status = callbacks->infer_expression_descriptor(database, plan, left_expression, NULL, &left);
    if (status != MYLITE_OK) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status;
    }
    status = mylite_select_subquery_bind_quantified_expression(database, expression, plan,
                                                               callbacks->bind_callbacks);
    if (status != MYLITE_OK) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status;
    }

    *out_descriptor = mylite_expression_descriptor_boolean(true);
    return MYLITE_OK;
}

static int validate_subquery_descriptor_callbacks(
    const struct mylite_expression_descriptor_subquery_callbacks *callbacks)
{
    if (callbacks == NULL || callbacks->infer_expression_descriptor == NULL ||
        callbacks->prepare_select_subquery == NULL || callbacks->bind_callbacks == NULL) {
        return MYLITE_MISUSE;
    }
    return MYLITE_OK;
}
