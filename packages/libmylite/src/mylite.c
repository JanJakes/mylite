#include <mylite/mylite.h>

#include "runtime/mylite_select_context.h"
#include "runtime/mylite_statement.h"
#include "runtime/mylite_statement_execute.h"
#include "runtime/mylite_statement_prepare.h"

int mylite_prepare(mylite_db *database, const char *sql, size_t length, mylite_stmt **out_stmt)
{
    return mylite_statement_prepare_with_callbacks(
        database, sql, length, out_stmt, mylite_select_context_statement_prepare_callbacks());
}

int mylite_statement_execute_custom(mylite_stmt *stmt)
{
    return mylite_statement_execute_custom_with_callbacks(
        stmt, mylite_select_context_statement_execute_callbacks());
}
