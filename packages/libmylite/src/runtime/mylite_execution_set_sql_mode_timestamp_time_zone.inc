static int apply_set_sql_mode_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node
) {
    char identifier[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *decoded = NULL;
    uint64_t modes = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return set_session_sql_mode(database, MYLITE_SESSION_SQL_MODE_DEFAULT_BITS);
    }
    if (value_node->kind == MYLITE_SQL_AST_IDENTIFIER) {
        rc = copy_identifier_text(value_node, identifier, sizeof(identifier), database);
        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = parse_sql_mode_text(database, identifier, &modes);
        if (rc == MYLITE_OK) {
            rc = append_set_sql_mode_warnings(database, modes);
        }
        if (rc == MYLITE_OK) {
            rc = set_session_sql_mode(database, modes);
        }
        return rc;
    }
    if (value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }

    rc = decode_table_option_string_literal(
        database,
        value_node,
        &decoded,
        NULL,
        (struct table_option_name_policy){
            .identifier_kind = "sql_mode",
            .nul_message = "SET sql_mode does not support NUL bytes",
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = parse_sql_mode_text(database, decoded, &modes);
    if (rc == MYLITE_OK) {
        rc = append_set_sql_mode_warnings(database, modes);
    }
    if (rc == MYLITE_OK) {
        rc = set_session_sql_mode(database, modes);
    }
    free(decoded);
    return rc;
}

static int apply_set_timestamp_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *literal_node = unwrap_parenthesized_expression(value_node);
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        database->session.has_timestamp_override = false;
        database->session.timestamp_override = 0;
        return MYLITE_OK;
    }
    if (literal_node != NULL && literal_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(literal_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            database->session.has_timestamp_override = false;
            database->session.timestamp_override = 0;
            return MYLITE_OK;
        }
        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, "SET timestamp supports only integer values");
            return MYLITE_ERROR;
        }
        literal_node = unwrap_parenthesized_expression(child_at(literal_node, 0U));
    }
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal_node) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(database, "SET timestamp supports only integer values");
        return MYLITE_ERROR;
    }

    rc = parse_unsigned_integer_literal(&literal_node->span, &magnitude);
    if (rc != MYLITE_OK || magnitude > INT32_MAX) {
        char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
        size_t copied = value_node->span.length < sizeof(value_text) ? value_node->span.length
                                                                     : sizeof(value_text) - 1U;

        if (value_node->span.text == NULL) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        memcpy(value_text, value_node->span.text, copied);
        value_text[copied] = '\0';
        set_system_variable_cant_be_set_value_error(database, "timestamp", value_text);
        return MYLITE_ERROR;
    }
    if (magnitude == 0U) {
        database->session.has_timestamp_override = false;
        database->session.timestamp_override = 0;
        return MYLITE_OK;
    }

    database->session.has_timestamp_override = true;
    database->session.timestamp_override = (int64_t)magnitude;
    return MYLITE_OK;
}

static int apply_set_time_zone_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node
) {
    char *decoded = NULL;
    int rc = MYLITE_OK;

    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return set_session_time_zone(database, "SYSTEM");
    }
    rc = copy_set_time_zone_value_text(database, value_node, &decoded);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = set_session_time_zone(database, decoded);
    free(decoded);
    return rc;
}

