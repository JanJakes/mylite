#include "mylite_select_subquery_diagnostics.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_span.h"
#include "sqlite3.h"

int mylite_select_subquery_set_operand_column_count_error(mylite_db *database,
                                                          size_t expected_width)
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

int mylite_select_subquery_set_operand_columns_error(mylite_db *database)
{
    return mylite_select_subquery_set_operand_column_count_error(database, 1U);
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
