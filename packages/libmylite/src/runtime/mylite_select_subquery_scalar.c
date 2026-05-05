#include "mylite_select_subquery.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_select_materialize.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <stdint.h>

static int
evaluate_scalar_subquery_expression(mylite_stmt *stmt, const struct mylite_sql_ast_node *subquery,
                                    struct mylite_expression_value *out_value,
                                    const struct mylite_select_subquery_eval_callbacks *callbacks);
static int
evaluate_exists_subquery_expression(mylite_stmt *stmt, const struct mylite_sql_ast_node *subquery,
                                    struct mylite_expression_value *out_value,
                                    const struct mylite_select_subquery_eval_callbacks *callbacks);
static int
subquery_statement_has_row(mylite_stmt *stmt, bool *out_has_row,
                           const struct mylite_select_subquery_eval_callbacks *callbacks);

int mylite_select_subquery_eval(mylite_stmt *stmt, const struct mylite_sql_ast_node *subquery,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value,
                                const struct mylite_select_subquery_eval_callbacks *callbacks)
{
    struct mylite_expression_warnings saved_warnings = {0};
    struct mylite_expression_warnings subquery_warnings = {0};
    int status = MYLITE_UNSUPPORTED;

    if (stmt == NULL || stmt->database == NULL || subquery == NULL || callbacks == NULL ||
        callbacks->prepare_select_subquery == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    saved_warnings = stmt->database->warnings;
    stmt->database->warnings = (struct mylite_expression_warnings){0};

    switch (subquery->kind) {
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
        status = evaluate_scalar_subquery_expression(stmt, subquery, out_value, callbacks);
        break;
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
        status = evaluate_exists_subquery_expression(stmt, subquery, out_value, callbacks);
        break;
    default:
        status = MYLITE_UNSUPPORTED;
        break;
    }

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

static int
evaluate_scalar_subquery_expression(mylite_stmt *stmt, const struct mylite_sql_ast_node *subquery,
                                    struct mylite_expression_value *out_value,
                                    const struct mylite_select_subquery_eval_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *select_statement = mylite_ast_child_at(subquery, 0U);
    mylite_stmt *subquery_stmt = NULL;
    int status =
        mylite_select_subquery_validate_scalar_select_list(stmt->database, select_statement);

    if (status != MYLITE_OK) {
        return status;
    }

    status = callbacks->prepare_select_subquery(stmt->database, select_statement, &subquery_stmt);
    if (status != MYLITE_OK) {
        return status;
    }

    status = mylite_step(subquery_stmt);
    if (status == MYLITE_DONE) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        mylite_finalize(subquery_stmt);
        return MYLITE_OK;
    }
    if (status != MYLITE_ROW) {
        mylite_finalize(subquery_stmt);
        return status;
    }

    status = mylite_select_subquery_copy_column_value(subquery_stmt, out_value);
    if (status != MYLITE_OK) {
        mylite_finalize(subquery_stmt);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        }
        return status;
    }

    status = mylite_step(subquery_stmt);
    if (status == MYLITE_ROW) {
        mylite_expression_value_deinit(out_value);
        status = mylite_select_subquery_set_scalar_cardinality_error(stmt->database);
    } else if (status == MYLITE_DONE) {
        status = MYLITE_OK;
    }
    mylite_finalize(subquery_stmt);
    return status;
}

static int
evaluate_exists_subquery_expression(mylite_stmt *stmt, const struct mylite_sql_ast_node *subquery,
                                    struct mylite_expression_value *out_value,
                                    const struct mylite_select_subquery_eval_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *select_statement = mylite_ast_child_at(subquery, 0U);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(select_statement, 1U);
    mylite_stmt *subquery_stmt = NULL;
    bool has_row = false;
    bool negated = subquery->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT;
    int64_t exists_value = 0;
    int status = MYLITE_OK;

    if (select_statement == NULL || select_statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return MYLITE_UNSUPPORTED;
    }
    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        has_row = true;
    } else {
        status =
            callbacks->prepare_select_subquery(stmt->database, select_statement, &subquery_stmt);
        if (status != MYLITE_OK) {
            return status;
        }
        if (subquery_stmt == NULL) {
            return MYLITE_UNSUPPORTED;
        }
        status = subquery_statement_has_row(subquery_stmt, &has_row, callbacks);
        mylite_finalize(subquery_stmt);
        if (status != MYLITE_OK) {
            return status;
        }
    }

    if (negated) {
        if (has_row) {
            has_row = false;
        } else {
            has_row = true;
        }
    }
    if (has_row) {
        exists_value = 1;
    }
    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                  .int64_value = exists_value};
    return MYLITE_OK;
}

static int subquery_statement_has_row(mylite_stmt *stmt, bool *out_has_row,
                                      const struct mylite_select_subquery_eval_callbacks *callbacks)
{
    int status = MYLITE_OK;

    *out_has_row = false;
    if (stmt == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (stmt->kind == MYLITE_STMT_SCALAR_SELECT) {
        *out_has_row = true;
        stmt->executed = true;
        stmt->affected_rows = -1;
        return MYLITE_OK;
    }
    if (stmt->kind == MYLITE_STMT_TABLE_SELECT) {
        enum mylite_sql_ast_select_duplicate_mode duplicate_mode = stmt->select_plan.duplicate_mode;
        size_t order_key_count = stmt->select_plan.order_key_count;

        if (callbacks->table_select_eval_callbacks == NULL) {
            return MYLITE_UNSUPPORTED;
        }
        stmt->select_plan.duplicate_mode = MYLITE_SQL_AST_SELECT_DUPLICATES_IMPLICIT_ALL;
        stmt->select_plan.order_key_count = 0U;
        status =
            mylite_select_materialize_table_result(stmt, callbacks->table_select_eval_callbacks);
        stmt->select_plan.duplicate_mode = duplicate_mode;
        stmt->select_plan.order_key_count = order_key_count;
        if (status != MYLITE_OK) {
            return status;
        }
        stmt->executed = true;
        stmt->affected_rows = -1;
        *out_has_row = stmt->select_result.row_count != 0U;
        return MYLITE_OK;
    }

    status = mylite_step(stmt);
    if (status == MYLITE_ROW) {
        *out_has_row = true;
        return MYLITE_OK;
    }
    if (status == MYLITE_DONE) {
        return MYLITE_OK;
    }
    return status;
}
