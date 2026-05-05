#include "mylite_select_subquery.h"

#include "mylite_select_subquery_outer_reference.h"
#include "mylite_span.h"

static int
validate_in_subquery_expression(mylite_db *database, const struct mylite_sql_ast_node *expression,
                                const struct mylite_select_plan *outer_plan,
                                const struct mylite_select_subquery_bind_callbacks *callbacks);
static int
validate_row_subquery_expression(mylite_db *database, const struct mylite_sql_ast_node *expression,
                                 const struct mylite_select_plan *outer_plan,
                                 const struct mylite_select_subquery_bind_callbacks *callbacks);
static int validate_quantified_subquery_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_select_plan *outer_plan,
    const struct mylite_select_subquery_bind_callbacks *callbacks);
int mylite_select_subquery_bind_select_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression, bool scalar_context,
    const struct mylite_select_subquery_bind_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *select_statement = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(select_statement, 1U);
    mylite_stmt *subquery_stmt = NULL;
    int status = MYLITE_OK;

    if (callbacks == NULL || callbacks->prepare_select_subquery == NULL) {
        return MYLITE_MISUSE;
    }
    if (expression == NULL ||
        (expression->kind != MYLITE_SQL_AST_SUBQUERY_EXPRESSION &&
         expression->kind != MYLITE_SQL_AST_EXISTS_EXPRESSION) ||
        select_statement == NULL || select_statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return MYLITE_UNSUPPORTED;
    }

    if (scalar_context) {
        status = mylite_select_subquery_validate_scalar_select_list(database, select_statement);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (expression->kind == MYLITE_SQL_AST_EXISTS_EXPRESSION &&
        (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL)) {
        return MYLITE_OK;
    }

    status = callbacks->prepare_select_subquery(database, select_statement, &subquery_stmt);
    if (subquery_stmt != NULL) {
        mylite_finalize(subquery_stmt);
    }
    return status;
}

int mylite_select_subquery_bind_in_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_select_plan *outer_plan,
    const struct mylite_select_subquery_bind_callbacks *callbacks)
{
    return validate_in_subquery_expression(database, expression, outer_plan, callbacks);
}

int mylite_select_subquery_bind_row_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_select_plan *outer_plan,
    const struct mylite_select_subquery_bind_callbacks *callbacks)
{
    return validate_row_subquery_expression(database, expression, outer_plan, callbacks);
}

int mylite_select_subquery_bind_quantified_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_select_plan *outer_plan,
    const struct mylite_select_subquery_bind_callbacks *callbacks)
{
    return validate_quantified_subquery_expression(database, expression, outer_plan, callbacks);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int
validate_in_subquery_expression(mylite_db *database, const struct mylite_sql_ast_node *expression,
                                const struct mylite_select_plan *outer_plan,
                                const struct mylite_select_subquery_bind_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *select_statement = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(select_statement, 1U);
    mylite_stmt *subquery_stmt = NULL;
    int status = MYLITE_OK;

    if (callbacks == NULL || callbacks->prepare_select_subquery == NULL ||
        callbacks->set_unsupported_where_error == NULL) {
        return MYLITE_MISUSE;
    }
    if (!mylite_select_subquery_binary_expression_is_in(expression)) {
        return MYLITE_UNSUPPORTED;
    }
    if (mylite_select_subquery_references_outer_plan(select_statement, outer_plan,
                                                     select_statement)) {
        return callbacks->set_unsupported_where_error(database);
    }

    status = mylite_select_subquery_validate_in_select(database, select_statement);
    if (status != MYLITE_OK) {
        return status;
    }
    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        return MYLITE_OK;
    }

    status = callbacks->prepare_select_subquery(database, select_statement, &subquery_stmt);
    if (status != MYLITE_OK) {
        mylite_finalize(subquery_stmt);
        if (mylite_select_subquery_has_unqualified_outer_column_reference(select_statement,
                                                                          outer_plan)) {
            return callbacks->set_unsupported_where_error(database);
        }
        return status;
    }
    if (subquery_stmt != NULL) {
        status = mylite_select_subquery_validate_in_prepared_columns(database, subquery_stmt);
        mylite_finalize(subquery_stmt);
    }
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int
validate_row_subquery_expression(mylite_db *database, const struct mylite_sql_ast_node *expression,
                                 const struct mylite_select_plan *outer_plan,
                                 const struct mylite_select_subquery_bind_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *left =
        mylite_sql_ast_unwrap_parenthesized_expression(mylite_ast_child_at(expression, 0U));
    const struct mylite_sql_ast_node *select_statement =
        mylite_select_subquery_row_select_statement(expression);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(select_statement, 1U);
    mylite_stmt *subquery_stmt = NULL;
    size_t expected_width = mylite_select_subquery_row_constructor_width(left);
    int status = MYLITE_OK;

    if (callbacks == NULL || callbacks->prepare_select_subquery == NULL ||
        callbacks->set_unsupported_where_error == NULL) {
        return MYLITE_MISUSE;
    }
    if (!mylite_select_subquery_row_expression_is_supported(expression) || expected_width < 2U) {
        return MYLITE_UNSUPPORTED;
    }
    if (mylite_select_subquery_references_outer_plan(select_statement, outer_plan,
                                                     select_statement)) {
        return callbacks->set_unsupported_where_error(database);
    }
    if (mylite_select_subquery_row_expression_is_membership(expression) &&
        mylite_ast_find_child_kind(select_statement, MYLITE_SQL_AST_LIMIT_CLAUSE) != NULL) {
        return mylite_select_subquery_set_in_limit_error(database);
    }

    status = mylite_select_subquery_validate_row_select_columns(database, select_statement,
                                                                expected_width);
    if (status != MYLITE_OK) {
        return status;
    }
    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        return MYLITE_OK;
    }

    status = callbacks->prepare_select_subquery(database, select_statement, &subquery_stmt);
    if (status != MYLITE_OK) {
        mylite_finalize(subquery_stmt);
        if (mylite_select_subquery_has_unqualified_outer_column_reference(select_statement,
                                                                          outer_plan)) {
            return callbacks->set_unsupported_where_error(database);
        }
        return status;
    }
    if (subquery_stmt != NULL) {
        status = mylite_select_subquery_validate_row_prepared_columns(database, subquery_stmt,
                                                                      expected_width);
        mylite_finalize(subquery_stmt);
    }
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int validate_quantified_subquery_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_select_plan *outer_plan,
    const struct mylite_select_subquery_bind_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *select_statement = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(select_statement, 1U);
    mylite_stmt *subquery_stmt = NULL;
    int status = MYLITE_OK;

    if (callbacks == NULL || callbacks->prepare_select_subquery == NULL ||
        callbacks->set_unsupported_where_error == NULL) {
        return MYLITE_MISUSE;
    }
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_QUANTIFIED_COMPARISON ||
        !mylite_select_subquery_quantified_operator_is_supported(expression->operator_kind) ||
        expression->subquery_quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_NONE) {
        return MYLITE_UNSUPPORTED;
    }
    if (mylite_select_subquery_quantified_comparison_has_row_left(expression)) {
        return mylite_select_subquery_set_row_quantified_non_alias_error(database, expression);
    }
    if (mylite_select_subquery_references_outer_plan(select_statement, outer_plan,
                                                     select_statement)) {
        return callbacks->set_unsupported_where_error(database);
    }

    status = mylite_select_subquery_validate_in_select(database, select_statement);
    if (status != MYLITE_OK) {
        return status;
    }
    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        return MYLITE_OK;
    }

    status = callbacks->prepare_select_subquery(database, select_statement, &subquery_stmt);
    if (status != MYLITE_OK) {
        mylite_finalize(subquery_stmt);
        if (mylite_select_subquery_has_unqualified_outer_column_reference(select_statement,
                                                                          outer_plan)) {
            return callbacks->set_unsupported_where_error(database);
        }
        return status;
    }
    if (subquery_stmt != NULL) {
        status = mylite_select_subquery_validate_in_prepared_columns(database, subquery_stmt);
        mylite_finalize(subquery_stmt);
    }
    return status;
}

