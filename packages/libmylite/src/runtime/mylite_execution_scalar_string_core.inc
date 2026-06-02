static int string_length_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum planned_string_length_function_kind function_kind = PLANNED_STRING_LENGTH_FUNCTION_NONE;
    struct session_scalar_cell argument_cell = {0};
    char *owned_text = NULL;
    const char *text = NULL;
    size_t text_length = 0U;
    size_t character_count = 0U;
    uint64_t result = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_LENGTH_FUNCTION_NONE
                                       : string_length_function_kind(expression->kind);
    if (function_kind == PLANNED_STRING_LENGTH_FUNCTION_NONE ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(database, "string length functions support exactly one argument");
        return MYLITE_ERROR;
    }

    rc = evaluate_string_length_scalar_argument(
        database,
        child_at(expression, 0U),
        &argument_cell,
        &owned_text,
        &text,
        &text_length,
        &is_null
    );
    if (rc != MYLITE_OK || is_null) {
        free(owned_text);
        session_scalar_cell_deinit(&argument_cell);
        return rc;
    }

    if (function_kind == PLANNED_STRING_LENGTH_FUNCTION_CHARACTER) {
        rc = validate_utf8_text(text, text_length, &character_count);
        if (rc != MYLITE_OK) {
            set_runtime_error(database, "invalid UTF-8 value in string length function");
            free(owned_text);
            session_scalar_cell_deinit(&argument_cell);
            return MYLITE_ERROR;
        }
        result = (uint64_t)character_count;
    } else if (function_kind == PLANNED_STRING_LENGTH_FUNCTION_BIT) {
        if (text_length > UINT64_MAX / byte_bit_count) {
            set_unsupported_error(database, "BIT_LENGTH() result is out of range");
            free(owned_text);
            session_scalar_cell_deinit(&argument_cell);
            return MYLITE_ERROR;
        }
        result = (uint64_t)text_length * byte_bit_count;
    } else {
        result = (uint64_t)text_length;
    }

    rc = format_uint64(database, result, out_cell->integer_text, sizeof(out_cell->integer_text));
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->integer_text;
    }
    free(owned_text);
    session_scalar_cell_deinit(&argument_cell);
    return rc;
}

static int evaluate_string_length_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = unwrap_parenthesized_expression(expression);
    if (!string_length_scalar_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "string length functions support only string, integer, boolean, NULL, "
            "session scalar, and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = decode_sql_string_literal(
                database,
                expression,
                "string length functions support only string literals",
                "string length function literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static int string_length_session_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL) {
        return MYLITE_OK;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
        if (database->session.has_selected_schema) {
            out_cell->value = database->session.selected_schema;
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_USER_FUNCTION:
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
        out_cell->value = database->session.client_user_identity;
        return MYLITE_OK;
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
        out_cell->value = database->session.current_user_identity;
        return MYLITE_OK;
    case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
        out_cell->value = "NONE";
        return MYLITE_OK;
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION: {
        int written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRIu64,
            database->session.connection_id
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            set_runtime_error(database, "failed to format CONNECTION_ID() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_VERSION_FUNCTION:
        out_cell->value = MYLITE_MYSQL_SERVER_VERSION_STRING;
        return MYLITE_OK;
    case MYLITE_SQL_AST_PI_FUNCTION:
        out_cell->value = scalar_pi_text;
        return MYLITE_OK;
    case MYLITE_SQL_AST_RAND_FUNCTION:
    case MYLITE_SQL_AST_RAND_SEED_FUNCTION:
        return rand_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_UUID_FUNCTION:
        return mylite_execution_scalar_uuid_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "UUID");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE:
        return current_timestamp_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "CURRENT_TIMESTAMP");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_SYSDATE_FUNCTION:
        return sysdate_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "SYSDATE");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_CURRENT_DATE_VALUE:
        return current_date_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_CURRENT_TIME_VALUE:
        return current_time_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_UTC_DATE_VALUE:
        return utc_date_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_UTC_TIME_VALUE:
        return utc_time_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE:
        return utc_timestamp_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION: {
        int written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRId64,
            database->session.previous_row_count
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            set_runtime_error(database, "failed to format ROW_COUNT() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION: {
        int written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRIu64,
            database->session.found_rows
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            set_runtime_error(database, "failed to format FOUND_ROWS() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION: {
        int written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRIu64,
            database->session.last_insert_id
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            set_runtime_error(database, "failed to format LAST_INSERT_ID() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        return system_variable_value(database, expression, out_cell);
    default:
        set_unsupported_error(
            database,
            "string length functions support only string, integer, boolean, NULL, "
            "session scalar, and system variable arguments"
        );
        return MYLITE_ERROR;
    }
}

static bool string_length_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION) {
        return false;
    }
    if (is_session_scalar_expression(expression)) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            unwrap_parenthesized_expression(child_at(expression, 0U));

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        return (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
                mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) != 0;
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    return (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
            literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
}

static enum planned_string_length_function_kind string_length_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_OCTET_LENGTH_FUNCTION:
        return PLANNED_STRING_LENGTH_FUNCTION_BYTE;
    case MYLITE_SQL_AST_BIT_LENGTH_FUNCTION:
        return PLANNED_STRING_LENGTH_FUNCTION_BIT;
    case MYLITE_SQL_AST_CHAR_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_CHARACTER_LENGTH_FUNCTION:
        return PLANNED_STRING_LENGTH_FUNCTION_CHARACTER;
    default:
        return PLANNED_STRING_LENGTH_FUNCTION_NONE;
    }
}