static int copy_set_time_zone_value_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    char **out_value
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid time_zone value");
        return MYLITE_ERROR;
    }
    *out_value = NULL;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_IDENTIFIER) {
        char value[MYLITE_SESSION_TIME_ZONE_CAPACITY];
        size_t value_length = 0U;

        rc = copy_identifier_text(value_node, value, sizeof(value), database);
        if (rc != MYLITE_OK) {
            return rc;
        }
        value_length = strlen(value);
        *out_value = (char *)malloc(value_length + 1U);
        if (*out_value == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        memcpy(*out_value, value, value_length + 1U);
        return MYLITE_OK;
    }
    if (value_node->kind != MYLITE_SQL_AST_LITERAL) {
        set_time_zone_incorrect_argument_type_error(database);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(value_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        return decode_table_option_string_literal(
            database,
            value_node,
            out_value,
            NULL,
            (struct table_option_name_policy){
                .identifier_kind = "time_zone",
                .nul_message = "SET time_zone does not support NUL bytes",
            }
        );
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        set_system_variable_cant_be_set_value_error(database, "time_zone", "NULL");
        return MYLITE_ERROR;
    }

    set_time_zone_incorrect_argument_type_error(database);
    return MYLITE_ERROR;
}

static int set_session_time_zone(struct mylite_db *database, const char *text) {
    char canonical[MYLITE_SESSION_TIME_ZONE_CAPACITY];
    int offset_minutes = 0;
    int rc = MYLITE_OK;

    if (database == NULL || text == NULL) {
        set_runtime_error(database, "invalid time_zone state");
        return MYLITE_ERROR;
    }

    rc = parse_time_zone_text(database, text, canonical, sizeof(canonical), &offset_minutes);
    if (rc != MYLITE_OK) {
        return rc;
    }

    memcpy(database->session.time_zone, canonical, strlen(canonical) + 1U);
    database->session.time_zone_offset_minutes = offset_minutes;
    database->session.time_zone_is_placeholder = false;
    return MYLITE_OK;
}

static int parse_time_zone_text(
    struct mylite_db *database,
    const char *text,
    char *canonical,
    size_t canonical_size,
    int *out_offset_minutes
) {
    if (text == NULL || canonical == NULL || canonical_size == 0U || out_offset_minutes == NULL) {
        set_runtime_error(database, "invalid time_zone value");
        return MYLITE_ERROR;
    }
    if (sql_mode_token_matches(text, strlen(text), "SYSTEM")) {
        if (sizeof("SYSTEM") > canonical_size) {
            set_runtime_error(database, "time_zone state is too small");
            return MYLITE_ERROR;
        }
        memcpy(canonical, "SYSTEM", sizeof("SYSTEM"));
        *out_offset_minutes = 0;
        return MYLITE_OK;
    }
    if (sql_mode_token_matches(text, strlen(text), "UTC")) {
        if (sizeof("UTC") > canonical_size) {
            set_runtime_error(database, "time_zone state is too small");
            return MYLITE_ERROR;
        }
        memcpy(canonical, "UTC", sizeof("UTC"));
        *out_offset_minutes = 0;
        return MYLITE_OK;
    }
    if (parse_time_zone_offset(text, canonical, canonical_size, out_offset_minutes)) {
        return MYLITE_OK;
    }

    set_unknown_or_incorrect_time_zone_error(database, text);
    return MYLITE_ERROR;
}

static bool parse_time_zone_offset(
    const char *text,
    char *canonical,
    size_t canonical_size,
    int *out_offset_minutes
) {
    size_t offset = 0U;
    int sign = 1;
    int hours = 0;
    int minutes = 0;
    int total_minutes = 0;
    int written = 0;

    if (text == NULL || canonical == NULL || out_offset_minutes == NULL) {
        return false;
    }
    if (text[offset] == '+') {
        sign = 1;
    } else if (text[offset] == '-') {
        sign = -1;
    } else {
        return false;
    }
    ++offset;
    if (text[offset] < '0' || text[offset] > '9') {
        return false;
    }
    hours = text[offset] - '0';
    ++offset;
    if (text[offset] >= '0' && text[offset] <= '9') {
        hours = (hours * decimal_base) + (text[offset] - '0');
        ++offset;
    }
    if (text[offset] != ':') {
        return false;
    }
    ++offset;
    if (text[offset] < '0' || text[offset] > '9') {
        return false;
    }
    minutes = text[offset] - '0';
    ++offset;
    if (text[offset] >= '0' && text[offset] <= '9') {
        minutes = (minutes * decimal_base) + (text[offset] - '0');
        ++offset;
    }
    if (text[offset] != '\0' || minutes > time_zone_max_minute) {
        return false;
    }

    total_minutes = sign * ((hours * time_zone_minutes_per_hour) + minutes);
    if (total_minutes > time_zone_max_positive_offset_minutes ||
        total_minutes < -time_zone_max_negative_offset_minutes) {
        return false;
    }
    if (total_minutes == 0) {
        sign = 1;
    }

    written = snprintf(
        canonical,
        canonical_size,
        "%c%02d:%02d",
        sign < 0 ? '-' : '+',
        abs(total_minutes) / time_zone_minutes_per_hour,
        abs(total_minutes) % time_zone_minutes_per_hour
    );
    if (written < 0 || (size_t)written >= canonical_size) {
        return false;
    }
    *out_offset_minutes = total_minutes;
    return true;
}

static void set_unknown_or_incorrect_time_zone_error(
    struct mylite_db *database,
    const char *value_text
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Unknown or incorrect time zone: '%s'",
        value_text == NULL ? "" : value_text
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_or_incorrect_time_zone,
        "HY000",
        message
    );
}

static void set_time_zone_incorrect_argument_type_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_argument_type,
        "42000",
        "Incorrect argument type to variable 'time_zone'"
    );
}

