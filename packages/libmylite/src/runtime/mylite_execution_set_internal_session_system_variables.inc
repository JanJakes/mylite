enum {
    original_server_version_max_value = 999999U,
    internal_session_system_variable_uint64_text_capacity = integer_text_capacity,
};

static int apply_set_internal_session_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    char *value = NULL;
    size_t value_size = 0U;
    bool boolean_value = false;
    int rc = MYLITE_OK;

    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!internal_session_system_variable_handles_kind(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_session_variable_set_global_error(database, target->name);
        return MYLITE_ERROR;
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_PSEUDO_REPLICA_MODE ||
        target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_PSEUDO_SLAVE_MODE ||
        target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ALLOW_BATCHING) {
        rc = parse_set_big_tables_value(database, target->name, value_node, &boolean_value);
        if (rc == MYLITE_OK) {
            rc = apply_set_session_placeholder_boolean_value(database, target, boolean_value);
        }
        if (rc == MYLITE_OK) {
            rc = append_internal_session_system_variable_set_warning_if_needed(
                database,
                target->kind
            );
        }
        return rc;
    }

    rc = copy_set_text_system_variable_node_value(
        database,
        value_node,
        target->kind,
        target->name,
        &value,
        &value_size
    );
    if (rc == MYLITE_OK) {
        rc = apply_set_internal_session_system_variable_text(database, target, value, value_size);
    }
    free(value);
    return rc;
}

static int apply_set_internal_session_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
) {
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    bool boolean_value = false;
    int rc = MYLITE_OK;

    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!internal_session_system_variable_handles_kind(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_session_variable_set_global_error(database, target->name);
        return MYLITE_ERROR;
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_PSEUDO_REPLICA_MODE ||
        target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_PSEUDO_SLAVE_MODE ||
        target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ALLOW_BATCHING) {
        rc = parse_set_big_tables_cell_value(
            database,
            target->name,
            value,
            value_kind,
            &boolean_value
        );
        if (rc == MYLITE_OK) {
            rc = apply_set_session_placeholder_boolean_value(database, target, boolean_value);
        }
        if (rc == MYLITE_OK) {
            rc = append_internal_session_system_variable_set_warning_if_needed(
                database,
                target->kind
            );
        }
        return rc;
    }

    if (text == NULL) {
        set_system_variable_cant_be_set_value_error(database, target->name, "NULL");
        return MYLITE_ERROR;
    }
    text_size = value->has_value_size ? value->value_size : strlen(text);
    return apply_set_internal_session_system_variable_text(database, target, text, text_size);
}

static bool internal_session_system_variable_handles_kind(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ORIGINAL_COMMIT_TIMESTAMP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ORIGINAL_SERVER_VERSION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PSEUDO_REPLICA_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PSEUDO_SLAVE_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RBR_EXEC_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ALLOW_BATCHING:
        return true;
    default:
        return false;
    }
}

static int apply_set_internal_session_system_variable_text(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *value,
    size_t value_size
) {
    if (target == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ORIGINAL_COMMIT_TIMESTAMP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ORIGINAL_SERVER_VERSION:
        return apply_set_internal_session_system_variable_uint64(
            database,
            target,
            value,
            value_size
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RBR_EXEC_MODE:
        return apply_set_internal_session_system_variable_rbr_exec_mode(database, target, value);
    default:
        set_runtime_error(database, "invalid internal session system variable assignment");
        return MYLITE_ERROR;
    }
}

static int apply_set_internal_session_system_variable_uint64(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *value,
    size_t value_size
) {
    uint64_t parsed = 0U;

    if (!parse_internal_session_system_variable_uint64_text(value, value_size, &parsed)) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_ORIGINAL_SERVER_VERSION &&
        parsed > original_server_version_max_value) {
        parsed = original_server_version_max_value;
    }
    return set_internal_session_system_variable_uint64_override(database, target, parsed);
}

static int apply_set_internal_session_system_variable_rbr_exec_mode(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *value
) {
    const char *canonical = NULL;

    if (text_equals_ascii_case_insensitive(value, "STRICT")) {
        canonical = "STRICT";
    } else if (text_equals_ascii_case_insensitive(value, "IDEMPOTENT")) {
        canonical = "IDEMPOTENT";
    } else {
        set_system_variable_cant_be_set_value_error(database, target->name, value);
        return MYLITE_ERROR;
    }

    return set_session_system_variable_override(
        database,
        target->kind,
        canonical,
        strlen(canonical)
    );
}

static bool parse_internal_session_system_variable_uint64_text(
    const char *text,
    size_t text_size,
    uint64_t *out_value
) {
    uint64_t value = 0U;
    size_t index = 0U;
    bool negative = false;

    if (text == NULL || out_value == NULL || text_size == 0U) {
        return false;
    }
    if (text[index] == '-' || text[index] == '+') {
        negative = text[index] == '-';
        ++index;
    }
    if (index == text_size) {
        return false;
    }
    for (; index < text_size; ++index) {
        unsigned char byte = (unsigned char)text[index];
        uint64_t digit = 0U;

        if (byte < (unsigned char)'0' || byte > (unsigned char)'9') {
            return false;
        }
        digit = (uint64_t)(byte - (unsigned char)'0');
        if (value > (UINT64_MAX - digit) / decimal_base) {
            return false;
        }
        value = value * decimal_base + digit;
    }

    *out_value = negative ? 0U : value;
    return true;
}

static int set_internal_session_system_variable_uint64_override(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t value
) {
    char text[internal_session_system_variable_uint64_text_capacity];
    int written = snprintf(text, sizeof(text), "%" PRIu64, value);

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (written < 0 || (size_t)written >= sizeof(text)) {
        set_runtime_error(database, "failed to format internal session system variable value");
        return MYLITE_ERROR;
    }
    return set_session_system_variable_override(database, target->kind, text, (size_t)written);
}

static int append_internal_session_system_variable_set_warning_if_needed(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
) {
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_PSEUDO_SLAVE_MODE) {
        return append_system_variable_read_warning(database, kind);
    }
    return MYLITE_OK;
}