static bool is_string_length_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return string_length_function_kind(ast_kind) != PLANNED_STRING_LENGTH_FUNCTION_NONE;
}

static int string_codepoint_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum planned_string_codepoint_function_kind function_kind =
        PLANNED_STRING_CODEPOINT_FUNCTION_NONE;
    struct session_scalar_cell argument_cell = {0};
    char *owned_text = NULL;
    const unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    uint64_t result = 0U;
    bool is_binary = false;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_CODEPOINT_FUNCTION_NONE
                                       : string_codepoint_function_kind(expression->kind);
    if (function_kind == PLANNED_STRING_CODEPOINT_FUNCTION_NONE ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(database, "string codepoint functions support exactly one argument");
        return MYLITE_ERROR;
    }

    rc = evaluate_string_codepoint_scalar_argument(
        database,
        child_at(expression, 0U),
        &argument_cell,
        &owned_text,
        &bytes,
        &byte_count,
        &is_binary,
        &is_null
    );
    if (rc != MYLITE_OK || is_null) {
        free(owned_text);
        session_scalar_cell_deinit(&argument_cell);
        return rc;
    }

    rc = mylite_string_codepoint_value(
        string_codepoint_function_to_value_kind(function_kind),
        bytes,
        byte_count,
        is_binary,
        &result
    );
    if (rc == MYLITE_OK) {
        rc =
            format_uint64(database, result, out_cell->integer_text, sizeof(out_cell->integer_text));
    }
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->integer_text;
    }

    free(owned_text);
    session_scalar_cell_deinit(&argument_cell);
    return rc;
}

static int evaluate_string_codepoint_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    bool *out_is_binary,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_bytes == NULL ||
        out_byte_count == NULL || out_is_binary == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_bytes = NULL;
    *out_byte_count = 0U;
    *out_is_binary = false;
    *out_is_null = false;

    expression = unwrap_parenthesized_expression(expression);
    if (!string_codepoint_scalar_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "string codepoint functions support only string, hex, integer, boolean, NULL, "
            "session scalar, and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = decode_sql_string_literal_with_policy(
                database,
                expression,
                "string codepoint functions support only string literals",
                "string codepoint function literals are invalid",
                true,
                out_owned_text,
                out_byte_count
            );
            if (rc == MYLITE_OK) {
                *out_bytes = (const unsigned char *)*out_owned_text;
            }
            return rc;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_HEX) {
            rc = decode_binary_hex_literal(database, expression, out_owned_text, out_byte_count);
            if (rc == MYLITE_OK) {
                *out_bytes = (const unsigned char *)*out_owned_text;
                *out_is_binary = true;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_bytes = (const unsigned char *)inout_cell->value;
    if (inout_cell->has_value_size) {
        *out_byte_count = inout_cell->value_size;
    } else {
        *out_byte_count = strlen(inout_cell->value);
    }
    return MYLITE_OK;
}

static bool string_codepoint_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION) {
        return false;
    }
    if (is_session_scalar_expression(expression)) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            unwrap_parenthesized_expression(child_at(expression, 0U));

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        return (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
                mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) != 0;
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    return (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_HEX ||
            literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
            literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
}

