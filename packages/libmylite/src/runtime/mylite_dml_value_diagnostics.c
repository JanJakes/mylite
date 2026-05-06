#include "mylite_dml.h"

#include "mylite_diagnostics.h"

int mylite_dml_set_not_null_column_error(mylite_db *database, const char *column_name) {
    int status = mylite_diagnostics_set_error_message_parts(
        database,
        "Column '",
        column_name,
        "' cannot be null"
    );

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}
