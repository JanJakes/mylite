#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "sqlite3.h"

#include <string.h>

int mylite_dml_set_update_unknown_column_error(mylite_db *database, const char *column_name,
                                               const char *clause_context)
{
    char *message = sqlite3_mprintf("Unknown column '%q' in '%q'", column_name,
                                    clause_context == NULL ? "field list" : clause_context);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_dml_set_update_unsupported_expression_error(mylite_db *database,
                                                       const char *clause_context)
{
    if (clause_context != NULL && strcmp(clause_context, "field list") == 0) {
        return mylite_dml_set_update_unsupported_assignment_error(database);
    }
    return mylite_dml_set_update_unsupported_clause_error(database);
}

int mylite_dml_set_update_unsupported_clause_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(database, "Unsupported UPDATE clause") ==
        MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}

int mylite_dml_set_update_unsupported_assignment_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(database, "Unsupported UPDATE assignment") ==
        MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}
