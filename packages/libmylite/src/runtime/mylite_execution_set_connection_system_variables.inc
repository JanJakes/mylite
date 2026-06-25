static int apply_set_connection_memory_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = connection_memory_chunk_size_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid connection memory system variable target");
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            connection_memory_system_variable_default_value_for_kind(target->kind),
            "SET GLOBAL connection memory system variable assignment is not supported"
        );
    }
    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return apply_connection_memory_system_variable_value(
            database,
            target,
            connection_memory_system_variable_default_value_for_kind(target->kind)
        );
    }

    rc = parse_set_connection_memory_system_variable_value(database, target, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return apply_connection_memory_system_variable_value(database, target, value);
}

static int apply_set_global_connection_system_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    const char *unsupported_message =
        "SET connection system variable supports only fixed no-op global assignments";
    uint64_t expected_value = 0U;

    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!mylite_execution_system_variable_is_fixed_global_connection_system(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_HOST_CACHE_SIZE:
        expected_value = host_cache_size_system_variable_value;
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GLOBAL_CONNECTION_MEMORY_LIMIT:
        expected_value = global_connection_memory_limit_default_value;
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_FAILED_CONNECTIONS_THRESHOLD:
        expected_value = connection_control_failed_connections_threshold_default_value;
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_MAX_CONNECTION_DELAY:
        expected_value = connection_control_max_connection_delay_default_value;
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_CONTROL_MIN_CONNECTION_DELAY:
        expected_value = connection_control_min_connection_delay_default_value;
        break;
    default:
        set_runtime_error(database, "invalid fixed connection system variable");
        return MYLITE_ERROR;
    }

    return validate_set_fixed_uint64_value(
        database,
        value_node,
        expected_value,
        unsupported_message
    );
}

static int apply_set_connection_memory_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    uint64_t parsed_value = connection_memory_chunk_size_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(
            database,
            "SET GLOBAL connection memory system variable assignment is not supported"
        );
        return MYLITE_ERROR;
    }

    rc = parse_set_connection_memory_system_variable_cell_value(
        database,
        target,
        value,
        value_kind,
        &parsed_value
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    return apply_connection_memory_system_variable_value(database, target, parsed_value);
}

static int apply_connection_memory_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t value
) {
    if (database == NULL || target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_MEMORY_CHUNK_SIZE) {
        database->session.connection_memory_chunk_size = value;
    } else if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_MEMORY_LIMIT) {
        database->session.connection_memory_limit = value;
    } else {
        set_runtime_error(database, "invalid connection memory system variable kind");
        return MYLITE_ERROR;
    }
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_connection_memory_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool negative = false;
    char value_text[integer_text_capacity];

    if (target == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = connection_memory_system_variable_default_value_for_kind(target->kind);
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }

    literal_node = unwrap_auto_increment_step_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "TRUE")) {
            return set_incorrect_system_variable_argument_type_error(database, target->name);
        }
        *out_value = 1U;
        if (*out_value < connection_memory_system_variable_min_value_for_kind(target->kind)) {
            *out_value = connection_memory_system_variable_min_value_for_kind(target->kind);
            return append_truncated_incorrect_system_variable_warning(
                database,
                target->name,
                "1"
            );
        }
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "FALSE")) {
            return set_incorrect_system_variable_argument_type_error(database, target->name);
        }
        *out_value = connection_memory_system_variable_min_value_for_kind(target->kind);
        return append_truncated_incorrect_system_variable_warning(database, target->name, "0");
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
    return parse_connection_memory_system_variable_integer(
        database,
        target,
        &literal_node->span,
        negative,
        value_text,
        out_value
    );
}

static int parse_set_connection_memory_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    bool negative = false;

    if (target == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = connection_memory_system_variable_default_value_for_kind(target->kind);
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    text_size = value->has_value_size ? value->value_size : strlen(text);
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }
    if (text[0] == '+' || text[0] == '-') {
        negative = text[0] == '-';
        ++text;
        --text_size;
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    unsigned_span = (struct mylite_sql_source_span){.text = text, .length = text_size};
    return parse_connection_memory_system_variable_integer(
        database,
        target,
        &unsigned_span,
        negative,
        value->value,
        out_value
    );
}

static int parse_connection_memory_system_variable_integer(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
) {
    uint64_t magnitude = 0U;
    uint64_t min_value = 1U;
    uint64_t max_value = UINT64_MAX;

    if (target == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    min_value = connection_memory_system_variable_min_value_for_kind(target->kind);
    max_value = connection_memory_system_variable_max_value_for_kind(target->kind);
    if (negative) {
        *out_value = min_value;
        return append_truncated_incorrect_system_variable_warning(
            database,
            target->name,
            value_text
        );
    }
    if (unsigned_span == NULL ||
        parse_unsigned_integer_literal(unsigned_span, &magnitude) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }
    if (magnitude > max_value) {
        *out_value = max_value;
        return append_truncated_incorrect_system_variable_warning(
            database,
            target->name,
            value_text
        );
    }
    if (magnitude < min_value) {
        *out_value = min_value;
        return append_truncated_incorrect_system_variable_warning(
            database,
            target->name,
            value_text
        );
    }

    *out_value = magnitude;
    return MYLITE_OK;
}

static uint64_t connection_memory_system_variable_default_value_for_kind(
    enum mylite_execution_system_variable_kind kind
) {
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_MEMORY_CHUNK_SIZE) {
        return connection_memory_chunk_size_default_value;
    }
    return connection_memory_limit_default_value;
}

static uint64_t connection_memory_system_variable_min_value_for_kind(
    enum mylite_execution_system_variable_kind kind
) {
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_MEMORY_LIMIT) {
        return connection_memory_limit_min_value;
    }
    return connection_memory_chunk_size_min_value;
}

static uint64_t connection_memory_system_variable_max_value_for_kind(
    enum mylite_execution_system_variable_kind kind
) {
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_CONNECTION_MEMORY_CHUNK_SIZE) {
        return connection_memory_chunk_size_max_value;
    }
    return UINT64_MAX;
}
