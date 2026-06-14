#include "mylite_execution_diagnostics_internal.h"

void mylite_execution_diagnostics_set_bad_null_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Column '%s' cannot be null", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_bad_null,
        "23000",
        message
    );
}

void mylite_execution_diagnostics_set_load_data_file_error(
    struct mylite_db *database,
    const char *file_path,
    int os_error
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *error_text = os_error == 0 ? "Unknown error" : strerror(os_error);
    int written = snprintf(
        message,
        sizeof(message),
        "Can't get stat of '%s' (OS errno %d - %s)",
        file_path == NULL ? "" : file_path,
        os_error,
        error_text
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_cant_get_stat,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_load_data_local_disabled_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_load_data_local_disabled,
        "42000",
        "Loading local data is disabled; this must be enabled on both the client and server sides"
    );
}

void mylite_execution_diagnostics_set_load_data_row_missing_error(
    struct mylite_db *database,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Row %zu doesn't contain data for all columns",
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_load_data_row_missing,
        "01000",
        message
    );
}

int mylite_execution_diagnostics_append_load_data_row_missing_warnings(
    struct mylite_db *database,
    struct load_data_missing_warning_request request
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Row %zu doesn't contain data for all columns",
        request.row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    for (size_t warning_index = 0U; rc == MYLITE_OK && warning_index < request.warning_count;
         ++warning_index) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_error_load_data_row_missing,
            "01000",
            message
        );
        if (rc == MYLITE_NOMEM) {
            mylite_execution_diagnostics_set_nomem_error(database);
        }
    }
    return rc;
}

void mylite_execution_diagnostics_set_load_data_row_truncated_error(
    struct mylite_db *database,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Row %zu was truncated; it contained more data than there were input columns",
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_load_data_row_truncated,
        "01000",
        message
    );
}

int mylite_execution_diagnostics_append_load_data_row_truncated_warning(
    struct mylite_db *database,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Row %zu was truncated; it contained more data than there were input columns",
        row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_load_data_row_truncated,
        "01000",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

void mylite_execution_diagnostics_set_load_data_null_to_not_null_error(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Column set to default value; NULL supplied to NOT NULL column '%s' at row %zu",
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_load_data_null_to_not_null,
        "22004",
        message
    );
}

int mylite_execution_diagnostics_append_load_data_null_to_not_null_warning(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Column set to default value; NULL supplied to NOT NULL column '%s' at row %zu",
        column_name,
        row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_load_data_null_to_not_null,
        "22004",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

void mylite_execution_diagnostics_set_spatial_bad_null_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Column '%s' cannot be null", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_spatial_column_cannot_be_null,
        "23000",
        message
    );
}

void mylite_execution_diagnostics_set_generated_column_value_error(
    struct mylite_db *database,
    const char *column_name,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "The value specified for generated column '%s' in table '%s' is not allowed.",
        column_name,
        table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_generated_column_value,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_data_truncated_error(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Data truncated for column '%s' at row %zu",
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_data_truncated,
        "01000",
        message
    );
}

void mylite_execution_diagnostics_set_data_too_long_error(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Data too long for column '%s' at row %zu",
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_data_too_long,
        "22001",
        message
    );
}

void mylite_execution_diagnostics_set_invalid_default_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Invalid default value for '%s'", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_default,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_json_cant_have_default_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "BLOB, TEXT, GEOMETRY or JSON column '%s' can't have a default value",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_blob_text_cant_have_default,
        "42000",
        message
    );
}

int mylite_execution_diagnostics_append_blob_text_cant_have_default_warning(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "BLOB, TEXT, GEOMETRY or JSON column '%s' can't have a default value",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_blob_text_cant_have_default,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_invalid_json_text_error(
    struct mylite_db *database,
    size_t position,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Invalid JSON text: \"Invalid value.\" at position %zu in value for column '%s'.",
        position,
        column_name == NULL ? "" : column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_json_text,
        "22032",
        message
    );
}

void mylite_execution_diagnostics_set_no_default_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Field '%s' doesn't have a default value", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_field_no_default,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_default_function_expression_error(struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_default_val_generated,
        "HY000",
        "DEFAULT function cannot be used with default value expressions"
    );
}

void mylite_execution_diagnostics_set_out_of_range_error(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Out of range value for column '%s' at row %zu",
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_data_out_of_range,
        "22003",
        message
    );
}

