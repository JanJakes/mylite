#include "mylite_select_prepare.h"

#include "mylite_diagnostics.h"
#include "mylite_information_schema.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_diagnostics.h"
#include "mylite_select_from.h"
#include "mylite_select_group_bind.h"
#include "mylite_select_group_validate.h"
#include "mylite_select_metadata.h"
#include "mylite_select_order_bind.h"
#include "mylite_select_predicate_bind.h"
#include "mylite_select_projection.h"
#include "mylite_select_scalar.h"
#include "mylite_select_sql.h"
#include "mylite_select_statement.h"
#include "mylite_span.h"
#include "mylite_statement.h"
#include "mylite_statement_custom.h"
#include "sqlite3.h"

#include <stdbool.h>

static int validate_select_duplicate_mode(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement);
static int prepare_table_select_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          const char *sql, size_t sql_length,
                                          mylite_stmt **out_stmt,
                                          const struct mylite_select_prepare_callbacks *callbacks);
static int
prepare_table_select_sqlite_statement(mylite_db *database, const struct mylite_select_plan *plan,
                                      mylite_stmt **out_stmt,
                                      const struct mylite_select_prepare_callbacks *callbacks);
static int prepare_scalar_select_statement(mylite_db *database,
                                           const struct mylite_sql_ast_node *statement,
                                           mylite_stmt **out_stmt,
                                           const struct mylite_select_prepare_callbacks *callbacks);
static int bind_table_select_clauses(mylite_db *database,
                                     const struct mylite_select_clause_nodes *clauses,
                                     struct mylite_select_plan *plan,
                                     const struct mylite_select_prepare_callbacks *callbacks);
static int bind_select_where_clause(mylite_db *database,
                                    const struct mylite_sql_ast_node *where_clause,
                                    const struct mylite_select_plan *plan,
                                    const struct mylite_select_prepare_callbacks *callbacks);
static int bind_select_join_predicates(mylite_db *database, const struct mylite_select_plan *plan,
                                       const struct mylite_select_prepare_callbacks *callbacks);
static int bind_select_group_by_clause(mylite_db *database,
                                       const struct mylite_sql_ast_node *group_by_clause,
                                       struct mylite_select_plan *plan,
                                       const struct mylite_select_prepare_callbacks *callbacks);
static int bind_select_having_clause(mylite_db *database,
                                     const struct mylite_sql_ast_node *having_clause,
                                     struct mylite_select_plan *plan,
                                     const struct mylite_select_prepare_callbacks *callbacks);
static int bind_select_order_by_clause(mylite_db *database,
                                       const struct mylite_sql_ast_node *order_by_clause,
                                       struct mylite_select_plan *plan,
                                       const struct mylite_select_prepare_callbacks *callbacks);

int mylite_select_prepare_statement(mylite_db *database,
                                    const struct mylite_sql_ast_node *statement, const char *sql,
                                    size_t sql_length, mylite_stmt **out_stmt,
                                    const struct mylite_select_prepare_callbacks *callbacks)
{
    int status = validate_select_duplicate_mode(database, statement);

    if (status != MYLITE_OK) {
        return status;
    }
    status = mylite_information_schema_prepare_select_statement(database, statement, out_stmt);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status =
        prepare_table_select_statement(database, statement, sql, sql_length, out_stmt, callbacks);
    if (status != MYLITE_UNSUPPORTED || database->error_message != NULL) {
        return status;
    }
    status = prepare_scalar_select_statement(database, statement, out_stmt, callbacks);
    if (status != MYLITE_UNSUPPORTED || database->error_message != NULL) {
        return status;
    }
    return MYLITE_UNSUPPORTED;
}

int mylite_select_prepare_subquery(mylite_db *database, const struct mylite_sql_ast_node *statement,
                                   mylite_stmt **out_stmt,
                                   const struct mylite_select_prepare_callbacks *callbacks)
{
    const char *sql = statement == NULL ? NULL : statement->span.text;
    size_t sql_length = statement == NULL ? 0U : statement->span.length;

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return MYLITE_UNSUPPORTED;
    }
    return mylite_select_prepare_statement(database, statement, sql, sql_length, out_stmt,
                                           callbacks);
}