int mylite_select_subquery_validate_scalar_select_list(mylite_db *database,
                                                       const struct mylite_sql_ast_node *statement)
{
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(statement, 0U);
    size_t column_count = 0U;

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT ||
        select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return MYLITE_UNSUPPORTED;
    }
    for (const struct mylite_sql_ast_node *item = select_list->first_child; item != NULL;
         item = item->next_sibling) {
        ++column_count;
    }
    if (column_count != 1U) {
        return mylite_select_subquery_set_operand_columns_error(database);
    }
    return MYLITE_OK;
}

int mylite_select_subquery_validate_in_select(mylite_db *database,
                                              const struct mylite_sql_ast_node *statement)
{
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(statement, 0U);

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT ||
        select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return MYLITE_UNSUPPORTED;
    }
    if (mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE) != NULL) {
        return mylite_select_subquery_set_in_limit_error(database);
    }
    return mylite_select_subquery_validate_scalar_select_list(database, statement);
}

int mylite_select_subquery_validate_in_prepared_columns(mylite_db *database,
                                                        const mylite_stmt *stmt)
{
    if (stmt == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (mylite_column_count(stmt) != 1) {
        return mylite_select_subquery_set_operand_columns_error(database);
    }
    return MYLITE_OK;
}

int mylite_select_subquery_validate_row_select_columns(mylite_db *database,
                                                       const struct mylite_sql_ast_node *statement,
                                                       size_t expected_width)
{
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(statement, 0U);
    size_t column_count = 0U;
    bool has_wildcard = false;

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT ||
        select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return MYLITE_UNSUPPORTED;
    }
    for (const struct mylite_sql_ast_node *item = select_list->first_child; item != NULL;
         item = item->next_sibling) {
        const struct mylite_sql_ast_node *expression = mylite_ast_child_at(item, 0U);

        if (expression != NULL && expression->kind == MYLITE_SQL_AST_WILDCARD) {
            has_wildcard = true;
        }
        ++column_count;
    }
    if (!has_wildcard && column_count != expected_width) {
        return mylite_select_subquery_set_operand_column_count_error(database, expected_width);
    }
    return MYLITE_OK;
}

int mylite_select_subquery_validate_row_prepared_columns(mylite_db *database,
                                                         const mylite_stmt *stmt,
                                                         size_t expected_width)
{
    if (stmt == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (mylite_column_count(stmt) != (int)expected_width) {
        return mylite_select_subquery_set_operand_column_count_error(database, expected_width);
    }
    return MYLITE_OK;
}