static int parse_sql_mode_text(struct mylite_db *database, const char *text, uint64_t *out_modes) {
    const char *invalid_token = NULL;
    size_t invalid_token_length = 0U;
    int rc =
        mylite_execution_sql_mode_parse(text, out_modes, &invalid_token, &invalid_token_length);

    if (rc == MYLITE_MISUSE) {
        set_runtime_error(database, "invalid sql_mode value");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_ERROR) {
        set_invalid_sql_mode_error(database, invalid_token, invalid_token_length);
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static bool sql_mode_token_matches(const char *text, size_t length, const char *expected) {
    return mylite_execution_sql_mode_token_matches(text, length, expected);
}

static void set_invalid_sql_mode_error(
    struct mylite_db *database,
    const char *text,
    size_t length
) {
    char value[MYLITE_SESSION_SQL_MODE_TEXT_CAPACITY];
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t copied = length < sizeof(value) ? length : sizeof(value) - 1U;
    int written = 0;

    if (text != NULL && copied > 0U) {
        memcpy(value, text, copied);
    }
    value[copied] = '\0';
    written = snprintf(
        message,
        sizeof(message),
        "Variable 'sql_mode' can't be set to the value of '%s'",
        value
    );
    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_variable_cant_be_set,
        "42000",
        message
    );
}

static void set_system_variable_cant_be_set_value_error(
    struct mylite_db *database,
    const char *variable_name,
    const char *value_text
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Variable '%s' can't be set to the value of '%s'",
        variable_name == NULL ? "unknown" : variable_name,
        value_text == NULL ? "" : value_text
    );

    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_variable_cant_be_set,
        "42000",
        message
    );
}

static int append_set_sql_mode_warnings(struct mylite_db *database, uint64_t modes) {
    const uint64_t zero_group = MYLITE_SESSION_SQL_MODE_NO_ZERO_IN_DATE |
                                MYLITE_SESSION_SQL_MODE_NO_ZERO_DATE |
                                MYLITE_SESSION_SQL_MODE_ERROR_FOR_DIVISION_BY_ZERO;
    bool has_strict = (modes & (MYLITE_SESSION_SQL_MODE_STRICT_TRANS_TABLES |
                                MYLITE_SESSION_SQL_MODE_STRICT_ALL_TABLES)) != 0U;
    bool has_zero_mode = (modes & zero_group) != 0U;
    bool has_zero_group = (modes & zero_group) == zero_group;
    int rc = MYLITE_OK;

    if ((has_strict || has_zero_mode) && !(has_strict && has_zero_group)) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_sql_mode_should_be_used_with_strict,
            "HY000",
            "'NO_ZERO_DATE', 'NO_ZERO_IN_DATE' and 'ERROR_FOR_DIVISION_BY_ZERO' sql modes "
            "should be used with strict mode. They will be merged with strict mode in a future "
            "release."
        );
        if (rc != MYLITE_OK) {
            if (rc == MYLITE_NOMEM) {
                set_nomem_error(database);
            }
            return rc;
        }
    }
    if ((modes & MYLITE_SESSION_SQL_MODE_PAD_CHAR_TO_FULL_LENGTH) != 0U) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_sql_mode_pad_char_deprecated,
            "HY000",
            "Changing sql mode 'PAD_CHAR_TO_FULL_LENGTH' is deprecated. It will be removed in "
            "a future release."
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static int set_session_sql_mode(struct mylite_db *database, uint64_t modes) {
    char text[MYLITE_SESSION_SQL_MODE_TEXT_CAPACITY];
    int rc = rebuild_sql_mode_text(database, modes, text, sizeof(text));

    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.sql_mode = modes;
    memcpy(database->session.sql_mode_text, text, strlen(text) + 1U);
    database->session.sql_mode_is_placeholder = false;
    return MYLITE_OK;
}

static int rebuild_sql_mode_text(
    struct mylite_db *database,
    uint64_t modes,
    char *buffer,
    size_t buffer_size
) {
    int rc = mylite_execution_sql_mode_format(modes, buffer, buffer_size);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static bool session_sql_mode_has(const struct mylite_session_state *session, uint64_t mode) {
    if (session == NULL) {
        return false;
    }
    if ((session->sql_mode & mode) != 0U) {
        return true;
    }
    return false;
}

static unsigned int lexer_modes_for_session_sql_mode(const struct mylite_session_state *session) {
    unsigned int modes = 0U;

    if (session_sql_mode_has(session, MYLITE_SESSION_SQL_MODE_ANSI_QUOTES)) {
        modes |= MYLITE_SQL_MODE_ANSI_QUOTES;
    }
    if (session_sql_mode_has(session, MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES)) {
        modes |= MYLITE_SQL_MODE_NO_BACKSLASH_ESCAPES;
    }
    if (session_sql_mode_has(session, MYLITE_SESSION_SQL_MODE_IGNORE_SPACE)) {
        modes |= MYLITE_SQL_MODE_IGNORE_SPACE;
    }
    if (session_sql_mode_has(session, MYLITE_SESSION_SQL_MODE_PIPES_AS_CONCAT)) {
        modes |= MYLITE_SQL_MODE_PIPES_AS_CONCAT;
    }
    return modes;
}