void mylite_execution_diagnostics_set_invalid_column_size_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Invalid size for column '%s'.", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_column_size,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_date_value_error(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect date value: '%s' for column '%s' at row %zu",
        value_text,
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_date_value,
        "22007",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_time_value_error(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect time value: '%s' for column '%s' at row %zu",
        value_text,
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_time_value,
        "22007",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_datetime_value_error(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect datetime value: '%s' for column '%s' at row %zu",
        value_text,
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_date_value,
        "22007",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_date_literal_error(
    struct mylite_db *database,
    const char *value_text
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Incorrect DATE value: '%s'", value_text);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_timestamp_value,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_datetime_literal_error(
    struct mylite_db *database,
    const char *value_text
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Incorrect DATETIME value: '%s'", value_text);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_timestamp_value,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_timestamp_value_error(
    struct mylite_db *database,
    const char *value_text
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Incorrect TIMESTAMP value: '%s'", value_text);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_timestamp_value,
        "HY000",
        message
    );
}

int mylite_execution_diagnostics_append_incorrect_datetime_predicate_warning(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect datetime value: '%s' for column '%s' at row 1",
        value_text,
        column_name
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_incorrect_temporal,
        "22007",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_incorrect_date_predicate_warning(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect date value: '%s' for column '%s' at row 1",
        value_text,
        column_name
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_incorrect_temporal,
        "22007",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_incorrect_date_value_note(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect date value: '%s' for column '%s' at row %zu",
        value_text,
        column_name,
        row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_note(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_date_value,
        "22007",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_bad_null_warning(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Column '%s' cannot be null", column_name);
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_bad_null,
        "23000",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_no_default_warning(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Field '%s' doesn't have a default value", column_name);
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_field_no_default,
        "HY000",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_out_of_range_warning(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Out of range value for column '%s' at row %zu",
        column_name,
        row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_data_out_of_range,
        "22003",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

void mylite_execution_diagnostics_set_incorrect_integer_value_error(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect integer value: '%s' for column '%s' at row %zu",
        value_text,
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_truncated_wrong_value_for_field,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_incorrect_decimal_value_error(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect decimal value: '%s' for column '%s' at row %zu",
        value_text,
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_truncated_wrong_value_for_field,
        "HY000",
        message
    );
}

int mylite_execution_diagnostics_append_incorrect_integer_value_warning(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect integer value: '%s' for column '%s' at row %zu",
        value_text,
        column_name,
        row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_truncated_wrong_value_for_field,
        "HY000",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_incorrect_decimal_value_warning(
    struct mylite_db *database,
    const char *value_text,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect decimal value: '%s' for column '%s' at row %zu",
        value_text,
        column_name,
        row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_truncated_wrong_value_for_field,
        "HY000",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_data_truncated_warning(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Data truncated for column '%s' at row %zu",
        column_name,
        row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_data_truncated,
        "01000",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_data_too_long_warning(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Data too long for column '%s' at row %zu",
        column_name,
        row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_data_too_long,
        "22001",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_data_truncated_note(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Data truncated for column '%s' at row %zu",
        column_name,
        row_number
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_note(
        mylite_connection_diagnostics(database),
        mysql_error_data_truncated,
        "01000",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
    }
    return rc;
}

int mylite_execution_diagnostics_append_decimal_truncated_note(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    return mylite_execution_diagnostics_append_data_truncated_note(
        database,
        column_name,
        row_number
    );
}

void mylite_execution_diagnostics_set_display_width_out_of_range_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Display width out of range for column '%s' (max = 255)",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_display_width_out_of_range,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_text_display_width_out_of_range_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Display width out of range for column '%s' (max = 4294967295)",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_display_width_out_of_range,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_bit_display_width_out_of_range_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Display width out of range for column '%s' (max = 64)",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_display_width_out_of_range,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_invalid_year_display_width_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_year_display_width,
        "HY000",
        "Invalid display width. Use YEAR instead."
    );
}

void mylite_execution_diagnostics_set_decimal_precision_too_big_error(
    struct mylite_db *database,
    const char *column_name,
    uint64_t precision
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Too-big precision %" PRIu64 " specified for '%s'. Maximum is 65.",
        precision,
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_decimal_precision_too_big,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_temporal_precision_too_big_error(
    struct mylite_db *database,
    const char *subject_name,
    uint64_t precision
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Too-big precision %" PRIu64 " specified for '%s'. Maximum is 6.",
        precision,
        subject_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_decimal_precision_too_big,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_decimal_scale_too_big_error(
    struct mylite_db *database,
    const char *column_name,
    uint64_t scale
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Too big scale %" PRIu64 " specified for column '%s'. Maximum is 30.",
        scale,
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_decimal_scale_too_big,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_decimal_scale_greater_than_precision_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "For float(M,D), double(M,D) or decimal(M,D), M must be >= D (column '%s').",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_decimal_must_be_greater_or_equal_to_d,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_predicate_out_of_range_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Out of range value for column '%s' in WHERE",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_data_out_of_range,
        "22003",
        message
    );
}

void mylite_execution_diagnostics_set_having_out_of_range_error(
    struct mylite_db *database,
    const char *operand_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Out of range value for '%s' in HAVING", operand_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_data_out_of_range,
        "22003",
        message
    );
}

void mylite_execution_diagnostics_set_limit_out_of_range_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        "LIMIT literal is outside the supported range"
    );
}

void mylite_execution_diagnostics_set_regexp_error(
    struct mylite_db *database,
    const char *message
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_regular_expression,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_regexp_illegal_argument_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_regexp_illegal_argument,
        "HY000",
        "Illegal argument to a regular expression."
    );
}

void mylite_execution_diagnostics_set_regexp_character_range_error(
    struct mylite_db *database,
    const char *message
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_regular_expression_character_range,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_identifier_too_long_error(
    struct mylite_db *database,
    const char *kind
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "%s identifier is too long", kind);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_identifier_too_long,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_reserved_name_error(
    struct mylite_db *database,
    const char *kind,
    const char *name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int code = mysql_error_incorrect_table_name;
    int written = 0;

    if (strcmp(kind, "database") == 0) {
        code = mysql_error_incorrect_database_name;
    } else if (strcmp(kind, "column") == 0) {
        code = mysql_error_incorrect_column_name;
    }

    written = snprintf(message, sizeof(message), "Incorrect %s name '%s'", kind, name);
    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(mylite_connection_diagnostics(database), code, "42000", message);
}
