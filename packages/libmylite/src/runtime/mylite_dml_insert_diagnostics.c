#include "mylite_dml_insert_diagnostics.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "sqlite3.h"

#include <stdio.h>
#include <string.h>

static char *copy_insert_duplicate_entry_value(
    const struct mylite_insert_unique_index *index,
    const struct mylite_insert_bound_value *values
);

int mylite_dml_insert_set_wrong_value_count_error(mylite_db *database, size_t row_index) {
    enum { row_number_buffer_size = 64 };

    char buffer[row_number_buffer_size];

    (void)snprintf(buffer, sizeof(buffer), "%zu", row_index + 1U);
    if (mylite_diagnostics_set_error_message_parts(
            database,
            "Column count doesn't match value count at row ",
            buffer,
            ""
        ) == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}

int mylite_dml_insert_set_no_default_error(mylite_db *database, const char *column_name) {
    int status = mylite_diagnostics_set_error_message_parts(
        database,
        "Field '",
        column_name,
        "' doesn't have a default value"
    );

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_dml_insert_set_default_function_generated_error(mylite_db *database) {
    static const char message[] = "DEFAULT function cannot be used with default value expressions";
    int status = mylite_diagnostics_set_error_message(database, message);

    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_DEFAULT_VAL_GENERATED,
            message
        );
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_dml_insert_set_unsupported_generated_default_error(
    mylite_db *database,
    const char *column_name
) {
    int status = mylite_diagnostics_set_error_message_parts(
        database,
        "Unsupported generated default expression for '",
        column_name,
        "'"
    );

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_dml_insert_set_unsupported_expression_error(mylite_db *database) {
    if (mylite_diagnostics_set_error_message(database, "Unsupported INSERT value expression") ==
        MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}

int mylite_dml_insert_append_no_default_warning(mylite_db *database, const char *column_name) {
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
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_insert_execution_state *state,
    size_t column_index
) {
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

int mylite_dml_insert_set_duplicate_entry_error(
    mylite_db *database,
    const char *table_name,
    const struct mylite_insert_unique_index *index,
    const struct mylite_insert_bound_value *values
) {
    char *entry = NULL;
    char *message = NULL;
    int status = MYLITE_OK;

    if (database == NULL || table_name == NULL || index == NULL || values == NULL) {
        return MYLITE_MISUSE;
    }

    entry = copy_insert_duplicate_entry_value(index, values);
    if (entry == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    message =
        sqlite3_mprintf("Duplicate entry '%q' for key '%q.%q'", entry, table_name, index->name);
    sqlite3_free(entry);
    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_set_error_message(database, message);
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_dml_insert_append_duplicate_entry_warning(
    mylite_db *database,
    const char *table_name,
    const struct mylite_insert_unique_index *index,
    const struct mylite_insert_bound_value *values
) {
    char *entry = NULL;
    char *message = NULL;
    int status = MYLITE_OK;

    if (database == NULL || table_name == NULL || index == NULL || values == NULL) {
        return MYLITE_MISUSE;
    }

    entry = copy_insert_duplicate_entry_value(index, values);
    if (entry == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    message =
        sqlite3_mprintf("Duplicate entry '%q' for key '%q.%q'", entry, table_name, index->name);
    sqlite3_free(entry);
    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_append_warning(database, MYLITE_MYSQL_ER_DUP_ENTRY, message);
    sqlite3_free(message);
    return status;
}

static char *copy_insert_duplicate_entry_value(
    const struct mylite_insert_unique_index *index,
    const struct mylite_insert_bound_value *values
) {
    sqlite3_str *text = sqlite3_str_new(NULL);

    if (text == NULL) {
        return NULL;
    }

    for (size_t part = 0U; part < index->column_count; ++part) {
        const struct mylite_insert_bound_value *value = &values[index->column_indexes[part]];

        if (part != 0U) {
            sqlite3_str_append(text, "-", 1);
        }
        switch (value->kind) {
        case MYLITE_INSERT_BOUND_NULL:
            sqlite3_str_append(text, "NULL", (int)strlen("NULL"));
            break;
        case MYLITE_INSERT_BOUND_INTEGER:
            sqlite3_str_appendf(text, "%lld", (long long)value->integer_value);
            break;
        case MYLITE_INSERT_BOUND_REAL:
            sqlite3_str_appendf(text, "%.15g", value->real_value);
            break;
        case MYLITE_INSERT_BOUND_TEXT:
            sqlite3_str_append(
                text,
                value->text_value == NULL ? "" : value->text_value,
                value->text_value == NULL ? 0 : (int)strlen(value->text_value)
            );
            break;
        }
    }
    return sqlite3_str_finish(text);
}

int mylite_dml_insert_append_null_warning(mylite_db *database, const char *column_name) {
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

int mylite_dml_insert_append_null_warning_once(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_insert_execution_state *state,
    size_t column_index
) {
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