static enum planned_string_codepoint_function_kind string_codepoint_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_ASCII_FUNCTION:
        return PLANNED_STRING_CODEPOINT_FUNCTION_ASCII;
    case MYLITE_SQL_AST_ORD_FUNCTION:
        return PLANNED_STRING_CODEPOINT_FUNCTION_ORD;
    default:
        return PLANNED_STRING_CODEPOINT_FUNCTION_NONE;
    }
}

static enum mylite_string_codepoint_kind string_codepoint_function_to_value_kind(
    enum planned_string_codepoint_function_kind function_kind
) {
    switch (function_kind) {
    case PLANNED_STRING_CODEPOINT_FUNCTION_ASCII:
        return MYLITE_STRING_CODEPOINT_ASCII;
    case PLANNED_STRING_CODEPOINT_FUNCTION_ORD:
        return MYLITE_STRING_CODEPOINT_ORD;
    case PLANNED_STRING_CODEPOINT_FUNCTION_NONE:
        break;
    }
    return MYLITE_STRING_CODEPOINT_ASCII;
}

static bool is_string_codepoint_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return string_codepoint_function_kind(ast_kind) != PLANNED_STRING_CODEPOINT_FUNCTION_NONE;
}

static int string_case_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum planned_string_case_function_kind function_kind = PLANNED_STRING_CASE_FUNCTION_NONE;
    struct session_scalar_cell argument_cell = {0};
    char *owned_text = NULL;
    const char *text = NULL;
    size_t text_length = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_CASE_FUNCTION_NONE
                                       : string_case_function_kind(expression->kind);
    if (function_kind == PLANNED_STRING_CASE_FUNCTION_NONE ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(database, "string case functions support exactly one argument");
        return MYLITE_ERROR;
    }

    rc = evaluate_string_case_scalar_argument(
        database,
        child_at(expression, 0U),
        &argument_cell,
        &owned_text,
        &text,
        &text_length,
        &is_null
    );
    if (rc != MYLITE_OK || is_null) {
        free(owned_text);
        session_scalar_cell_deinit(&argument_cell);
        return rc;
    }

    rc = mylite_string_case_ascii_value(
        database,
        string_case_function_to_value_kind(function_kind),
        text,
        text_length,
        &out_cell->owned_text
    );
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (rc == MYLITE_OK) {
        out_cell->value = out_cell->owned_text;
    }

    free(owned_text);
    session_scalar_cell_deinit(&argument_cell);
    return rc;
}

static int evaluate_string_case_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = unwrap_parenthesized_expression(expression);
    if (!string_case_scalar_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "string case functions support only string, integer, boolean, NULL, "
            "session scalar, and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = decode_sql_string_literal(
                database,
                expression,
                "string case functions support only string literals",
                "string case function literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static bool string_case_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression) {
    return string_length_scalar_argument_is_admitted(expression);
}

static enum planned_string_case_function_kind string_case_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_LOWER_FUNCTION:
    case MYLITE_SQL_AST_LCASE_FUNCTION:
        return PLANNED_STRING_CASE_FUNCTION_LOWER;
    case MYLITE_SQL_AST_UPPER_FUNCTION:
    case MYLITE_SQL_AST_UCASE_FUNCTION:
        return PLANNED_STRING_CASE_FUNCTION_UPPER;
    default:
        return PLANNED_STRING_CASE_FUNCTION_NONE;
    }
}

static bool is_string_case_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return string_case_function_kind(ast_kind) != PLANNED_STRING_CASE_FUNCTION_NONE;
}

static enum mylite_string_case_kind string_case_function_to_value_kind(
    enum planned_string_case_function_kind function_kind
) {
    if (function_kind == PLANNED_STRING_CASE_FUNCTION_UPPER) {
        return MYLITE_STRING_CASE_UPPER;
    }
    return MYLITE_STRING_CASE_LOWER;
}

