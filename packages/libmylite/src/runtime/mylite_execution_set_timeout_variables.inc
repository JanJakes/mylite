static int apply_set_timeout_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = timeout_system_variable_default_value_for_kind(
        target == NULL ? MYLITE_EXECUTION_SYSTEM_VARIABLE_NONE : target->kind
    );
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid timeout system variable target");
        return MYLITE_ERROR;
    }
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL &&
        !mylite_execution_system_variable_allows_session_scope(target->kind)) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        return validate_set_fixed_timeout_system_variable_value(database, target, value_node);
    }
    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return apply_timeout_system_variable_value(database, target, value);
    }

    rc = parse_set_timeout_system_variable_value(database, target, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return apply_timeout_system_variable_value(database, target, value);
}

static int apply_timeout_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t value
) {
    if (database == NULL || target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_WAIT_TIMEOUT) {
        database->session.wait_timeout = value;
    } else if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT) {
        database->session.interactive_timeout = value;
    } else if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCK_WAIT_TIMEOUT) {
        database->session.lock_wait_timeout = value;
    } else if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_LOCK_WAIT_TIMEOUT) {
        database->session.innodb_lock_wait_timeout = value;
    } else if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_READ_TIMEOUT) {
        database->session.net_read_timeout = value;
    } else if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_RETRY_COUNT) {
        database->session.net_retry_count = value;
    } else if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_WRITE_TIMEOUT) {
        database->session.net_write_timeout = value;
    } else {
        set_runtime_error(database, "invalid timeout system variable kind");
        return MYLITE_ERROR;
    }
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int validate_set_fixed_timeout_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = 0U;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid timeout system variable target");
        return MYLITE_ERROR;
    }
    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }

    rc = parse_set_timeout_system_variable_value(database, target, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value == timeout_system_variable_default_value_for_kind(target->kind)) {
        return MYLITE_OK;
    }

    set_unsupported_error(
        database,
        "SET timeout system variable supports only fixed no-op global assignments"
    );
    return MYLITE_ERROR;
}

static int parse_set_timeout_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool negative = false;
    char value_text[integer_text_capacity];

    if (out_value == NULL) {
        set_runtime_error(database, "invalid timeout system variable output");
        return MYLITE_ERROR;
    }
    if (target == NULL) {
        set_runtime_error(database, "invalid timeout system variable target");
        return MYLITE_ERROR;
    }
    *out_value = timeout_system_variable_default_value_for_kind(target->kind);
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
        if (*out_value < timeout_system_variable_min_value_for_kind(target->kind)) {
            *out_value = timeout_system_variable_min_value_for_kind(target->kind);
            return append_truncated_incorrect_system_variable_warning(database, target->name, "1");
        }
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "FALSE")) {
            return set_incorrect_system_variable_argument_type_error(database, target->name);
        }
        *out_value = timeout_system_variable_min_value_for_kind(target->kind);
        return append_truncated_incorrect_system_variable_warning(database, target->name, "0");
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
    return parse_timeout_system_variable_integer(
        database,
        target,
        &literal_node->span,
        negative,
        value_text,
        out_value
    );
}

static int parse_timeout_system_variable_integer(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
) {
    uint64_t magnitude = 0U;
    uint64_t min_value = 1U;
    uint64_t max_value = timeout_system_variable_max_value;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid timeout system variable output");
        return MYLITE_ERROR;
    }
    if (target == NULL) {
        set_runtime_error(database, "invalid timeout system variable target");
        return MYLITE_ERROR;
    }
    min_value = timeout_system_variable_min_value_for_kind(target->kind);
    max_value = timeout_system_variable_max_value_for_kind(target->kind);
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
        if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_RETRY_COUNT) {
            return set_incorrect_system_variable_argument_type_error(database, target->name);
        }
        *out_value = max_value;
        return append_truncated_incorrect_system_variable_warning(
            database,
            target->name,
            value_text
        );
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

static int parse_set_timeout_system_variable_cell_value(
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

    if (out_value == NULL) {
        set_runtime_error(database, "invalid timeout system variable output");
        return MYLITE_ERROR;
    }
    if (target == NULL) {
        set_runtime_error(database, "invalid timeout system variable target");
        return MYLITE_ERROR;
    }
    *out_value = timeout_system_variable_default_value_for_kind(target->kind);
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    if (value->has_value_size) {
        text_size = value->value_size;
    } else {
        text_size = strlen(text);
    }
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
    return parse_timeout_system_variable_integer(
        database,
        target,
        &unsigned_span,
        negative,
        value->value,
        out_value
    );
}

static int apply_set_long_query_time_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = long_query_time_default_microseconds;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid long_query_time system variable target");
        return MYLITE_ERROR;
    }

    rc = parse_set_long_query_time_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        if (value != long_query_time_default_microseconds) {
            set_unsupported_error(
                database,
                "SET GLOBAL long_query_time assignment is not supported"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    database->session.long_query_time_microseconds = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int apply_set_sort_buffer_size_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = sort_buffer_size_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid sort_buffer_size system variable target");
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            sort_buffer_size_default_value,
            "SET GLOBAL sort_buffer_size assignment is not supported"
        );
    }

    rc = parse_set_sort_buffer_size_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.sort_buffer_size = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_long_query_time_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool negative = false;
    char value_text[integer_text_capacity];

    if (out_value == NULL) {
        set_runtime_error(database, "invalid long_query_time output");
        return MYLITE_ERROR;
    }
    *out_value = long_query_time_default_microseconds;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }

    literal_node = unwrap_auto_increment_step_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER &&
        literal_kind != MYLITE_SQL_AST_LITERAL_DECIMAL &&
        literal_kind != MYLITE_SQL_AST_LITERAL_FLOAT) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
    return parse_long_query_time_text(
        database,
        value_text,
        strlen(value_text),
        variable_name,
        out_value
    );
}

