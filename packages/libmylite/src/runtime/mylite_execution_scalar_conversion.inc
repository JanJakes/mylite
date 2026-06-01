static int cast_binary_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    static const struct scalar_text_conversion_messages messages = {
        .unsupported = "CAST AS BINARY supports only string, integer, boolean, and NULL values",
        .signed_value = "CAST AS BINARY supports only signed integer values",
        .string_unsupported = "CAST AS BINARY supports only string literals",
        .embedded_nul = "CAST AS BINARY does not support embedded NUL bytes",
    };

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CAST_BINARY_EXPRESSION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(database, "CAST AS BINARY supports only bare BINARY casts");
        return MYLITE_ERROR;
    }

    return scalar_text_conversion_input_value(
        database,
        child_at(expression, 0U),
        &messages,
        out_cell
    );
}

static int cast_char_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    static const struct scalar_text_conversion_messages messages = {
        .unsupported = "CAST AS CHAR supports only string, integer, boolean, and NULL values",
        .signed_value = "CAST AS CHAR supports only signed integer values",
        .string_unsupported = "CAST AS CHAR supports only string literals",
        .embedded_nul = "CAST AS CHAR does not support embedded NUL bytes",
    };

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CAST_CHAR_EXPRESSION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(database, "CAST AS CHAR supports only bare CHAR casts");
        return MYLITE_ERROR;
    }

    return scalar_text_conversion_input_value(
        database,
        child_at(expression, 0U),
        &messages,
        out_cell
    );
}

static int cast_signed_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    static const struct scalar_integer_cast_messages messages = {
        .unsupported = "CAST AS SIGNED supports only string, integer, boolean, and NULL values",
        .signed_value = "CAST AS SIGNED supports only signed integer values",
        .embedded_nul = "CAST AS SIGNED does not support embedded NUL bytes",
    };

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(database, "CAST AS SIGNED supports only signed integer casts");
        return MYLITE_ERROR;
    }

    return scalar_integer_cast_input_value(
        database,
        child_at(expression, 0U),
        SCALAR_INTEGER_CAST_SIGNED,
        &messages,
        out_cell
    );
}

static int cast_unsigned_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    static const struct scalar_integer_cast_messages messages = {
        .unsupported = "CAST AS UNSIGNED supports only string, integer, boolean, and NULL values",
        .signed_value = "CAST AS UNSIGNED supports only signed integer values",
        .embedded_nul = "CAST AS UNSIGNED does not support embedded NUL bytes",
    };

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(database, "CAST AS UNSIGNED supports only unsigned integer casts");
        return MYLITE_ERROR;
    }

    return scalar_integer_cast_input_value(
        database,
        child_at(expression, 0U),
        SCALAR_INTEGER_CAST_UNSIGNED,
        &messages,
        out_cell
    );
}

static int convert_binary_type_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    static const struct scalar_text_conversion_messages messages = {
        .unsupported =
            "CONVERT(value, BINARY) supports only string, integer, boolean, and NULL values",
        .signed_value = "CONVERT(value, BINARY) supports only signed integer values",
        .string_unsupported = "CONVERT(value, BINARY) supports only string literals",
        .embedded_nul = "CONVERT(value, BINARY) does not support embedded NUL bytes",
    };

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(
            database,
            "CONVERT(value, BINARY) supports only CONVERT(value, BINARY)"
        );
        return MYLITE_ERROR;
    }

    return scalar_text_conversion_input_value(
        database,
        child_at(expression, 0U),
        &messages,
        out_cell
    );
}

static int convert_char_type_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    static const struct scalar_text_conversion_messages messages = {
        .unsupported =
            "CONVERT(value, CHAR) supports only string, integer, boolean, and NULL values",
        .signed_value = "CONVERT(value, CHAR) supports only signed integer values",
        .string_unsupported = "CONVERT(value, CHAR) supports only string literals",
        .embedded_nul = "CONVERT(value, CHAR) does not support embedded NUL bytes",
    };

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(database, "CONVERT(value, CHAR) supports only CONVERT(value, CHAR)");
        return MYLITE_ERROR;
    }

    return scalar_text_conversion_input_value(
        database,
        child_at(expression, 0U),
        &messages,
        out_cell
    );
}