static int validate_select_duplicate_mode(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement)
{
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return MYLITE_OK;
    }
    if (statement->select_duplicate_mode_conflict) {
        return mylite_select_set_duplicate_mode_error(database);
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int prepare_table_select_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          const char *sql, size_t sql_length,
                                          mylite_stmt **out_stmt,
                                          const struct mylite_select_prepare_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(statement, 1U);
    const struct mylite_sql_ast_node *where_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_WHERE_CLAUSE);
    const struct mylite_sql_ast_node *group_by_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_GROUP_BY_CLAUSE);
    const struct mylite_sql_ast_node *having_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_HAVING_CLAUSE);
    const struct mylite_sql_ast_node *order_by_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    const struct mylite_sql_ast_node *limit_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    const struct mylite_select_clause_nodes clauses = {
        .where = where_clause,
        .group_by = group_by_clause,
        .having = having_clause,
        .order_by = order_by_clause,
        .limit = limit_clause,
    };
    bool custom_runtime = false;
    struct mylite_select_plan plan = {0};
    int status = MYLITE_OK;

    if (from_clause == NULL || (from_clause->kind != MYLITE_SQL_AST_FROM_TABLE &&
                                from_clause->kind != MYLITE_SQL_AST_FROM_TABLE_REFERENCES)) {
        return MYLITE_UNSUPPORTED;
    }

    plan.duplicate_mode = statement->select_duplicate_mode;
    status = mylite_select_bind_from_clause(database, from_clause, &plan);
    if (status == MYLITE_OK) {
        status = bind_select_join_predicates(database, &plan, callbacks);
    }
    if (status == MYLITE_OK) {
        status = mylite_select_build_outputs(database, select_list, true, &plan,
                                             callbacks->projection_callbacks);
    }
    if (status == MYLITE_OK && mylite_select_plan_table_count(&plan) > 1U &&
        (group_by_clause != NULL || having_clause != NULL)) {
        status = mylite_select_set_unsupported_join_grouping_error(database);
    }
    if (status == MYLITE_OK) {
        custom_runtime = mylite_select_plan_requires_custom_runtime(&plan, &clauses);
    }
    if (status == MYLITE_OK) {
        status = bind_table_select_clauses(database, &clauses, &plan, callbacks);
    }
    if (status == MYLITE_OK && custom_runtime) {
        status = mylite_select_prepare_custom_table_statement(database, where_clause, sql,
                                                              sql_length, &plan, out_stmt,
                                                              callbacks->statement_callbacks);
    }
    if (status == MYLITE_OK && !custom_runtime) {
        status = prepare_table_select_sqlite_statement(database, &plan, out_stmt, callbacks);
    }

    mylite_select_plan_deinit(&plan);
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int
prepare_table_select_sqlite_statement(mylite_db *database, const struct mylite_select_plan *plan,
                                      mylite_stmt **out_stmt,
                                      const struct mylite_select_prepare_callbacks *callbacks)
{
    char *sqlite_sql = mylite_select_build_physical_sql(database, plan);
    int status = MYLITE_OK;

    if (sqlite_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    if (status == MYLITE_OK) {
        status =
            mylite_select_attach_result_metadata(*out_stmt, plan, callbacks->metadata_callbacks);
        if (status != MYLITE_OK) {
            mylite_finalize(*out_stmt);
            *out_stmt = NULL;
        }
    }
    sqlite3_free(sqlite_sql);
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int prepare_scalar_select_statement(mylite_db *database,
                                           const struct mylite_sql_ast_node *statement,
                                           mylite_stmt **out_stmt,
                                           const struct mylite_select_prepare_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(statement, 1U);

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_FROM_TABLE) != NULL ||
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_FROM_TABLE_REFERENCES) != NULL ||
        (from_clause != NULL && from_clause->kind != MYLITE_SQL_AST_FROM_DUAL &&
         from_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE &&
         from_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE)) {
        return MYLITE_UNSUPPORTED;
    }
    for (const struct mylite_sql_ast_node *item = select_list->first_child; item != NULL;
         item = item->next_sibling) {
        if (item->kind != MYLITE_SQL_AST_SELECT_ITEM || mylite_ast_child_at(item, 0U) == NULL) {
            return MYLITE_UNSUPPORTED;
        }
    }
    return mylite_statement_prepare_custom(database, MYLITE_STMT_SCALAR_SELECT, statement,
                                           callbacks->scalar_callbacks, out_stmt);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_table_select_clauses(mylite_db *database,
                                     const struct mylite_select_clause_nodes *clauses,
                                     struct mylite_select_plan *plan,
                                     const struct mylite_select_prepare_callbacks *callbacks)
{
    int status = MYLITE_OK;

    if (clauses->where != NULL) {
        status = bind_select_where_clause(database, clauses->where, plan, callbacks);
    }
    if (status == MYLITE_OK && clauses->group_by != NULL) {
        status = bind_select_group_by_clause(database, clauses->group_by, plan, callbacks);
    }
    if (status == MYLITE_OK && clauses->having != NULL) {
        status = bind_select_having_clause(database, clauses->having, plan, callbacks);
    }
    if (status == MYLITE_OK && clauses->limit != NULL) {
        status = mylite_select_bind_limit_clause(clauses->limit, plan);
    }
    if (status == MYLITE_OK && clauses->order_by != NULL) {
        status = bind_select_order_by_clause(database, clauses->order_by, plan, callbacks);
    }
    if (status == MYLITE_OK) {
        status = mylite_select_validate_grouping(database, plan);
    }
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_where_clause(mylite_db *database,
                                    const struct mylite_sql_ast_node *where_clause,
                                    const struct mylite_select_plan *plan,
                                    const struct mylite_select_prepare_callbacks *callbacks)
{
    return mylite_select_bind_where_clause(database, where_clause, plan,
                                           callbacks->predicate_callbacks);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_join_predicates(mylite_db *database, const struct mylite_select_plan *plan,
                                       const struct mylite_select_prepare_callbacks *callbacks)
{
    return mylite_select_bind_join_predicates(database, plan, callbacks->predicate_callbacks);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_group_by_clause(mylite_db *database,
                                       const struct mylite_sql_ast_node *group_by_clause,
                                       struct mylite_select_plan *plan,
                                       const struct mylite_select_prepare_callbacks *callbacks)
{
    return mylite_select_bind_group_by_clause(database, group_by_clause, plan,
                                              callbacks->group_callbacks);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_having_clause(mylite_db *database,
                                     const struct mylite_sql_ast_node *having_clause,
                                     struct mylite_select_plan *plan,
                                     const struct mylite_select_prepare_callbacks *callbacks)
{
    return mylite_select_bind_having_clause(database, having_clause, plan,
                                            callbacks->group_callbacks);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_order_by_clause(mylite_db *database,
                                       const struct mylite_sql_ast_node *order_by_clause,
                                       struct mylite_select_plan *plan,
                                       const struct mylite_select_prepare_callbacks *callbacks)
{
    return mylite_select_bind_order_by_clause(database, order_by_clause, plan,
                                              callbacks->order_callbacks);
}
