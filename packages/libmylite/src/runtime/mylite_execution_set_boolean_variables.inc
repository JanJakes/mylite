static int apply_set_sql_require_primary_key_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    bool value = false;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid sql_require_primary_key system variable target");
        return MYLITE_ERROR;
    }

    rc = parse_set_big_tables_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        if (value) {
            set_unsupported_error(
                database,
                "SET GLOBAL sql_require_primary_key assignment is not supported"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    database->session.sql_require_primary_key = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int apply_set_sql_require_primary_key_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    bool boolean_value = false;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid sql_require_primary_key system variable target");
        return MYLITE_ERROR;
    }

    rc = parse_set_big_tables_cell_value(database, target->name, value, value_kind, &boolean_value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        if (boolean_value) {
            set_unsupported_error(
                database,
                "SET GLOBAL sql_require_primary_key assignment is not supported"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    database->session.sql_require_primary_key = boolean_value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int apply_set_big_tables_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    bool value = false;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid big_tables system variable target");
        return MYLITE_ERROR;
    }

    rc = parse_set_big_tables_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        if (value) {
            set_unsupported_error(
                database,
                "SET big_tables supports only fixed no-op global assignments"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    database->session.big_tables = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int apply_set_low_priority_updates_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    bool value = false;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid low_priority_updates system variable target");
        return MYLITE_ERROR;
    }

    rc = parse_set_big_tables_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        if (value) {
            set_unsupported_error(
                database,
                "SET GLOBAL low_priority_updates assignment is not supported"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    database->session.low_priority_updates = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int apply_set_big_tables_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    bool big_tables = false;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }

    rc = parse_set_big_tables_cell_value(database, target->name, value, value_kind, &big_tables);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        if (big_tables) {
            set_unsupported_error(
                database,
                "SET big_tables supports only fixed no-op global assignments"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    database->session.big_tables = big_tables;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int apply_set_low_priority_updates_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    bool low_priority_updates = false;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }

    rc = parse_set_big_tables_cell_value(
        database,
        target->name,
        value,
        value_kind,
        &low_priority_updates
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        if (low_priority_updates) {
            set_unsupported_error(
                database,
                "SET GLOBAL low_priority_updates assignment is not supported"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    database->session.low_priority_updates = low_priority_updates;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_big_tables_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    bool *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t magnitude = 0U;
    bool negative = false;
    char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = false;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }
    if (value_node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        const struct mylite_sql_ast_node *inner_node =
            unwrap_parenthesized_expression(child_at(value_node, 0U));

        if (inner_node != NULL && inner_node->kind == MYLITE_SQL_AST_LITERAL &&
            (mylite_sql_ast_node_literal_kind(inner_node) == MYLITE_SQL_AST_LITERAL_TRUE ||
             mylite_sql_ast_node_literal_kind(inner_node) == MYLITE_SQL_AST_LITERAL_FALSE)) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
    }

    literal_node = unwrap_big_tables_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_value = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        char *decoded = NULL;
        size_t decoded_size = 0U;
        int rc = decode_sql_string_literal(
            database,
            literal_node,
            "SET big_tables supports only string literals",
            "SET big_tables string values do not support NUL bytes",
            &decoded,
            &decoded_size
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        if (text_equals_ascii_case_insensitive(decoded, "ON")) {
            *out_value = true;
            free(decoded);
            return MYLITE_OK;
        }
        if (text_equals_ascii_case_insensitive(decoded, "OFF")) {
            free(decoded);
            return MYLITE_OK;
        }
        free(decoded);
        copy_big_tables_value_text(value_node, value_text, sizeof(value_text));
        set_system_variable_cant_be_set_value_error(database, variable_name, value_text);
        return MYLITE_ERROR;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        set_system_variable_cant_be_set_value_error(database, variable_name, "NULL");
        return MYLITE_ERROR;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER ||
        parse_unsigned_integer_literal(&literal_node->span, &magnitude) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    copy_big_tables_value_text(value_node, value_text, sizeof(value_text));
    return finish_parse_set_big_tables_integer(
        database,
        variable_name,
        magnitude,
        negative,
        value_text,
        out_value
    );
}

static int parse_set_big_tables_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_value
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    uint64_t magnitude = 0U;
    bool negative = false;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = false;
    if (value == NULL || text == NULL) {
        set_system_variable_cant_be_set_value_error(database, variable_name, "NULL");
        return MYLITE_ERROR;
    }
    if (value->has_value_size) {
        text_size = value->value_size;
    } else {
        text_size = strlen(text);
    }
    if (value_kind == MYLITE_SESSION_USER_VARIABLE_VALUE_DECIMAL) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (value_kind == MYLITE_SESSION_USER_VARIABLE_VALUE_STRING) {
        if (text_equals_ascii_case_insensitive(text, "ON")) {
            *out_value = true;
            return MYLITE_OK;
        }
        if (text_equals_ascii_case_insensitive(text, "OFF")) {
            return MYLITE_OK;
        }
        set_system_variable_cant_be_set_value_error(database, variable_name, text);
        return MYLITE_ERROR;
    }
    if (value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER || text_size == 0U) {
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
    if (parse_unsigned_integer_literal(&unsigned_span, &magnitude) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    return finish_parse_set_big_tables_integer(
        database,
        variable_name,
        magnitude,
        negative,
        value->value,
        out_value
    );
}

static int finish_parse_set_big_tables_integer(
    struct mylite_db *database,
    const char *variable_name,
    uint64_t magnitude,
    bool negative,
    const char *value_text,
    bool *out_value
) {
    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (magnitude == 0U) {
        *out_value = false;
        return MYLITE_OK;
    }
    if (!negative && magnitude == 1U) {
        *out_value = true;
        return MYLITE_OK;
    }

    set_system_variable_cant_be_set_value_error(database, variable_name, value_text);
    return MYLITE_ERROR;
}

static const struct mylite_sql_ast_node *unwrap_big_tables_value_literal(
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

static void copy_big_tables_value_text(
    const struct mylite_sql_ast_node *value_node,
    char *buffer,
    size_t buffer_size
) {
    const struct mylite_sql_source_span *span = value_node == NULL ? NULL : &value_node->span;
    size_t source_offset = 0U;
    size_t source_length = 0U;
    size_t copied = 0U;

    if (buffer == NULL || buffer_size == 0U) {
        return;
    }
    if (span != NULL && span->text != NULL) {
        source_length = span->length;
        if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING &&
            source_length >= 2U) {
            source_offset = 1U;
            source_length -= 2U;
        }
        copied = source_length < buffer_size ? source_length : buffer_size - 1U;
        memcpy(buffer, &span->text[source_offset], copied);
    }
    buffer[copied] = '\0';
}

static int apply_set_autocommit_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    bool value = true;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid autocommit system variable target");
        return MYLITE_ERROR;
    }
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(database, "SET GLOBAL system variable assignment is not supported");
        return MYLITE_ERROR;
    }
    if (value_node->kind != MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        rc = parse_set_big_tables_value(database, target->name, value_node, &value);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    return apply_set_autocommit_boolean_value(database, value);
}

static int apply_set_autocommit_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    bool boolean_value = false;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid autocommit system variable target");
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(database, "SET GLOBAL system variable assignment is not supported");
        return MYLITE_ERROR;
    }

    rc = parse_set_big_tables_cell_value(
        database,
        target->name,
        value,
        value_kind,
        &boolean_value
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    return apply_set_autocommit_boolean_value(database, boolean_value);
}

static int apply_set_autocommit_boolean_value(struct mylite_db *database, bool value) {
    int rc = MYLITE_OK;

    if (value && !database->session.autocommit_enabled) {
        rc = commit_active_user_transaction(database);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    database->session.autocommit_enabled = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int apply_set_foreign_key_checks_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    bool value = true;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid foreign_key_checks system variable target");
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(
            database,
            "SET GLOBAL foreign_key_checks assignment is not supported"
        );
        return MYLITE_ERROR;
    }

    rc = parse_set_foreign_key_checks_value(database, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.foreign_key_checks_enabled = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_foreign_key_checks_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    bool *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t magnitude = 0U;
    bool negative = false;
    char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

    if (out_value == NULL) {
        set_runtime_error(database, "invalid foreign_key_checks output");
        return MYLITE_ERROR;
    }
    *out_value = true;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        *out_value = false;
        return MYLITE_OK;
    }

    literal_node = unwrap_foreign_key_checks_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        copy_foreign_key_checks_value_text(value_node, value_text, sizeof(value_text));
        set_system_variable_cant_be_set_value_error(database, "foreign_key_checks", value_text);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_value = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_value = false;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER && !negative &&
        parse_unsigned_integer_literal(&literal_node->span, &magnitude) == MYLITE_OK &&
        magnitude <= 1U) {
        *out_value = magnitude == 1U;
        return MYLITE_OK;
    }

    copy_foreign_key_checks_value_text(value_node, value_text, sizeof(value_text));
    set_system_variable_cant_be_set_value_error(database, "foreign_key_checks", value_text);
    return MYLITE_ERROR;
}

static const struct mylite_sql_ast_node *unwrap_foreign_key_checks_value_literal(
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

static void copy_foreign_key_checks_value_text(
    const struct mylite_sql_ast_node *value_node,
    char *buffer,
    size_t buffer_size
) {
    const struct mylite_sql_source_span *span = value_node == NULL ? NULL : &value_node->span;
    size_t source_offset = 0U;
    size_t source_length = 0U;
    size_t copied = 0U;

    if (buffer == NULL || buffer_size == 0U) {
        return;
    }
    if (span != NULL && span->text != NULL) {
        source_length = span->length;
        if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING &&
            source_length >= 2U) {
            source_offset = 1U;
            source_length -= 2U;
        }
        copied = source_length < buffer_size ? source_length : buffer_size - 1U;
        memcpy(buffer, &span->text[source_offset], copied);
    }
    buffer[copied] = '\0';
}
