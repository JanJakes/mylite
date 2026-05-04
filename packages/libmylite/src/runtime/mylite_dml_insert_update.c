#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"

static int append_insert_values_deprecated_warning(mylite_db *database);

int mylite_dml_append_insert_update_deprecated_warnings(
    mylite_db *database, const struct mylite_insert_duplicate_update_plan *plan)
{
    size_t warning_count = 0U;

    if (database == NULL || plan == NULL) {
        return MYLITE_MISUSE;
    }
    if (!plan->has_clause) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; index < plan->assignment_count; ++index) {
        warning_count += plan->assignments[index].value.values_function_count;
    }
    for (size_t index = 0U; index < warning_count; ++index) {
        int status = append_insert_values_deprecated_warning(database);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int append_insert_values_deprecated_warning(mylite_db *database)
{
    static const char message[] =
        "'VALUES function' is deprecated and will be removed in a future release. Please use an "
        "alias (INSERT INTO ... VALUES (...) AS alias) and replace VALUES(col) in the ON "
        "DUPLICATE KEY UPDATE clause with alias.col instead";

    return mylite_diagnostics_append_warning(database, MYLITE_MYSQL_ER_WARN_DEPRECATED_SYNTAX,
                                             message);
}
