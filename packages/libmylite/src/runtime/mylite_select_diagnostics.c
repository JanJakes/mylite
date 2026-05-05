#include "mylite_select_diagnostics.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"

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

int mylite_select_set_unsupported_projection_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(database, "Unsupported SELECT projection") ==
        MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_UNSUPPORTED;
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
