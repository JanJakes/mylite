#include "mylite_select_scalar.h"

#include "mylite_diagnostics.h"
#include "mylite_expression_validation.h"
#include "mylite_metadata.h"
#include "mylite_select.h"
#include "mylite_select_scalar_order_validate.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "mylite_statement_ast.h"

#include <stdlib.h>

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
        column_count,
        sizeof(*stmt->scalar_result.expressions)); // NOLINT(bugprone-sizeof-expression)
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
    int status =
        mylite_statement_ast_clone_subtree(&stmt->scalar_select_ast, expression, source_sql,
                                           stmt->scalar_select_sql_text, source_sql_length, &clone);

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