static int string_trim_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum planned_string_trim_function_kind function_kind = PLANNED_STRING_TRIM_FUNCTION_NONE;
    struct session_scalar_cell value_cell = {0};
    struct session_scalar_cell remove_cell = {0};
    char *owned_value = NULL;
    char *owned_remove = NULL;
    const char *value = NULL;
    const char *remove_string = " ";
    size_t value_length = 0U;
    size_t remove_string_length = 1U;
    size_t argument_count = 0U;
    bool value_is_null = false;
    bool remove_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_TRIM_FUNCTION_NONE
                                       : string_trim_function_kind(expression->kind);
    argument_count = expression == NULL ? 0U : mylite_sql_ast_node_child_count(expression);
    if (function_kind == PLANNED_STRING_TRIM_FUNCTION_NONE ||
        (argument_count != 1U && argument_count != 2U)) {
        set_unsupported_error(
            database,
            "trim functions support one value and an optional remove string"
        );
        return MYLITE_ERROR;
    }

    rc = evaluate_string_trim_scalar_argument(
        database,
        child_at(expression, 0U),
        &value_cell,
        &owned_value,
        &value,
        &value_length,
        &value_is_null
    );
    if (rc == MYLITE_OK && argument_count == 2U) {
        rc = evaluate_string_trim_scalar_argument(
            database,
            child_at(expression, 1U),
            &remove_cell,
            &owned_remove,
            &remove_string,
            &remove_string_length,
            &remove_is_null
        );
    }
    if (rc != MYLITE_OK || value_is_null || remove_is_null) {
        free(owned_value);
        free(owned_remove);
        session_scalar_cell_deinit(&value_cell);
        session_scalar_cell_deinit(&remove_cell);
        return rc;
    }

    rc = mylite_string_trim_value(
        database,
        string_trim_function_to_value_kind(function_kind),
        value,
        value_length,
        remove_string,
        remove_string_length,
        &out_cell->owned_text
    );
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (rc == MYLITE_OK) {
        out_cell->value = out_cell->owned_text;
    }

    free(owned_value);
    free(owned_remove);
    session_scalar_cell_deinit(&value_cell);
    session_scalar_cell_deinit(&remove_cell);
    return rc;
}

static int evaluate_string_trim_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = unwrap_parenthesized_expression(expression);
    if (!string_trim_scalar_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "trim functions support only string, integer, boolean, NULL, session scalar, and "
            "system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = decode_sql_string_literal(
                database,
                expression,
                "trim functions support only string literals",
                "trim function literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = literal_projection_value(database, expression, inout_cell);
    } else {
        rc = string_length_session_scalar_argument_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static bool string_trim_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression) {
    return string_length_scalar_argument_is_admitted(expression);
}

static enum planned_string_trim_function_kind string_trim_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_LTRIM_FUNCTION:
    case MYLITE_SQL_AST_TRIM_LEADING_FUNCTION:
        return PLANNED_STRING_TRIM_FUNCTION_LEADING;
    case MYLITE_SQL_AST_RTRIM_FUNCTION:
    case MYLITE_SQL_AST_TRIM_TRAILING_FUNCTION:
        return PLANNED_STRING_TRIM_FUNCTION_TRAILING;
    case MYLITE_SQL_AST_TRIM_FUNCTION:
        return PLANNED_STRING_TRIM_FUNCTION_BOTH;
    default:
        return PLANNED_STRING_TRIM_FUNCTION_NONE;
    }
}

static bool is_string_trim_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return string_trim_function_kind(ast_kind) != PLANNED_STRING_TRIM_FUNCTION_NONE;
}

static enum mylite_string_trim_kind string_trim_function_to_value_kind(
    enum planned_string_trim_function_kind function_kind
) {
    switch (function_kind) {
    case PLANNED_STRING_TRIM_FUNCTION_LEADING:
        return MYLITE_STRING_TRIM_LEADING;
    case PLANNED_STRING_TRIM_FUNCTION_TRAILING:
        return MYLITE_STRING_TRIM_TRAILING;
    case PLANNED_STRING_TRIM_FUNCTION_BOTH:
    case PLANNED_STRING_TRIM_FUNCTION_NONE:
        break;
    }
    return MYLITE_STRING_TRIM_BOTH;
}
