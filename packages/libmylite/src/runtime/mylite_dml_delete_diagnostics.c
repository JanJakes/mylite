#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "sqlite3.h"

int mylite_dml_set_delete_unknown_column_error(
    mylite_db *database,
    const char *column_name,
    const char *clause_context
) {
    char *message = sqlite3_mprintf(
        "Unknown column '%q' in '%q'",
        column_name,
        clause_context == NULL ? "where clause" : clause_context
    );
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_dml_set_delete_unsupported_clause_error(mylite_db *database) {
    if (mylite_diagnostics_set_error_message(database, "Unsupported DELETE clause") ==
        MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}