static int convert_signed_type_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    static const struct scalar_integer_cast_messages messages = {
        .unsupported =
            "CONVERT(value, SIGNED) supports only string, integer, boolean, and NULL values",
        .signed_value = "CONVERT(value, SIGNED) supports only signed integer values",
        .embedded_nul = "CONVERT(value, SIGNED) does not support embedded NUL bytes",
    };

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(
            database,
            "CONVERT(value, SIGNED) supports only CONVERT(value, SIGNED)"
        );
        return MYLITE_ERROR;
    }

    return scalar_integer_cast_input_value(
        database,
        child_at(expression, 0U),
        SCALAR_INTEGER_CAST_SIGNED,
        &messages,
        out_cell
    );
}

static int convert_unsigned_type_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    static const struct scalar_integer_cast_messages messages = {
        .unsupported =
            "CONVERT(value, UNSIGNED) supports only string, integer, boolean, and NULL values",
        .signed_value = "CONVERT(value, UNSIGNED) supports only signed integer values",
        .embedded_nul = "CONVERT(value, UNSIGNED) does not support embedded NUL bytes",
    };

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONVERT_UNSIGNED_TYPE_EXPRESSION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(
            database,
            "CONVERT(value, UNSIGNED) supports only CONVERT(value, UNSIGNED)"
        );
        return MYLITE_ERROR;
    }

    return scalar_integer_cast_input_value(
        database,
        child_at(expression, 0U),
        SCALAR_INTEGER_CAST_UNSIGNED,
        &messages,
        out_cell
    );
}

static int convert_using_binary_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *literal = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(
            database,
            "CONVERT USING BINARY supports only CONVERT(value USING BINARY)"
        );
        return MYLITE_ERROR;
    }

    literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "CONVERT USING BINARY supports only string literals");
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        literal,
        "CONVERT USING BINARY supports only string literals",
        "CONVERT USING BINARY does not support embedded NUL bytes",
        &out_cell->owned_text,
        &text_length
    );
    (void)text_length;
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->owned_text;
    }
    return rc;
}

