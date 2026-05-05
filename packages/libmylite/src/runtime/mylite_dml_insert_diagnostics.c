#include "mylite_dml_insert_diagnostics.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "sqlite3.h"

#include <stdio.h>

int mylite_dml_insert_set_wrong_value_count_error(mylite_db *database, size_t row_index)
{
    enum { row_number_buffer_size = 64 };
    char buffer[row_number_buffer_size];

    (void)snprintf(buffer, sizeof(buffer), "%zu", row_index + 1U);
    if (mylite_diagnostics_set_error_message_parts(database,
                                                   "Column count doesn't match value count at row ",
                                                   buffer, "") == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}

int mylite_dml_insert_set_no_default_error(mylite_db *database, const char *column_name)
{
    int status = mylite_diagnostics_set_error_message_parts(database, "Field '", column_name,
                                                            "' doesn't have a default value");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_dml_insert_set_unsupported_generated_default_error(mylite_db *database,
                                                              const char *column_name)
{
    int status = mylite_diagnostics_set_error_message_parts(
        database, "Unsupported generated default expression for '", column_name, "'");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_dml_insert_set_unsupported_expression_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(database, "Unsupported INSERT value expression") ==
        MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}

int mylite_dml_insert_append_no_default_warning(mylite_db *database, const char *column_name)
{
    char *message = sqlite3_mprintf("Field '%q' doesn't have a default value", column_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status =
        mylite_diagnostics_append_warning(database, MYLITE_MYSQL_ER_NO_DEFAULT_FOR_FIELD, message);
    sqlite3_free(message);
    return status;
}

int mylite_dml_insert_append_no_default_warning_once(
    mylite_db *database, const struct mylite_insert_table_column *column,
    struct mylite_insert_execution_state *state, size_t column_index)
{
    if (state != NULL && state->warned_omitted_no_default_columns != NULL &&
        state->warned_omitted_no_default_columns[column_index]) {
        return MYLITE_OK;
    }

    int status = mylite_dml_insert_append_no_default_warning(database, column->name);

    if (status != MYLITE_OK) {
        return status;
    }
    if (state != NULL && state->warned_omitted_no_default_columns != NULL) {
        state->warned_omitted_no_default_columns[column_index] = true;
    }
    return MYLITE_OK;
}

int mylite_dml_insert_append_null_warning(mylite_db *database, const char *column_name)
{
    char *message = sqlite3_mprintf("Column '%q' cannot be null", column_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_append_warning(database, MYLITE_MYSQL_ER_BAD_NULL_ERROR, message);
    sqlite3_free(message);
    return status;
}

int mylite_dml_insert_append_null_warning_once(mylite_db *database,
                                               const struct mylite_insert_table_column *column,
                                               struct mylite_insert_execution_state *state,
                                               size_t column_index)
{
    if (state != NULL && state->warned_null_columns != NULL &&
        state->warned_null_columns[column_index]) {
        return MYLITE_OK;
    }

    int status = mylite_dml_insert_append_null_warning(database, column->name);

    if (status != MYLITE_OK) {
        return status;
    }
    if (state != NULL && state->warned_null_columns != NULL) {
        state->warned_null_columns[column_index] = true;
    }
    return MYLITE_OK;
}
