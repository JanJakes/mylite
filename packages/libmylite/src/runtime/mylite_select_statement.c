#include "mylite_select_statement.h"

#include "mylite_diagnostics.h"
#include "mylite_select.h"
#include "mylite_select_aggregate_bind.h"
#include "mylite_select_eval.h"
#include "mylite_select_materialize.h"
#include "mylite_select_metadata.h"
#include "mylite_select_rowset.h"
#include "mylite_select_sql.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "sqlite3.h"

#include <stdlib.h>

static bool
select_statement_callbacks_are_valid(const struct mylite_select_statement_callbacks *callbacks);
static int
clone_table_select_expressions(mylite_stmt *stmt, const struct mylite_sql_ast_node *where_clause,
                               const char *sql, size_t sql_length,
                               const struct mylite_select_statement_callbacks *callbacks);
static int clone_table_select_join_expressions(mylite_stmt *stmt, const char *sql,
                                               size_t sql_length);
static int clone_table_select_output_expressions(mylite_stmt *stmt, const char *sql,
                                                 size_t sql_length);
static int clone_table_select_group_expressions(mylite_stmt *stmt, const char *sql,
                                                size_t sql_length);
static int clone_table_select_having_expression(mylite_stmt *stmt, const char *sql,
                                                size_t sql_length);
static int
collect_table_select_aggregate_bindings(mylite_stmt *stmt,
                                        const struct mylite_select_statement_callbacks *callbacks);
static int collect_table_select_expression_aggregate_bindings(
    mylite_stmt *stmt, const struct mylite_select_statement_callbacks *callbacks);
static int collect_table_select_order_aggregate_bindings(
    mylite_stmt *stmt, const struct mylite_select_statement_callbacks *callbacks);
static int clone_table_select_expression_node(mylite_stmt *stmt,
                                              const struct mylite_sql_ast_node *expression,
                                              const char *source_sql, size_t sql_length,
                                              struct mylite_sql_ast_node **out_node);

int mylite_select_prepare_custom_table_statement(
    mylite_db *database, const struct mylite_sql_ast_node *where_clause, const char *sql,
    size_t sql_length, struct mylite_select_plan *plan, mylite_stmt **out_stmt,
    const struct mylite_select_statement_callbacks *callbacks)
{
    sqlite3_stmt *sqlite_stmt = NULL;
    mylite_stmt *stmt = NULL;
    char *scan_sql = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (!select_statement_callbacks_are_valid(callbacks)) {
        return MYLITE_MISUSE;
    }
    if (mylite_select_plan_table_count(plan) <= 1U) {
        scan_sql = mylite_select_build_scan_sql(database, plan);
        if (scan_sql == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }

        rc = sqlite3_prepare_v3(database->sqlite, scan_sql, -1, SQLITE_PREPARE_PERSISTENT,
                                &sqlite_stmt, NULL);
        sqlite3_free(scan_sql);
        if (rc != SQLITE_OK) {
            return mylite_diagnostics_set_sqlite_error(database);
        }
    }

    stmt = calloc(1U, sizeof(*stmt));
    if (stmt == NULL) {
        sqlite3_finalize(sqlite_stmt);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *stmt = (mylite_stmt){
        .database = database,
        .kind = MYLITE_STMT_TABLE_SELECT,
        .sqlite_stmt = sqlite_stmt,
        .affected_rows = -1,
    };

    status = mylite_select_attach_result_metadata(stmt, plan, callbacks->metadata_callbacks);
    if (status == MYLITE_OK) {
        stmt->select_plan = *plan;
        *plan = (struct mylite_select_plan){0};
        status = clone_table_select_expressions(stmt, where_clause, sql, sql_length, callbacks);
    }
    if (status == MYLITE_OK) {
        stmt->preserve_prepare_warnings = database->warnings.count > 0U;
        *out_stmt = stmt;
        return MYLITE_OK;
    }

    mylite_finalize(stmt);
    return status;
}

int mylite_select_execute_table_statement(mylite_stmt *stmt,
                                          const struct mylite_select_eval_callbacks *callbacks)
{
    if (stmt->sqlite_stmt == NULL && mylite_select_plan_table_count(&stmt->select_plan) <= 1U) {
        return MYLITE_MISUSE;
    }
    stmt->executed = true;
    stmt->affected_rows = -1;

    int status = mylite_select_materialize_table_result(stmt, callbacks);

    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->select_result.next_row >= stmt->select_result.row_count) {
        mylite_select_result_current_values_deinit(&stmt->select_result);
        stmt->select_result.has_current_row = false;
        return MYLITE_DONE;
    }

    status = mylite_select_eval_set_current_row(
        stmt, &stmt->select_result.rows[stmt->select_result.next_row], callbacks);
    if (status != MYLITE_OK) {
        return status;
    }
    ++stmt->select_result.next_row;
    return MYLITE_ROW;
}