static int convert_using_charset_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct scalar_convert_charset_info info = {0};
    static const struct scalar_text_conversion_messages messages = {
        .unsupported =
            "CONVERT USING charset supports only string, integer, boolean, and NULL values",
        .signed_value = "CONVERT USING charset supports only signed integer values",
        .string_unsupported = "CONVERT USING charset supports only string literals",
        .embedded_nul = "CONVERT USING charset does not support embedded NUL bytes",
    };
    size_t value_length = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    rc = scalar_convert_charset_info_for_expression(database, expression, &info);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = scalar_text_conversion_input_value(
        database,
        child_at(unwrap_parenthesized_expression(expression), 0U),
        &messages,
        out_cell
    );
    if (rc != MYLITE_OK || !info.ascii_only_value || out_cell->value == NULL) {
        return rc;
    }

    value_length = strlen(out_cell->value);
    for (size_t index = 0U; index < value_length; ++index) {
        if (((const unsigned char *)out_cell->value)[index] > utf8_ascii_max) {
            session_scalar_cell_deinit(out_cell);
            *out_cell = (struct session_scalar_cell){0};
            set_unsupported_error(
                database,
                "CONVERT USING latin1 supports only ASCII scalar values"
            );
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static int collate_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_execution_catalog_scalar_collation *collation_info = NULL;
    const struct mylite_sql_ast_node *value = NULL;
    static const struct scalar_text_conversion_messages messages = {
        .unsupported = "COLLATE supports only scalar values with known character set metadata",
        .signed_value = "COLLATE supports only signed integer values",
        .string_unsupported = "COLLATE supports only string literals",
        .embedded_nul = "COLLATE does not support embedded NUL bytes",
    };
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    rc = scalar_collation_info_for_expression(database, expression, &collation_info);
    if (rc != MYLITE_OK) {
        return rc;
    }
    (void)collation_info;
    value =
        unwrap_parenthesized_expression(child_at(unwrap_parenthesized_expression(expression), 0U));
    if (value == NULL) {
        set_unsupported_error(
            database,
            "COLLATE supports only scalar values with known character set metadata"
        );
        return MYLITE_ERROR;
    }

    switch (value->kind) {
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return scalar_text_conversion_input_value(database, value, &messages, out_cell);
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
        return cast_binary_value(database, value, out_cell);
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
        return convert_binary_type_value(database, value, out_cell);
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        return convert_using_binary_value(database, value, out_cell);
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
        return convert_using_charset_value(database, value, out_cell);
    default:
        break;
    }

    set_unsupported_error(
        database,
        "COLLATE supports only scalar values with known character set metadata"
    );
    return MYLITE_ERROR;
}

static int scalar_convert_charset_info_for_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_convert_charset_info *out_info
) {
    const struct mylite_sql_ast_node *charset = NULL;
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (out_info == NULL) {
        return MYLITE_MISUSE;
    }
    *out_info = (struct scalar_convert_charset_info){0};

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_unsupported_error(
            database,
            "CONVERT USING charset supports only CONVERT(value USING charset)"
        );
        return MYLITE_ERROR;
    }

    charset = child_at(expression, 1U);
    rc = copy_table_option_name_text(
        database,
        charset,
        charset_name,
        sizeof(charset_name),
        (struct table_option_name_policy){
            .identifier_kind = "character set",
            .nul_message = "CONVERT USING charset names do not support NUL bytes",
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    return scalar_convert_charset_info_by_name(database, charset_name, out_info);
}

static int scalar_convert_charset_info_by_name(
    struct mylite_db *database,
    const char *charset_name,
    struct scalar_convert_charset_info *out_info
) {
    if (charset_name == NULL || out_info == NULL) {
        return MYLITE_MISUSE;
    }
    *out_info = (struct scalar_convert_charset_info){0};

    if (text_equals_ascii_case_insensitive(charset_name, "utf8mb4")) {
        *out_info = (struct scalar_convert_charset_info){
            .charset = "utf8mb4",
            .collation = "utf8mb4_0900_ai_ci",
            .ascii_only_value = false,
            .warning = SCALAR_CONVERT_CHARSET_WARNING_NONE,
        };
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(charset_name, "utf8")) {
        *out_info = (struct scalar_convert_charset_info){
            .charset = "utf8mb3",
            .collation = "utf8mb3_general_ci",
            .ascii_only_value = false,
            .warning = SCALAR_CONVERT_CHARSET_WARNING_UTF8_ALIAS,
        };
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(charset_name, "utf8mb3")) {
        *out_info = (struct scalar_convert_charset_info){
            .charset = "utf8mb3",
            .collation = "utf8mb3_general_ci",
            .ascii_only_value = false,
            .warning = SCALAR_CONVERT_CHARSET_WARNING_UTF8MB3_DEPRECATED,
        };
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(charset_name, "latin1")) {
        *out_info = (struct scalar_convert_charset_info){
            .charset = "latin1",
            .collation = "latin1_swedish_ci",
            .ascii_only_value = true,
            .warning = SCALAR_CONVERT_CHARSET_WARNING_NONE,
        };
        return MYLITE_OK;
    }

    set_unknown_character_set_error(database, charset_name);
    return MYLITE_ERROR;
}

static int scalar_text_conversion_input_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct scalar_text_conversion_messages *messages,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *literal = expression;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;
    bool has_sign = false;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (messages == NULL) {
        return MYLITE_MISUSE;
    }
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        has_sign = true;
        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, messages->signed_value);
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (has_sign && literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(database, messages->signed_value);
        return MYLITE_ERROR;
    }
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_STRING: {
        size_t text_length = 0U;
        int rc = decode_sql_string_literal(
            database,
            literal,
            messages->string_unsupported,
            messages->embedded_nul,
            &out_cell->owned_text,
            &text_length
        );
        (void)text_length;
        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->owned_text;
        }
        return rc;
    }
    case MYLITE_SQL_AST_LITERAL_INTEGER: {
        int rc = normalize_decimal_integer_literal(
            database,
            &literal->span,
            is_negative,
            out_cell->literal_text,
            sizeof(out_cell->literal_text)
        );

        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->literal_text;
        }
        return rc;
    }
    case MYLITE_SQL_AST_LITERAL_TRUE:
        out_cell->value = "1";
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_FALSE:
        out_cell->value = "0";
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_NULL:
        out_cell->value = NULL;
        return MYLITE_OK;
    default:
        break;
    }

    set_unsupported_error(database, messages->unsupported);
    return MYLITE_ERROR;
}

