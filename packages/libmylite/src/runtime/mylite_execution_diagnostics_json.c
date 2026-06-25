#include "mylite_execution_diagnostics_internal.h"

void mylite_execution_diagnostics_set_native_function_parameter_count_error(
    struct mylite_db *database,
    const char *function_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect parameter count in the call to native function '%s'",
        function_name
    );

    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_parameter_count,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_invalid_json_function_text_error(
    struct mylite_db *database,
    size_t position
) {
    struct mylite_json_normalize_result result = {
        .status = MYLITE_JSON_NORMALIZE_INVALID,
        .position = position,
    };
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Invalid JSON text in argument 1 to JSON function: \"%s\" at position %zu.",
        mylite_json_invalid_text_error_message(&result),
        result.position
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_json_text_in_function,
        "22032",
        message
    );
}

int mylite_execution_diagnostics_append_invalid_json_value_warning(
    struct mylite_db *database,
    const struct mylite_json_normalize_result *result
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t position = result == NULL ? 0U : result->position;
    int written = snprintf(
        message,
        sizeof(message),
        "Invalid JSON text in argument 1 to function json_value: \"%s\" at position %zu.",
        mylite_json_invalid_text_error_message(result),
        position
    );

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_json_text_in_function,
        "22032",
        message
    );
}

void mylite_execution_diagnostics_set_invalid_json_path_error(
    struct mylite_db *database,
    size_t position
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Invalid JSON path expression. The error is around character position %zu.",
        position
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_json_path,
        "42000",
        message
    );
}

void mylite_execution_diagnostics_set_json_path_not_allowed_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_json_path_not_allowed,
        "42000",
        "The path expression '$' is not allowed in this context."
    );
}

void mylite_execution_diagnostics_set_json_path_not_array_cell_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_json_path_not_array_cell,
        "42000",
        "A path expression is not a path to a cell in an array."
    );
}

void mylite_execution_diagnostics_set_invalid_json_data_type_error(
    struct mylite_db *database,
    const char *function_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Invalid data type for JSON data in argument 1 to function %s; a JSON string or JSON type "
        "is required.",
        function_name == NULL ? "JSON" : function_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_json_data,
        "22032",
        message
    );
}

void mylite_execution_diagnostics_set_invalid_json_one_or_all_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_json_one_or_all,
        "42000",
        "The oneOrAll argument to json_contains_path may take these values: 'one' or 'all'."
    );
}

void mylite_execution_diagnostics_set_json_unquote_incorrect_type_error(struct mylite_db *database
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_json_unquote_incorrect_type,
        "HY000",
        "Incorrect type for argument 1 in function JSON_UNQUOTE."
    );
}

void mylite_execution_diagnostics_set_json_quote_incorrect_type_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_json_quote_incorrect_type,
        "HY000",
        "Incorrect type for argument 1 in function json_quote."
    );
}

void mylite_execution_diagnostics_set_json_binary_charset_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_json_charset,
        "22032",
        "Cannot create a JSON value from a string with CHARACTER SET 'binary'."
    );
}

void mylite_execution_diagnostics_set_json_null_member_name_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_json_null_member_name,
        "22032",
        "JSON documents may not contain NULL member names."
    );
}
