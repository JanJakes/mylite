#include "mylite_table_ddl_statement.h"

#include "mylite_runtime.h"
#include "mylite_table_ddl.h"

int mylite_table_ddl_execute_rename_table_prepared_statement(mylite_stmt *stmt)
{
    int status = MYLITE_OK;

    stmt->affected_rows = 0;
    status = mylite_table_ddl_execute_rename_table_statement(
        stmt->database, stmt->database->selected_schema, &stmt->rename_table);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
    }
    return status;
}

int mylite_table_ddl_execute_truncate_table_prepared_statement(mylite_stmt *stmt)
{
    int status = MYLITE_OK;

    stmt->affected_rows = 0;
    status = mylite_table_ddl_execute_truncate_table_statement(
        stmt->database, stmt->database->selected_schema, &stmt->truncate_table);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
    }
    return status;
}

int mylite_table_ddl_execute_create_index_prepared_statement(mylite_stmt *stmt)
{
    int status = MYLITE_OK;

    stmt->affected_rows = 0;
    status = mylite_table_ddl_execute_create_index_statement(
        stmt->database, stmt->database->selected_schema, &stmt->index_ddl);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
    }
    return status;
}

int mylite_table_ddl_execute_drop_index_prepared_statement(mylite_stmt *stmt)
{
    int status = MYLITE_OK;

    stmt->affected_rows = 0;
    status = mylite_table_ddl_execute_drop_index_statement(
        stmt->database, stmt->database->selected_schema, &stmt->index_ddl);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
    }
    return status;
}
