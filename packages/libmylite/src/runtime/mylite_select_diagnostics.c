#include "mylite_select_diagnostics.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_expression.h"
#include "mylite_runtime.h"

int mylite_select_set_invalid_group_function_error(mylite_db *database)
{
    int status = mylite_diagnostics_set_error_message(database, "Invalid use of group function");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_INVALID_GROUP_FUNC_USE,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_select_set_duplicate_mode_error(mylite_db *database)
{
    int status =
        mylite_diagnostics_set_error_message(database, "Incorrect usage of ALL and DISTINCT");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_WRONG_USAGE,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_select_set_unsupported_window_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(
            database, "Unsupported window functions or WINDOW clause") == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_UNSUPPORTED;
}

int mylite_select_set_unsupported_projection_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(database, "Unsupported SELECT projection") ==
        MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_UNSUPPORTED;
}

int mylite_select_set_where_predicate_eval_error(mylite_stmt *stmt)
{
    mylite_db *database = stmt == NULL ? NULL : stmt->database;

    if (database == NULL) {
        return MYLITE_EXEC_ERROR;
    }
    if (database->warnings.count != 0U) {
        const struct mylite_expression_warning *warning =
            &database->warnings.items[database->warnings.count - 1U];

        if (warning->level == MYLITE_EXPRESSION_WARNING_LEVEL_ERROR) {
            int status = mylite_diagnostics_set_error_message(database, warning->message);

            return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
        }
        if (warning->code == MYLITE_MYSQL_ER_WRONG_ARGUMENTS) {
            int status = mylite_diagnostics_set_error_message(database, warning->message);

            return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
        }
    }
    return mylite_select_set_unsupported_where_error(database);
}

int mylite_select_set_unsupported_where_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(database, "Unsupported WHERE predicate") ==
        MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_UNSUPPORTED;
}

int mylite_select_set_unsupported_order_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(database, "Unsupported ORDER BY expression") ==
        MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_UNSUPPORTED;
}

int mylite_select_set_unsupported_join_grouping_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(
            database, "Unsupported GROUP BY or HAVING over joined tables") == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}