static int parse_set_sort_buffer_size_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool negative = false;
    char value_text[integer_text_capacity];

    if (out_value == NULL) {
        set_runtime_error(database, "invalid sort_buffer_size output");
        return MYLITE_ERROR;
    }
    *out_value = sort_buffer_size_default_value;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }

    literal_node = unwrap_auto_increment_step_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        const struct mylite_sql_source_span true_span = {.text = "1", .length = 1U};

        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "TRUE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        return parse_sort_buffer_size_integer(
            database,
            variable_name,
            &true_span,
            negative,
            negative ? "-1" : "1",
            out_value
        );
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        const struct mylite_sql_source_span false_span = {.text = "0", .length = 1U};

        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "FALSE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        return parse_sort_buffer_size_integer(
            database,
            variable_name,
            &false_span,
            negative,
            negative ? "-0" : "0",
            out_value
        );
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
    return parse_sort_buffer_size_integer(
        database,
        variable_name,
        &literal_node->span,
        negative,
        value_text,
        out_value
    );
}

static int parse_sort_buffer_size_integer(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
) {
    uint64_t magnitude = 0U;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid sort_buffer_size output");
        return MYLITE_ERROR;
    }
    if (negative) {
        *out_value = sort_buffer_size_min_value;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value_text
        );
    }
    if (unsigned_span == NULL ||
        parse_unsigned_integer_literal(unsigned_span, &magnitude) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (magnitude < sort_buffer_size_min_value) {
        *out_value = sort_buffer_size_min_value;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value_text
        );
    }

    *out_value = magnitude;
    return MYLITE_OK;
}

static int apply_set_long_query_time_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    uint64_t long_query_time = long_query_time_default_microseconds;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }

    rc = parse_set_long_query_time_cell_value(
        database,
        target->name,
        value,
        value_kind,
        &long_query_time
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        if (long_query_time != long_query_time_default_microseconds) {
            set_unsupported_error(
                database,
                "SET GLOBAL long_query_time assignment is not supported"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    database->session.long_query_time_microseconds = long_query_time;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_long_query_time_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
) {
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid long_query_time output");
        return MYLITE_ERROR;
    }
    *out_value = long_query_time_default_microseconds;
    if (value == NULL || text == NULL ||
        (value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER &&
         value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_DECIMAL)) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    text_size = value->has_value_size ? value->value_size : strlen(text);
    return parse_long_query_time_text(database, text, text_size, variable_name, out_value);
}

static int apply_set_sort_buffer_size_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    uint64_t sort_buffer_size = sort_buffer_size_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(
            database,
            "SET sort_buffer_size from user variables is not supported for global scope"
        );
        return MYLITE_ERROR;
    }

    rc = parse_set_sort_buffer_size_cell_value(
        database,
        target->name,
        value,
        value_kind,
        &sort_buffer_size
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.sort_buffer_size = sort_buffer_size;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_sort_buffer_size_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    bool negative = false;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid sort_buffer_size output");
        return MYLITE_ERROR;
    }
    *out_value = sort_buffer_size_default_value;
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    text_size = value->has_value_size ? value->value_size : strlen(text);
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (text[0] == '+' || text[0] == '-') {
        negative = text[0] == '-';
        ++text;
        --text_size;
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    unsigned_span = (struct mylite_sql_source_span){.text = text, .length = text_size};
    return parse_sort_buffer_size_integer(
        database,
        variable_name,
        &unsigned_span,
        negative,
        value->value,
        out_value
    );
}

static int parse_long_query_time_text(
    struct mylite_db *database,
    const char *text,
    size_t text_size,
    const char *variable_name,
    uint64_t *out_value
) {
    enum { long_query_time_text_capacity = 128U };

    char buffer[long_query_time_text_capacity];
    char *end = NULL;
    long double seconds = 0.0L;
    long double microseconds = 0.0L;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid long_query_time output");
        return MYLITE_ERROR;
    }
    *out_value = long_query_time_default_microseconds;
    if (text == NULL || text_size == 0U || text_size >= sizeof(buffer)) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    memcpy(buffer, text, text_size);
    buffer[text_size] = '\0';
    errno = 0;
    seconds = strtold(buffer, &end);
    if (end == buffer || (end != NULL && *end != '\0')) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (seconds < 0.0L) {
        *out_value = 0U;
        return append_truncated_incorrect_system_variable_warning(database, variable_name, buffer);
    }

    microseconds = seconds * long_query_time_microseconds_per_second;
    if (!isfinite(microseconds) || microseconds > (long double)long_query_time_max_microseconds) {
        *out_value = long_query_time_max_microseconds;
        return append_truncated_incorrect_system_variable_warning(database, variable_name, buffer);
    }

    microseconds = floorl(microseconds + long_query_time_round_half_microsecond);
    if (microseconds > (long double)long_query_time_max_microseconds) {
        *out_value = long_query_time_max_microseconds;
        return append_truncated_incorrect_system_variable_warning(database, variable_name, buffer);
    }
    *out_value = (uint64_t)microseconds;
    return MYLITE_OK;
}

static int append_truncated_incorrect_system_variable_warning(
    struct mylite_db *database,
    const char *variable_name,
    const char *value_text
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Truncated incorrect %s value: '%s'",
        variable_name == NULL ? "system variable" : variable_name,
        value_text == NULL ? "" : value_text
    );

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_incorrect_auto_increment,
        "HY000",
        message
    );
}