static int scalar_integer_cast_input_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum scalar_integer_cast_target target,
    const struct scalar_integer_cast_messages *messages,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *literal = expression;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;
    bool has_sign = false;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (messages == NULL) {
        return MYLITE_MISUSE;
    }
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        has_sign = true;
        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, messages->signed_value);
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (has_sign && literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(database, messages->signed_value);
        return MYLITE_ERROR;
    }
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_STRING: {
        size_t text_length = 0U;
        int rc = decode_sql_string_literal(
            database,
            literal,
            messages->unsupported,
            messages->embedded_nul,
            &out_cell->owned_text,
            &text_length
        );

        (void)text_length;
        if (rc != MYLITE_OK) {
            return rc;
        }
        return scalar_integer_cast_string_value(database, out_cell->owned_text, target, out_cell);
    }
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return scalar_integer_cast_decimal_literal_value(
            database,
            literal,
            is_negative,
            target,
            out_cell
        );
    case MYLITE_SQL_AST_LITERAL_TRUE:
        return format_unsigned_magnitude_value(database, 1U, out_cell);
    case MYLITE_SQL_AST_LITERAL_FALSE:
        return format_unsigned_magnitude_value(database, 0U, out_cell);
    case MYLITE_SQL_AST_LITERAL_NULL:
        out_cell->value = NULL;
        return MYLITE_OK;
    default:
        break;
    }

    set_unsupported_error(database, messages->unsupported);
    return MYLITE_ERROR;
}

