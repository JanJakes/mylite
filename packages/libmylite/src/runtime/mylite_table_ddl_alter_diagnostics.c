#include "mylite_table_ddl_alter.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "sqlite3.h"

int mylite_table_ddl_set_alter_table_duplicate_column_error(mylite_db *database,
                                                            const char *column_name)
{
    int status = mylite_diagnostics_set_error_message_parts(database, "Duplicate column name '",
                                                            column_name, "'");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_DUP_FIELDNAME,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_table_ddl_set_alter_table_unknown_column_error(mylite_db *database,
                                                          const char *table_name,
                                                          const char *column_name)
{
    char *message = sqlite3_mprintf("Unknown column '%q' in '%q'", column_name, table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    sqlite3_free(message);
    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_BAD_FIELD_ERROR,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_table_ddl_set_alter_table_cant_drop_column_error(mylite_db *database,
                                                            const char *column_name)
{
    char *message = sqlite3_mprintf("Can't DROP '%q'; check that column/key exists", column_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    sqlite3_free(message);
    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_CANT_DROP_FIELD_OR_KEY,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_table_ddl_set_alter_table_cant_remove_all_columns_error(mylite_db *database)
{
    int status = mylite_diagnostics_set_error_message(
        database, "You can't delete all columns with ALTER TABLE; use DROP TABLE");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_CANT_REMOVE_ALL_FIELDS,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_table_ddl_set_alter_table_wrong_auto_increment_error(mylite_db *database)
{
    int status = mylite_diagnostics_set_error_message(
        database,
        "Incorrect table definition; there can be only one auto column and it must be defined "
        "as a key");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_WRONG_AUTO_KEY,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}
