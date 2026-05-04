#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "sql/mylite_expression.h"

int mylite_dml_promote_expression_warnings(mylite_db *database, size_t warning_start)
{
    const struct mylite_expression_warning *warning = NULL;
    int status = MYLITE_OK;

    if (database == NULL || warning_start >= database->warnings.count) {
        return MYLITE_OK;
    }

    warning = &database->warnings.items[warning_start];
    status = mylite_diagnostics_set_error_message(database, warning->message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_dml_set_expression_condition_error(mylite_db *database, size_t warning_start)
{
    if (database == NULL || warning_start >= database->warnings.count) {
        return MYLITE_OK;
    }

    for (size_t index = warning_start; index < database->warnings.count; ++index) {
        const struct mylite_expression_warning *condition = &database->warnings.items[index];

        if (condition->level == MYLITE_EXPRESSION_WARNING_LEVEL_NOTE) {
            continue;
        }

        int status = mylite_diagnostics_set_error_message(database, condition->message);

        return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
    }
    return MYLITE_OK;
}