int mylite_select_clone_order_expressions(mylite_stmt *stmt, const char *sql, size_t sql_length)
{
    for (size_t index = 0U; index < stmt->select_plan.order_key_count; ++index) {
        struct mylite_sql_ast_node *clone = NULL;
        int status = MYLITE_OK;

        if (stmt->select_plan.order_keys[index].kind != MYLITE_SELECT_ORDER_KEY_EXPRESSION) {
            continue;
        }
        status = clone_table_select_expression_node(
            stmt, stmt->select_plan.order_keys[index].expression, sql, sql_length, &clone);
        if (status != MYLITE_OK) {
            return status;
        }
        stmt->select_plan.order_keys[index].expression = clone;
    }
    return MYLITE_OK;
}

static bool
select_statement_callbacks_are_valid(const struct mylite_select_statement_callbacks *callbacks)
{
    if (callbacks == NULL || callbacks->aggregate_callbacks == NULL ||
        callbacks->metadata_callbacks == NULL) {
        return false;
    }
    return true;
}

static int clone_table_select_expressions(mylite_stmt *stmt,
                                          const struct mylite_sql_ast_node *where_clause,
                                          const char *sql, size_t sql_length,
                                          const struct mylite_select_statement_callbacks *callbacks)
{
    int status = MYLITE_OK;

    stmt->select_sql_text = mylite_copy_span_text(sql, sql_length);
    if (stmt->select_sql_text == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    if (where_clause != NULL) {
        const struct mylite_sql_ast_node *predicate = mylite_ast_child_at(where_clause, 0U);
        struct mylite_sql_ast_node *clone = NULL;

        status = clone_table_select_expression_node(stmt, predicate, sql, sql_length, &clone);
        if (status != MYLITE_OK) {
            return status;
        }
        stmt->select_predicate = clone;
    }

    status = clone_table_select_join_expressions(stmt, sql, sql_length);
    if (status == MYLITE_OK) {
        status = clone_table_select_output_expressions(stmt, sql, sql_length);
    }
    if (status == MYLITE_OK) {
        status = clone_table_select_group_expressions(stmt, sql, sql_length);
    }
    if (status == MYLITE_OK) {
        status = clone_table_select_having_expression(stmt, sql, sql_length);
    }
    if (status == MYLITE_OK) {
        status = mylite_select_clone_order_expressions(stmt, sql, sql_length);
    }
    if (status == MYLITE_OK) {
        status = collect_table_select_aggregate_bindings(stmt, callbacks);
    }
    return status;
}

static int clone_table_select_join_expressions(mylite_stmt *stmt, const char *sql,
                                               size_t sql_length)
{
    for (size_t index = 0U; index < stmt->select_plan.join_predicate_count; ++index) {
        struct mylite_sql_ast_node *clone = NULL;
        int status = clone_table_select_expression_node(
            stmt, stmt->select_plan.join_predicates[index].expression, sql, sql_length, &clone);

        if (status != MYLITE_OK) {
            return status;
        }
        stmt->select_plan.join_predicates[index].expression = clone;
    }
    return MYLITE_OK;
}

static int clone_table_select_output_expressions(mylite_stmt *stmt, const char *sql,
                                                 size_t sql_length)
{
    for (size_t index = 0U; index < stmt->select_plan.output_count; ++index) {
        struct mylite_sql_ast_node *clone = NULL;
        int status = MYLITE_OK;

        if (stmt->select_plan.outputs[index].kind != MYLITE_SELECT_OUTPUT_EXPRESSION) {
            continue;
        }
        status = clone_table_select_expression_node(
            stmt, stmt->select_plan.outputs[index].expression, sql, sql_length, &clone);
        if (status != MYLITE_OK) {
            return status;
        }
        stmt->select_plan.outputs[index].expression = clone;
    }
    return MYLITE_OK;
}

static int clone_table_select_group_expressions(mylite_stmt *stmt, const char *sql,
                                                size_t sql_length)
{
    for (size_t index = 0U; index < stmt->select_plan.group_key_count; ++index) {
        struct mylite_sql_ast_node *clone = NULL;
        int status = MYLITE_OK;

        if (stmt->select_plan.group_keys[index].kind != MYLITE_SELECT_GROUP_KEY_EXPRESSION) {
            continue;
        }
        status = clone_table_select_expression_node(
            stmt, stmt->select_plan.group_keys[index].expression, sql, sql_length, &clone);
        if (status != MYLITE_OK) {
            return status;
        }
        stmt->select_plan.group_keys[index].expression = clone;
    }
    return MYLITE_OK;
}

static int clone_table_select_having_expression(mylite_stmt *stmt, const char *sql,
                                                size_t sql_length)
{
    if (stmt->select_plan.having_expression != NULL) {
        struct mylite_sql_ast_node *clone = NULL;
        int status = clone_table_select_expression_node(stmt, stmt->select_plan.having_expression,
                                                        sql, sql_length, &clone);

        if (status != MYLITE_OK) {
            return status;
        }
        stmt->select_plan.having_expression = clone;
    }
    return MYLITE_OK;
}

static int
collect_table_select_aggregate_bindings(mylite_stmt *stmt,
                                        const struct mylite_select_statement_callbacks *callbacks)
{
    int status = MYLITE_OK;

    mylite_select_plan_clear_aggregate_bindings(&stmt->select_plan);
    status = collect_table_select_expression_aggregate_bindings(stmt, callbacks);
    if (status == MYLITE_OK && stmt->select_plan.having_expression != NULL) {
        status = mylite_select_collect_aggregate_bindings(
            stmt->database, stmt->select_plan.having_expression, &stmt->select_plan,
            callbacks->aggregate_callbacks);
    }
    if (status == MYLITE_OK) {
        status = collect_table_select_order_aggregate_bindings(stmt, callbacks);
    }
    return status;
}

static int collect_table_select_expression_aggregate_bindings(
    mylite_stmt *stmt, const struct mylite_select_statement_callbacks *callbacks)
{
    for (size_t index = 0U; index < stmt->select_plan.output_count; ++index) {
        int status = MYLITE_OK;

        if (stmt->select_plan.outputs[index].kind != MYLITE_SELECT_OUTPUT_EXPRESSION) {
            continue;
        }
        status = mylite_select_collect_aggregate_bindings(
            stmt->database, stmt->select_plan.outputs[index].expression, &stmt->select_plan,
            callbacks->aggregate_callbacks);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int collect_table_select_order_aggregate_bindings(
    mylite_stmt *stmt, const struct mylite_select_statement_callbacks *callbacks)
{
    for (size_t index = 0U; index < stmt->select_plan.order_key_count; ++index) {
        int status = MYLITE_OK;

        if (stmt->select_plan.order_keys[index].kind != MYLITE_SELECT_ORDER_KEY_EXPRESSION) {
            continue;
        }
        status = mylite_select_collect_aggregate_bindings(
            stmt->database, stmt->select_plan.order_keys[index].expression, &stmt->select_plan,
            callbacks->aggregate_callbacks);
        if (status != MYLITE_OK) {
            return status;
        }
    }

    return MYLITE_OK;
}

static int clone_table_select_expression_node(mylite_stmt *stmt,
                                              const struct mylite_sql_ast_node *expression,
                                              const char *source_sql, size_t sql_length,
                                              struct mylite_sql_ast_node **out_node)
{
    int status =
        mylite_statement_clone_sql_ast_subtree(&stmt->select_predicate_ast, expression, source_sql,
                                               stmt->select_sql_text, sql_length, out_node);

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
    }
    return status;
}