static int scalar_integer_cast_string_value(
    struct mylite_db *database,
    const char *text,
    enum scalar_integer_cast_target target,
    struct session_scalar_cell *out_cell
) {
    struct scalar_integer_cast_parse parse = {
        .is_negative = false,
        .saw_digits = false,
        .overflowed = false,
        .has_truncated_integer_warning = false,
        .magnitude = 0U,
    };

    if (text == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    parse_scalar_integer_cast_string(text, &parse);
    if (parse.has_truncated_integer_warning) {
        int rc = stage_truncated_integer_warning(text, out_cell);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    if (target == SCALAR_INTEGER_CAST_SIGNED) {
        if (!parse.is_negative && parse.magnitude > (uint64_t)INT64_MAX && !parse.overflowed) {
            out_cell->staged_signed_complement_warning_count = 1U;
        }
        return format_signed_cast_value(database, parse.is_negative, parse.magnitude, out_cell);
    }

    if (parse.is_negative && parse.saw_digits && !parse.overflowed) {
        out_cell->staged_unsigned_complement_warning_count = 1U;
    }
    return format_unsigned_cast_value(database, parse.is_negative, parse.magnitude, out_cell);
}

static int scalar_integer_cast_decimal_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    enum scalar_integer_cast_target target,
    struct session_scalar_cell *out_cell
) {
    struct scalar_integer_cast_parse parse = {
        .is_negative = false,
        .saw_digits = false,
        .overflowed = false,
        .has_truncated_integer_warning = false,
        .magnitude = 0U,
    };
    uint64_t limit = UINT64_MAX;

    if (literal == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (is_negative) {
        limit = scalar_integer_cast_int64_min_magnitude;
    }
    parse_scalar_integer_cast_decimal_literal(&literal->span, limit, &parse);
    parse.is_negative = is_negative;
    if (parse.overflowed) {
        int rc = stage_cast_truncated_decimal_warning(database, literal, is_negative, out_cell);

        if (rc != MYLITE_OK) {
            return rc;
        }
        if (!is_negative && target == SCALAR_INTEGER_CAST_SIGNED) {
            parse.magnitude = (uint64_t)INT64_MAX;
        }
    }

    if (target == SCALAR_INTEGER_CAST_SIGNED) {
        return format_signed_cast_value(database, parse.is_negative, parse.magnitude, out_cell);
    }
    return format_unsigned_cast_value(database, parse.is_negative, parse.magnitude, out_cell);
}

static void parse_scalar_integer_cast_string(
    const char *text,
    struct scalar_integer_cast_parse *out_parse
) {
    size_t offset = 0U;
    size_t digit_end = 0U;
    uint64_t limit = UINT64_MAX;

    *out_parse = (struct scalar_integer_cast_parse){
        .is_negative = false,
        .saw_digits = false,
        .overflowed = false,
        .has_truncated_integer_warning = false,
        .magnitude = 0U,
    };
    while (scalar_integer_cast_is_ascii_space((unsigned char)text[offset])) {
        ++offset;
    }
    if (text[offset] == '+' || text[offset] == '-') {
        out_parse->is_negative = text[offset] == '-';
        ++offset;
    }
    if (out_parse->is_negative) {
        limit = scalar_integer_cast_int64_min_magnitude;
    }
    parse_scalar_integer_cast_digits(
        (struct scalar_integer_cast_digit_source){
            .text = text,
            .offset = offset,
            .limit = limit,
        },
        out_parse,
        &digit_end
    );
    if (!out_parse->saw_digits) {
        out_parse->has_truncated_integer_warning = true;
        out_parse->magnitude = 0U;
        return;
    }

    offset = digit_end;
    while (scalar_integer_cast_is_ascii_space((unsigned char)text[offset])) {
        ++offset;
    }
    if (text[offset] != '\0') {
        out_parse->has_truncated_integer_warning = true;
    }
}

static void parse_scalar_integer_cast_digits(
    struct scalar_integer_cast_digit_source source,
    struct scalar_integer_cast_parse *inout_parse,
    size_t *out_end_offset
) {
    size_t offset = source.offset;

    while (source.text[offset] >= '0' && source.text[offset] <= '9') {
        uint64_t digit = (uint64_t)(source.text[offset] - '0');

        inout_parse->saw_digits = true;
        if (!inout_parse->overflowed &&
            inout_parse->magnitude > (source.limit - digit) / decimal_base) {
            inout_parse->overflowed = true;
            inout_parse->has_truncated_integer_warning = true;
            inout_parse->magnitude = source.limit;
        } else if (!inout_parse->overflowed) {
            inout_parse->magnitude = (inout_parse->magnitude * decimal_base) + digit;
        }
        ++offset;
    }

    if (out_end_offset != NULL) {
        *out_end_offset = offset;
    }
}

static void parse_scalar_integer_cast_decimal_literal(
    const struct mylite_sql_source_span *span,
    uint64_t limit,
    struct scalar_integer_cast_parse *out_parse
) {
    *out_parse = (struct scalar_integer_cast_parse){
        .is_negative = false,
        .saw_digits = false,
        .overflowed = false,
        .has_truncated_integer_warning = false,
        .magnitude = 0U,
    };
    if (span == NULL || span->text == NULL) {
        return;
    }
    for (size_t index = 0U; index < span->length; ++index) {
        uint64_t digit = 0U;

        if (span->text[index] < '0' || span->text[index] > '9') {
            return;
        }
        digit = (uint64_t)(span->text[index] - '0');
        out_parse->saw_digits = true;
        if (!out_parse->overflowed && out_parse->magnitude > (limit - digit) / decimal_base) {
            out_parse->overflowed = true;
            out_parse->magnitude = limit;
        } else if (!out_parse->overflowed) {
            out_parse->magnitude = (out_parse->magnitude * decimal_base) + digit;
        }
    }
}

static int stage_truncated_integer_warning(
    const char *input_text,
    struct session_scalar_cell *cell
) {
    if (input_text == NULL || cell == NULL) {
        return MYLITE_MISUSE;
    }
    cell->has_staged_truncated_integer_warning = true;
    cell->staged_truncated_integer_text = input_text;
    return MYLITE_OK;
}

static int stage_cast_truncated_decimal_warning(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    struct session_scalar_cell *cell
) {
    size_t output_offset = 0U;
    size_t required_size = 0U;

    if (literal == NULL || literal->span.text == NULL || cell == NULL) {
        return MYLITE_MISUSE;
    }
    required_size = literal->span.length + 1U;
    if (is_negative) {
        ++required_size;
    }
    if (required_size > sizeof(cell->staged_truncated_decimal_text)) {
        set_unsupported_error(
            database,
            "CAST integer conversion supports at most 81 decimal digits in warning text"
        );
        return MYLITE_ERROR;
    }
    if (is_negative) {
        cell->staged_truncated_decimal_text[output_offset] = '-';
        ++output_offset;
    }
    memcpy(
        cell->staged_truncated_decimal_text + output_offset,
        literal->span.text,
        literal->span.length
    );
    output_offset += literal->span.length;
    cell->staged_truncated_decimal_text[output_offset] = '\0';
    cell->has_staged_truncated_decimal_warning = true;
    return MYLITE_OK;
}

static int format_signed_cast_value(
    struct mylite_db *database,
    bool is_negative,
    uint64_t magnitude,
    struct session_scalar_cell *out_cell
) {
    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (is_negative) {
        return format_negative_signed_cast_value(database, magnitude, out_cell);
    }
    if (magnitude <= (uint64_t)INT64_MAX) {
        return format_unsigned_magnitude_value(database, magnitude, out_cell);
    }
    return format_twos_complement_signed_cast_value(database, magnitude, out_cell);
}

static int format_negative_signed_cast_value(
    struct mylite_db *database,
    uint64_t magnitude,
    struct session_scalar_cell *out_cell
) {
    if (magnitude == 0U) {
        return format_unsigned_magnitude_value(database, 0U, out_cell);
    }
    return format_signed_cast_text(database, magnitude, out_cell);
}

static int format_twos_complement_signed_cast_value(
    struct mylite_db *database,
    uint64_t magnitude,
    struct session_scalar_cell *out_cell
) {
    return format_signed_cast_text(database, UINT64_C(0) - magnitude, out_cell);
}

static int format_signed_cast_text(
    struct mylite_db *database,
    uint64_t magnitude,
    struct session_scalar_cell *out_cell
) {
    int written = 0;

    if (magnitude == scalar_integer_cast_int64_min_magnitude) {
        written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "-9223372036854775808"
        );
    } else {
        written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "-%" PRIu64,
            magnitude
        );
    }

    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        set_runtime_error(database, "failed to format CAST signed integer value");
        return MYLITE_ERROR;
    }
    out_cell->value = out_cell->integer_text;
    return MYLITE_OK;
}

static int format_unsigned_cast_value(
    struct mylite_db *database,
    bool is_negative,
    uint64_t magnitude,
    struct session_scalar_cell *out_cell
) {
    uint64_t value = magnitude;

    if (is_negative) {
        value = UINT64_C(0) - magnitude;
    }
    return format_unsigned_magnitude_value(database, value, out_cell);
}

static int format_unsigned_magnitude_value(
    struct mylite_db *database,
    uint64_t value,
    struct session_scalar_cell *out_cell
) {
    int written = 0;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    written = snprintf(out_cell->integer_text, sizeof(out_cell->integer_text), "%" PRIu64, value);
    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        set_runtime_error(database, "failed to format CAST unsigned integer value");
        return MYLITE_ERROR;
    }
    out_cell->value = out_cell->integer_text;
    return MYLITE_OK;
}

static bool scalar_integer_cast_is_ascii_space(unsigned char byte) {
    return (byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' || byte == '\v' ||
            byte == '\f') != 0;
}
