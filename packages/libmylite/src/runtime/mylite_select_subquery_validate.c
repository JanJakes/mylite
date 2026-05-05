#include "mylite_select_subquery.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_span.h"
#include "sqlite3.h"

static int set_operand_column_count_error(mylite_db *database, size_t expected_width);

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
        return set_operand_column_count_error(database, expected_width);
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
        return set_operand_column_count_error(database, expected_width);
    }
    return MYLITE_OK;
}

int mylite_select_subquery_set_operand_columns_error(mylite_db *database)
{
    return set_operand_column_count_error(database, 1U);
}

int mylite_select_subquery_set_in_limit_error(mylite_db *database)
{
    static const char message[] =
        "This version of MySQL doesn't yet support 'LIMIT & IN/ALL/ANY/SOME subquery'";
    int status = mylite_diagnostics_set_error_message(database, message);

    if (status == MYLITE_OK) {
        status =
            mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_NOT_SUPPORTED_YET, message);
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_select_subquery_set_row_quantified_non_alias_error(
    mylite_db *database, const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *select_statement = mylite_ast_child_at(expression, 1U);

    if (mylite_ast_find_child_kind(select_statement, MYLITE_SQL_AST_LIMIT_CLAUSE) != NULL) {
        return mylite_select_subquery_set_in_limit_error(database);
    }
    return mylite_select_subquery_set_operand_columns_error(database);
}

int mylite_select_subquery_set_scalar_cardinality_error(mylite_db *database)
{
    static const char message[] = "Subquery returns more than 1 row";
    int status = mylite_diagnostics_set_error_message(database, message);

    if (status == MYLITE_OK) {
        status =
            mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_SUBQUERY_NO_1_ROW, message);
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_operand_column_count_error(mylite_db *database, size_t expected_width)
{
    char *message = sqlite3_mprintf("Operand should contain %llu column(s)",
                                    (unsigned long long)expected_width);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status =
            mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_OPERAND_COLUMNS, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}
