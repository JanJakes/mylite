#include "mylite_execution_diagnostics_internal.h"

enum {
    system_variable_body_offset = 2,
};

static bool diagnostics_text_equals_ascii_case_insensitive(const char *left, const char *right);
static unsigned char diagnostics_ascii_lower(unsigned char value);
static int mylite_execution_diagnostics_copy_system_variable_name_for_error(
    const struct mylite_sql_source_span *span,
    char **out_name
);
static int mylite_execution_diagnostics_copy_system_variable_component_name_for_error(
    const struct mylite_sql_source_span *span,
    size_t *offset,
    char **out_name
);
static int mylite_execution_diagnostics_copy_system_variable_raw_body_for_error(
    const struct mylite_sql_source_span *span,
    char **out_name
);
static int mylite_execution_diagnostics_append_system_variable_error_name_byte(
    char value,
    char **name,
    size_t *length,
    size_t capacity
);

void mylite_execution_diagnostics_set_session_variable_only_error(
    struct mylite_db *database,
    const char *variable_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Variable '%s' is a SESSION variable", variable_name);

    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_session_variable_only,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_global_variable_only_error(
    struct mylite_db *database,
    const char *variable_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Variable '%s' is a GLOBAL variable", variable_name);

    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_session_variable_only,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_global_variable_set_global_required_error(
    struct mylite_db *database,
    const char *variable_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Variable '%s' is a GLOBAL variable and should be set with SET GLOBAL",
        variable_name
    );

    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_global_variable_only,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_session_read_only_system_variable_error(
    struct mylite_db *database,
    const char *variable_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "SESSION variable '%s' is read-only. Use SET GLOBAL to assign the value",
        variable_name
    );

    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_session_variable_read_only,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_read_only_system_variable_error(
    struct mylite_db *database,
    const char *variable_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Variable '%s' is a read only variable", variable_name);

    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_session_variable_only,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_system_variable_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    char *variable_name = NULL;
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *display_name = "unknown";
    int written = 0;

    if (expression != NULL) {
        (void)mylite_execution_diagnostics_copy_system_variable_name_for_error(
            &expression->span,
            &variable_name
        );
    }
    if (variable_name != NULL) {
        display_name = variable_name;
    }

    written = snprintf(message, sizeof(message), "Unknown system variable '%s'", display_name);
    if (written < 0) {
        message[0] = '\0';
    }
    free(variable_name);

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_system_variable,
        "HY000",
        message
    );
}

void mylite_execution_diagnostics_set_unknown_system_variable_name_error(
    struct mylite_db *database,
    const char *variable_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *display_name = variable_name == NULL ? "unknown" : variable_name;
    int written = snprintf(message, sizeof(message), "Unknown system variable '%s'", display_name);

    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_system_variable,
        "HY000",
        message
    );
}

static int mylite_execution_diagnostics_copy_system_variable_name_for_error(
    const struct mylite_sql_source_span *span,
    char **out_name
) {
    char *first = NULL;
    char *second = NULL;
    size_t offset = system_variable_body_offset;
    int rc = MYLITE_OK;

    if (out_name == NULL) {
        return MYLITE_MISUSE;
    }
    *out_name = NULL;
    if (span == NULL || span->text == NULL || span->length <= system_variable_body_offset) {
        return MYLITE_ERROR;
    }

    rc = mylite_execution_diagnostics_copy_system_variable_component_name_for_error(
        span,
        &offset,
        &first
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (offset < span->length && span->text[offset] == '.') {
        ++offset;
        rc = mylite_execution_diagnostics_copy_system_variable_component_name_for_error(
            span,
            &offset,
            &second
        );
        if (rc != MYLITE_OK) {
            free(first);
            return rc;
        }
        if (diagnostics_text_equals_ascii_case_insensitive(first, "session") ||
            diagnostics_text_equals_ascii_case_insensitive(first, "local") ||
            diagnostics_text_equals_ascii_case_insensitive(first, "global")) {
            free(first);
            *out_name = second;
            return MYLITE_OK;
        }
        free(second);
        free(first);
        return mylite_execution_diagnostics_copy_system_variable_raw_body_for_error(span, out_name);
    }

    *out_name = first;
    return MYLITE_OK;
}

static int mylite_execution_diagnostics_copy_system_variable_component_name_for_error(
    const struct mylite_sql_source_span *span,
    size_t *offset,
    char **out_name
) {
    const size_t capacity = span->length + 1U;
    size_t length = 0U;
    char *name = (char *)malloc(capacity);

    if (name == NULL) {
        return MYLITE_NOMEM;
    }
    name[0] = '\0';

    if (*offset < span->length && span->text[*offset] == '`') {
        ++*offset;
        while (*offset < span->length) {
            char value = span->text[*offset];

            if (value == '`') {
                ++*offset;
                if (*offset < span->length && span->text[*offset] == '`') {
                    int byte_rc =
                        mylite_execution_diagnostics_append_system_variable_error_name_byte(
                            '`',
                            &name,
                            &length,
                            capacity
                        );
                    if (byte_rc != MYLITE_OK) {
                        return byte_rc;
                    }
                    ++*offset;
                    continue;
                }
                *out_name = name;
                return MYLITE_OK;
            }

            {
                int byte_rc = mylite_execution_diagnostics_append_system_variable_error_name_byte(
                    value,
                    &name,
                    &length,
                    capacity
                );
                if (byte_rc != MYLITE_OK) {
                    return byte_rc;
                }
            }
            ++*offset;
        }

        free(name);
        return MYLITE_ERROR;
    }

    while (*offset < span->length && span->text[*offset] != '.') {
        int byte_rc = mylite_execution_diagnostics_append_system_variable_error_name_byte(
            span->text[*offset],
            &name,
            &length,
            capacity
        );
        if (byte_rc != MYLITE_OK) {
            return byte_rc;
        }
        ++*offset;
    }

    *out_name = name;
    return MYLITE_OK;
}

static int mylite_execution_diagnostics_copy_system_variable_raw_body_for_error(
    const struct mylite_sql_source_span *span,
    char **out_name
) {
    const size_t length = span->length - system_variable_body_offset;
    char *name = (char *)malloc(length + 1U);

    if (name == NULL) {
        return MYLITE_NOMEM;
    }

    memcpy(name, span->text + system_variable_body_offset, length);
    name[length] = '\0';
    *out_name = name;
    return MYLITE_OK;
}

static int mylite_execution_diagnostics_append_system_variable_error_name_byte(
    char value,
    char **name,
    size_t *length,
    size_t capacity
) {
    if (*length + 1U >= capacity) {
        free(*name);
        *name = NULL;
        return MYLITE_NOMEM;
    }

    (*name)[*length] = value;
    ++*length;
    (*name)[*length] = '\0';
    return MYLITE_OK;
}

static bool diagnostics_text_equals_ascii_case_insensitive(const char *left, const char *right) {
    size_t index = 0U;

    if (left == NULL || right == NULL) {
        return false;
    }
    while (left[index] != '\0' && right[index] != '\0') {
        if (diagnostics_ascii_lower((unsigned char)left[index]) !=
            diagnostics_ascii_lower((unsigned char)right[index])) {
            return false;
        }
        ++index;
    }

    if (left[index] != '\0') {
        return false;
    }
    if (right[index] != '\0') {
        return false;
    }

    return true;
}

static unsigned char diagnostics_ascii_lower(unsigned char value) {
    if (value >= 'A' && value <= 'Z') {
        return (unsigned char)(value - 'A' + 'a');
    }
    return value;
}
