static int apply_set_max_allowed_packet_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    if (target == NULL) {
        set_runtime_error(database, "invalid max_allowed_packet target");
        return MYLITE_ERROR;
    }
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_session_read_only_system_variable_error(database, target->name);
        return MYLITE_ERROR;
    }

    return validate_set_fixed_uint64_value(
        database,
        value_node,
        max_allowed_packet_default_value,
        "SET max_allowed_packet supports only fixed no-op global assignments"
    );
}

static int validate_set_fixed_uint64_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    uint64_t expected_value,
    const char *unsupported_message
) {
    const struct mylite_sql_ast_node *literal_node = unwrap_parenthesized_expression(value_node);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t actual_value = 0U;

    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }
    if (literal_node != NULL && literal_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(literal_node);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, unsupported_message);
            return MYLITE_ERROR;
        }
        literal_node = unwrap_parenthesized_expression(child_at(literal_node, 0U));
    }
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER ||
        parse_unsigned_integer_literal(&literal_node->span, &actual_value) != MYLITE_OK ||
        actual_value != expected_value) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_set_fixed_decimal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    uint64_t expected_integral_value,
    const char *unsupported_message
) {
    const struct mylite_sql_ast_node *literal_node = unwrap_parenthesized_expression(value_node);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }
    if (literal_node != NULL && literal_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(literal_node);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, unsupported_message);
            return MYLITE_ERROR;
        }
        literal_node = unwrap_parenthesized_expression(child_at(literal_node, 0U));
    }
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if ((literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER &&
         literal_kind != MYLITE_SQL_AST_LITERAL_DECIMAL) ||
        !fixed_decimal_text_matches_uint64(literal_node->span, expected_integral_value)) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static bool fixed_decimal_text_matches_uint64(
    struct mylite_sql_source_span text,
    uint64_t expected_value
) {
    struct mylite_sql_source_span integer_span = {0};
    size_t index = 0U;
    uint64_t actual_value = 0U;

    if (text.text == NULL || text.length == 0U) {
        return false;
    }
    while (index < text.length) {
        unsigned char byte = (unsigned char)text.text[index];

        if (byte < '0' || byte > '9') {
            break;
        }
        ++index;
    }
    if (index == 0U) {
        return false;
    }
    integer_span = (struct mylite_sql_source_span){.text = text.text, .length = index};
    if (parse_unsigned_integer_literal(&integer_span, &actual_value) != MYLITE_OK ||
        actual_value != expected_value) {
        return false;
    }
    if (index == text.length) {
        return true;
    }
    if (text.text[index] != '.') {
        return false;
    }
    ++index;
    while (index < text.length) {
        if (text.text[index] != '0') {
            return false;
        }
        ++index;
    }
    return true;
}

static int apply_set_transaction_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    struct transaction_characteristics characteristics = {
        .has_isolation = false,
        .has_access_mode = false,
        .has_consistent_snapshot = false,
        .isolation = MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ,
        .access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE,
    };
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid transaction system variable target");
        return MYLITE_ERROR;
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ISOLATION) {
        rc =
            parse_set_transaction_isolation_value(database, value_node, &characteristics.isolation);
        if (rc != MYLITE_OK) {
            return rc;
        }
        characteristics.has_isolation = true;
        if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL &&
            characteristics.isolation != MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ) {
            set_unsupported_error(
                database,
                "SET transaction_isolation supports only fixed no-op global assignments"
            );
            return MYLITE_ERROR;
        }
    } else if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_READ_ONLY) {
        rc = parse_set_transaction_read_only_value(
            database,
            value_node,
            &characteristics.access_mode
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        characteristics.has_access_mode = true;
        if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL &&
            characteristics.access_mode != MYLITE_TRANSACTION_ACCESS_READ_WRITE) {
            set_unsupported_error(
                database,
                "SET transaction_read_only supports only fixed no-op global assignments"
            );
            return MYLITE_ERROR;
        }
    } else {
        set_runtime_error(database, "invalid transaction system variable kind");
        return MYLITE_ERROR;
    }

    return apply_transaction_system_variable_characteristics(database, target, &characteristics);
}

static int apply_set_auto_increment_step_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = 1U;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid auto-increment system variable target");
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(
            database,
            "SET GLOBAL auto-increment system variable assignment is not supported"
        );
        return MYLITE_ERROR;
    }

    rc = parse_set_auto_increment_step_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_INCREMENT) {
        database->session.auto_increment_increment = value;
    } else if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_OFFSET) {
        database->session.auto_increment_offset = value;
    } else {
        set_runtime_error(database, "invalid auto-increment system variable kind");
        return MYLITE_ERROR;
    }
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_auto_increment_step_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    enum { max_auto_increment_step_value = 65535U };
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t magnitude = 0U;
    bool negative = false;
    char value_text[integer_text_capacity];

    if (out_value == NULL) {
        set_runtime_error(database, "invalid auto-increment system variable output");
        return MYLITE_ERROR;
    }
    *out_value = 1U;
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
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        return append_truncated_incorrect_auto_increment_warning(database, variable_name, "0");
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));

    if (negative) {
        return append_truncated_incorrect_auto_increment_warning(
            database,
            variable_name,
            value_text
        );
    }
    if (parse_unsigned_integer_literal(&literal_node->span, &magnitude) != MYLITE_OK ||
        magnitude > max_auto_increment_step_value) {
        *out_value = max_auto_increment_step_value;
        return append_truncated_incorrect_auto_increment_warning(
            database,
            variable_name,
            value_text
        );
    }
    if (magnitude == 0U) {
        return append_truncated_incorrect_auto_increment_warning(
            database,
            variable_name,
            value_text
        );
    }

    *out_value = magnitude;
    return MYLITE_OK;
}

static const struct mylite_sql_ast_node *unwrap_auto_increment_step_value_literal(
    const struct mylite_sql_ast_node *value_node,
    bool *out_negative
) {
    const struct mylite_sql_ast_node *literal_node = unwrap_parenthesized_expression(value_node);

    if (out_negative != NULL) {
        *out_negative = false;
    }
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return literal_node;
    }

    switch (mylite_sql_ast_node_operator(literal_node)) {
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
        if (out_negative != NULL) {
            *out_negative = true;
        }
        break;
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
        break;
    default:
        return NULL;
    }

    return unwrap_parenthesized_expression(child_at(literal_node, 0U));
}

static void copy_auto_increment_step_value_text(
    const struct mylite_sql_ast_node *value_node,
    char *buffer,
    size_t buffer_size
) {
    size_t copied = 0U;

    if (buffer == NULL || buffer_size == 0U) {
        return;
    }
    if (value_node != NULL && value_node->span.text != NULL) {
        copied = value_node->span.length < buffer_size ? value_node->span.length : buffer_size - 1U;
        memcpy(buffer, value_node->span.text, copied);
    }
    buffer[copied] = '\0';
}

static int set_incorrect_system_variable_argument_type_error(
    struct mylite_db *database,
    const char *variable_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect argument type to variable '%s'",
        variable_name == NULL ? "unknown" : variable_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_argument_type,
        "42000",
        message
    );
    return MYLITE_ERROR;
}

static int append_truncated_incorrect_auto_increment_warning(
    struct mylite_db *database,
    const char *variable_name,
    const char *value_text
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Truncated incorrect %s value: '%s'",
        variable_name == NULL ? "auto_increment" : variable_name,
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
