#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_span.h"
#include "mylite_statement.h"

#include <stdlib.h>

static int clone_update_plan_nodes(mylite_stmt *stmt, const struct mylite_sql_ast_node *statement,
                                   const char *sql, size_t sql_length);
static int clone_update_ast_node(mylite_stmt *stmt, const struct mylite_sql_ast_node *node,
                                 const char *source_sql, size_t sql_length,
                                 const struct mylite_sql_ast_node **out_node);
static int clone_delete_plan_nodes(mylite_stmt *stmt, const struct mylite_sql_ast_node *statement,
                                   const char *sql, size_t sql_length);
static int clone_delete_ast_node(mylite_stmt *stmt, const struct mylite_sql_ast_node *node,
                                 const char *source_sql, size_t sql_length,
                                 const struct mylite_sql_ast_node **out_node);

int mylite_dml_prepare_update_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        const char *sql, size_t sql_length, mylite_stmt **out_stmt)
{
    mylite_stmt *stmt = calloc(1U, sizeof(*stmt));
    int status = MYLITE_OK;

    if (stmt == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    *stmt = (mylite_stmt){
        .database = database,
        .kind = MYLITE_STMT_UPDATE,
        .affected_rows = 0,
    };

    status = mylite_dml_copy_update_statement(statement, &stmt->update);
    if (status == MYLITE_OK) {
        status = clone_update_plan_nodes(stmt, statement, sql, sql_length);
    }
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        mylite_finalize(stmt);
        return status;
    }

    *out_stmt = stmt;
    return MYLITE_OK;
}

int mylite_dml_prepare_delete_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        const char *sql, size_t sql_length, mylite_stmt **out_stmt)
{
    mylite_stmt *stmt = calloc(1U, sizeof(*stmt));
    int status = MYLITE_OK;

    if (stmt == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    *stmt = (mylite_stmt){
        .database = database,
        .kind = MYLITE_STMT_DELETE,
        .affected_rows = 0,
    };

    status = mylite_dml_copy_delete_statement(statement, &stmt->delete_plan);
    if (status == MYLITE_OK) {
        status = clone_delete_plan_nodes(stmt, statement, sql, sql_length);
    }
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        mylite_finalize(stmt);
        return status;
    }

    *out_stmt = stmt;
    return MYLITE_OK;
}

static int clone_update_plan_nodes(mylite_stmt *stmt, const struct mylite_sql_ast_node *statement,
                                   const char *sql, size_t sql_length)
{
    const struct mylite_sql_ast_node *assignments = mylite_ast_child_at(statement, 1U);
    size_t assignment_index = 0U;
    int status = MYLITE_OK;

    stmt->update_sql_text = mylite_copy_span_text(sql, sql_length);
    if (stmt->update_sql_text == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    for (const struct mylite_sql_ast_node *assignment =
             assignments == NULL ? NULL : assignments->first_child;
         assignment != NULL; assignment = assignment->next_sibling, ++assignment_index) {
        const struct mylite_sql_ast_node *clone = NULL;

        if (assignment_index >= stmt->update.assignment_count) {
            return MYLITE_UNSUPPORTED;
        }
        status = clone_update_ast_node(stmt, mylite_ast_child_at(assignment, 1U), sql, sql_length,
                                       &clone);
        if (status != MYLITE_OK) {
            return status;
        }
        stmt->update.assignments[assignment_index].value = clone;
    }
    if (assignment_index != stmt->update.assignment_count) {
        return MYLITE_UNSUPPORTED;
    }

    status = clone_update_ast_node(
        stmt, mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_WHERE_CLAUSE), sql, sql_length,
        &stmt->update.where_clause);
    if (status != MYLITE_OK) {
        return status;
    }
    status = clone_update_ast_node(
        stmt, mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE), sql,
        sql_length, &stmt->update.order_by_clause);
    if (status != MYLITE_OK) {
        return status;
    }
    return clone_update_ast_node(
        stmt, mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE), sql,
        sql_length, &stmt->update.limit_clause);
}

static int clone_update_ast_node(mylite_stmt *stmt, const struct mylite_sql_ast_node *node,
                                 const char *source_sql, size_t sql_length,
                                 const struct mylite_sql_ast_node **out_node)
{
    struct mylite_sql_ast_node *clone = NULL;
    int status = mylite_statement_clone_sql_ast_subtree(&stmt->update_ast, node, source_sql,
                                                        stmt->update_sql_text, sql_length, &clone);

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
    }
    *out_node = clone;
    return status;
}

static int clone_delete_plan_nodes(mylite_stmt *stmt, const struct mylite_sql_ast_node *statement,
                                   const char *sql, size_t sql_length)
{
    int status = MYLITE_OK;

    stmt->delete_sql_text = mylite_copy_span_text(sql, sql_length);
    if (stmt->delete_sql_text == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = clone_delete_ast_node(
        stmt, mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_WHERE_CLAUSE), sql, sql_length,
        &stmt->delete_plan.where_clause);
    if (status != MYLITE_OK) {
        return status;
    }
    status = clone_delete_ast_node(
        stmt, mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE), sql,
        sql_length, &stmt->delete_plan.order_by_clause);
    if (status != MYLITE_OK) {
        return status;
    }
    return clone_delete_ast_node(
        stmt, mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE), sql,
        sql_length, &stmt->delete_plan.limit_clause);
}

static int clone_delete_ast_node(mylite_stmt *stmt, const struct mylite_sql_ast_node *node,
                                 const char *source_sql, size_t sql_length,
                                 const struct mylite_sql_ast_node **out_node)
{
    struct mylite_sql_ast_node *clone = NULL;
    int status = mylite_statement_clone_sql_ast_subtree(&stmt->delete_ast, node, source_sql,
                                                        stmt->delete_sql_text, sql_length, &clone);

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
    }
    *out_node = clone;
    return status;
}
