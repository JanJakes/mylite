#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_dml_insert_copy.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "mylite_statement_ast.h"
#include "mylite_statement_types.h"

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
static int prepare_insert_like_statement(mylite_db *database, enum mylite_stmt_kind kind,
                                         const struct mylite_sql_ast_node *statement,
                                         const char *sql, size_t sql_length,
                                         mylite_stmt **out_stmt);
static int copy_insert_like_statement(enum mylite_stmt_kind kind,
                                      const struct mylite_sql_ast_node *statement,
                                      mylite_stmt *stmt);
static int clone_insert_plan_nodes(mylite_stmt *stmt, const struct mylite_sql_ast_node *statement,
                                   const char *sql, size_t sql_length);
static int clone_insert_values_nodes(mylite_stmt *stmt, const struct mylite_sql_ast_node *rows_node,
                                     const char *sql, size_t sql_length);
static int clone_insert_set_nodes(mylite_stmt *stmt,
                                  const struct mylite_sql_ast_node *assignments_node,
                                  const char *sql, size_t sql_length);
static int clone_insert_value_node(mylite_stmt *stmt, const struct mylite_sql_ast_node *value_node,
                                   const char *source_sql, size_t sql_length,
                                   struct mylite_insert_value *value);
static bool insert_value_needs_expression_eval(const struct mylite_sql_ast_node *node);
static bool insert_function_is_special_values(const struct mylite_sql_ast_node *node);
static const struct mylite_sql_ast_node *
insert_unwrap_parenthesized_expression(const struct mylite_sql_ast_node *node);
static int clone_insert_ast_node(mylite_stmt *stmt, const struct mylite_sql_ast_node *node,
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

int mylite_dml_prepare_insert_values_statement(mylite_db *database,
                                               const struct mylite_sql_ast_node *statement,
                                               const char *sql, size_t sql_length,
                                               mylite_stmt **out_stmt)
{
    return prepare_insert_like_statement(database, MYLITE_STMT_INSERT_VALUES, statement, sql,
                                         sql_length, out_stmt);
}

int mylite_dml_prepare_insert_set_statement(mylite_db *database,
                                            const struct mylite_sql_ast_node *statement,
                                            const char *sql, size_t sql_length,
                                            mylite_stmt **out_stmt)
{
    return prepare_insert_like_statement(database, MYLITE_STMT_INSERT_SET, statement, sql,
                                         sql_length, out_stmt);
}

int mylite_dml_prepare_replace_values_statement(mylite_db *database,
                                                const struct mylite_sql_ast_node *statement,
                                                const char *sql, size_t sql_length,
                                                mylite_stmt **out_stmt)
{
    return prepare_insert_like_statement(database, MYLITE_STMT_REPLACE_VALUES, statement, sql,
                                         sql_length, out_stmt);
}

int mylite_dml_prepare_replace_set_statement(mylite_db *database,
                                             const struct mylite_sql_ast_node *statement,
                                             const char *sql, size_t sql_length,
                                             mylite_stmt **out_stmt)
{
    return prepare_insert_like_statement(database, MYLITE_STMT_REPLACE_SET, statement, sql,
                                         sql_length, out_stmt);
}

static int prepare_insert_like_statement(mylite_db *database, enum mylite_stmt_kind kind,
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
        .kind = kind,
        .affected_rows = 0,
    };

    status = copy_insert_like_statement(kind, statement, stmt);
    if (status == MYLITE_OK) {
        status = clone_insert_plan_nodes(stmt, statement, sql, sql_length);
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

static int copy_insert_like_statement(enum mylite_stmt_kind kind,
                                      const struct mylite_sql_ast_node *statement,
                                      mylite_stmt *stmt)
{
    switch (kind) {
    case MYLITE_STMT_INSERT_VALUES:
        return mylite_dml_copy_insert_values_statement(statement, &stmt->insert_values,
                                                       &stmt->insert_update);
    case MYLITE_STMT_INSERT_SET:
        return mylite_dml_copy_insert_set_statement(statement, &stmt->insert_values,
                                                    &stmt->insert_set, &stmt->insert_update);
    case MYLITE_STMT_REPLACE_VALUES:
        return mylite_dml_copy_replace_values_statement(statement, &stmt->insert_values);
    case MYLITE_STMT_REPLACE_SET:
        return mylite_dml_copy_replace_set_statement(statement, &stmt->insert_values,
                                                     &stmt->insert_set);
    case MYLITE_STMT_CREATE_SCHEMA:
    case MYLITE_STMT_ALTER_SCHEMA:
    case MYLITE_STMT_DROP_SCHEMA:
    case MYLITE_STMT_USE_SCHEMA:
    case MYLITE_STMT_SET_NAMES:
    case MYLITE_STMT_SET_CHARACTER_SET:
    case MYLITE_STMT_SET_SYSTEM_VARIABLE:
    case MYLITE_STMT_CREATE_TABLE:
    case MYLITE_STMT_DROP_TABLE:
    case MYLITE_STMT_RENAME_TABLE:
    case MYLITE_STMT_TRUNCATE_TABLE:
    case MYLITE_STMT_ALTER_TABLE:
    case MYLITE_STMT_CREATE_INDEX:
    case MYLITE_STMT_DROP_INDEX:
    case MYLITE_STMT_UPDATE:
    case MYLITE_STMT_DELETE:
    case MYLITE_STMT_START_TRANSACTION:
    case MYLITE_STMT_BEGIN_TRANSACTION:
    case MYLITE_STMT_COMMIT:
    case MYLITE_STMT_ROLLBACK:
    case MYLITE_STMT_SAVEPOINT:
    case MYLITE_STMT_ROLLBACK_TO_SAVEPOINT:
    case MYLITE_STMT_RELEASE_SAVEPOINT:
    case MYLITE_STMT_SCALAR_SELECT:
    case MYLITE_STMT_TABLE_SELECT:
    case MYLITE_STMT_UNION_QUERY:
    case MYLITE_STMT_SQLITE:
        break;
    }
    return MYLITE_UNSUPPORTED;
}

static int clone_insert_plan_nodes(mylite_stmt *stmt, const struct mylite_sql_ast_node *statement,
                                   const char *sql, size_t sql_length)
{
    const struct mylite_sql_ast_node *second_child = mylite_ast_child_at(statement, 1U);
    const struct mylite_sql_ast_node *values_node = NULL;
    const struct mylite_sql_ast_node *assignments_node = NULL;

    stmt->insert_sql_text = mylite_copy_span_text(sql, sql_length);
    if (stmt->insert_sql_text == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    if (statement->kind == MYLITE_SQL_AST_INSERT_SET_STATEMENT ||
        statement->kind == MYLITE_SQL_AST_REPLACE_SET_STATEMENT) {
        assignments_node = second_child;
        return clone_insert_set_nodes(stmt, assignments_node, sql, sql_length);
    }

    values_node = second_child;
    if (second_child != NULL && second_child->kind == MYLITE_SQL_AST_INSERT_COLUMN_LIST) {
        values_node = mylite_ast_child_at(statement, 2U);
    }
    return clone_insert_values_nodes(stmt, values_node, sql, sql_length);
}

static int clone_insert_values_nodes(mylite_stmt *stmt, const struct mylite_sql_ast_node *rows_node,
                                     const char *sql, size_t sql_length)
{
    size_t row_index = 0U;

    if (rows_node == NULL || rows_node->kind != MYLITE_SQL_AST_INSERT_ROW_LIST) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *row_node = rows_node->first_child; row_node != NULL;
         row_node = row_node->next_sibling, ++row_index) {
        size_t value_index = 0U;
        struct mylite_insert_row *row = NULL;

        if (row_index >= stmt->insert_values.row_count) {
            return MYLITE_UNSUPPORTED;
        }
        row = &stmt->insert_values.rows[row_index];
        for (const struct mylite_sql_ast_node *value_node = row_node->first_child;
             value_node != NULL; value_node = value_node->next_sibling, ++value_index) {
            int status = MYLITE_OK;

            if (value_index >= row->value_count) {
                return MYLITE_UNSUPPORTED;
            }
            status = clone_insert_value_node(stmt, value_node, sql, sql_length,
                                             &row->values[value_index]);
            if (status != MYLITE_OK) {
                return status;
            }
        }
        if (value_index != row->value_count) {
            return MYLITE_UNSUPPORTED;
        }
    }
    return row_index == stmt->insert_values.row_count ? MYLITE_OK : MYLITE_UNSUPPORTED;
}

static int clone_insert_set_nodes(mylite_stmt *stmt,
                                  const struct mylite_sql_ast_node *assignments_node,
                                  const char *sql, size_t sql_length)
{
    size_t assignment_index = 0U;

    if (assignments_node == NULL ||
        assignments_node->kind != MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *assignment_node = assignments_node->first_child;
         assignment_node != NULL;
         assignment_node = assignment_node->next_sibling, ++assignment_index) {
        int status = MYLITE_OK;

        if (assignment_index >= stmt->insert_set.assignment_count) {
            return MYLITE_UNSUPPORTED;
        }
        status =
            clone_insert_value_node(stmt, mylite_ast_child_at(assignment_node, 1U), sql, sql_length,
                                    &stmt->insert_set.assignments[assignment_index].value);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return assignment_index == stmt->insert_set.assignment_count ? MYLITE_OK : MYLITE_UNSUPPORTED;
}

static int clone_insert_value_node(mylite_stmt *stmt, const struct mylite_sql_ast_node *value_node,
                                   const char *source_sql, size_t sql_length,
                                   struct mylite_insert_value *value)
{
    const struct mylite_sql_ast_node *clone = NULL;
    int status = MYLITE_OK;

    if (!insert_value_needs_expression_eval(value_node)) {
        return MYLITE_OK;
    }

    status = clone_insert_ast_node(stmt, value_node, source_sql, sql_length, &clone);
    if (status == MYLITE_OK) {
        value->kind = MYLITE_INSERT_VALUE_EXPRESSION;
        value->expression = clone;
    }
    return status;
}

static bool insert_value_needs_expression_eval(const struct mylite_sql_ast_node *node)
{
    node = insert_unwrap_parenthesized_expression(node);
    if (node == NULL) {
        return false;
    }
    if (node->kind == MYLITE_SQL_AST_DEFAULT || node->kind == MYLITE_SQL_AST_LITERAL ||
        node->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP || node->kind == MYLITE_SQL_AST_IDENTIFIER ||
        node->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return false;
    }
    if (node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        const struct mylite_sql_ast_node *operand =
            insert_unwrap_parenthesized_expression(mylite_ast_child_at(node, 0U));

        if (operand != NULL && operand->kind == MYLITE_SQL_AST_LITERAL &&
            (operand->literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
             operand->literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL ||
             operand->literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT)) {
            return false;
        }
    }
    if (insert_function_is_special_values(node)) {
        return false;
    }
    return true;
}

static bool insert_function_is_special_values(const struct mylite_sql_ast_node *node)
{
    const struct mylite_sql_ast_node *name = NULL;

    if (node == NULL || node->kind != MYLITE_SQL_AST_FUNCTION_CALL) {
        return false;
    }

    name = mylite_ast_child_at(node, 0U);
    return name != NULL && name->kind == MYLITE_SQL_AST_IDENTIFIER &&
           mylite_span_equal_ci(name->span, "VALUES");
}

static const struct mylite_sql_ast_node *
insert_unwrap_parenthesized_expression(const struct mylite_sql_ast_node *node)
{
    while (node != NULL && node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        node = mylite_ast_child_at(node, 0U);
    }
    return node;
}

static int clone_insert_ast_node(mylite_stmt *stmt, const struct mylite_sql_ast_node *node,
                                 const char *source_sql, size_t sql_length,
                                 const struct mylite_sql_ast_node **out_node)
{
    struct mylite_sql_ast_node *clone = NULL;
    int status = mylite_statement_ast_clone_subtree(&stmt->insert_ast, node, source_sql,
                                                    stmt->insert_sql_text, sql_length, &clone);

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
    }
    *out_node = clone;
    return status;
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
    int status = mylite_statement_ast_clone_subtree(&stmt->update_ast, node, source_sql,
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

    if (statement->delete_form != MYLITE_SQL_AST_DELETE_SINGLE_TABLE) {
        status = clone_delete_ast_node(stmt, mylite_ast_child_at(statement, 1U), sql, sql_length,
                                       &stmt->delete_plan.from_clause);
    }
    if (status != MYLITE_OK) {
        return status;
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
    int status = mylite_statement_ast_clone_subtree(&stmt->delete_ast, node, source_sql,
                                                    stmt->delete_sql_text, sql_length, &clone);

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
    }
    *out_node = clone;
    return status;
}
