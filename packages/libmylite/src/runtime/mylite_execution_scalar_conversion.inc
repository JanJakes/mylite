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

    int rc = scalar_text_conversion_input_value(
        database,
        child_at(expression, 0U),
        &messages,
        out_cell
    );
    if (rc == MYLITE_OK) {
        rc = apply_scalar_placeholder_cast(
            database,
            expression,
            SCALAR_PLACEHOLDER_CAST_BINARY,
            false,
            out_cell
        );
    }
    return rc;
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

    int rc = scalar_text_conversion_input_value(
        database,
        child_at(expression, 0U),
        &messages,
        out_cell
    );
    if (rc == MYLITE_OK) {
        rc = apply_scalar_placeholder_cast(
            database,
            expression,
            SCALAR_PLACEHOLDER_CAST_TEXT,
            false,
            out_cell
        );
    }
    return rc;
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

    int rc = scalar_text_conversion_input_value(
        database,
        child_at(expression, 0U),
        &messages,
        out_cell
    );
    if (rc == MYLITE_OK) {
        rc = apply_scalar_placeholder_cast(
            database,
            expression,
            SCALAR_PLACEHOLDER_CAST_BINARY,
            true,
            out_cell
        );
    }
    return rc;
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

    int rc = scalar_text_conversion_input_value(
        database,
        child_at(expression, 0U),
        &messages,
        out_cell
    );
    if (rc == MYLITE_OK) {
        rc = apply_scalar_placeholder_cast(
            database,
            expression,
            SCALAR_PLACEHOLDER_CAST_TEXT,
            true,
            out_cell
        );
    }
    return rc;
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
    const struct mylite_sql_ast_node *value = NULL;
    static const struct scalar_text_conversion_messages messages = {
        .unsupported =
            "CONVERT USING charset supports only string, integer, boolean, and NULL values",
        .signed_value = "CONVERT USING charset supports only signed integer values",
        .string_unsupported = "CONVERT USING charset supports only string literals",
        .embedded_nul = "CONVERT USING charset does not support embedded NUL bytes",
    };
    size_t value_length = 0U;
    bool handled_left_slice = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    rc = scalar_convert_charset_info_for_expression(database, expression, &info);
    if (rc != MYLITE_OK) {
        return rc;
    }
    value = child_at(unwrap_parenthesized_expression(expression), 0U);

    rc = convert_using_charset_left_slice_value(
        database,
        value,
        &info,
        out_cell,
        &handled_left_slice
    );
    if (rc != MYLITE_OK || handled_left_slice) {
        return rc;
    }

    rc = scalar_text_conversion_input_value(
        database,
        value,
        &messages,
        out_cell
    );
    if (rc != MYLITE_OK || !info.ascii_only_value || out_cell->value == NULL) {
        return rc;
    }

    value_length = scalar_cell_text_length(out_cell);
    rc = scalar_convert_charset_enforce_ascii(database, out_cell->value, value_length);
    if (rc != MYLITE_OK) {
        session_scalar_cell_deinit(out_cell);
        *out_cell = (struct session_scalar_cell){0};
    }
    return rc;
}

static int convert_using_charset_left_slice_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct scalar_convert_charset_info *target_info,
    struct session_scalar_cell *out_cell,
    bool *out_handled
) {
    const struct mylite_sql_ast_node *value = NULL;
    const struct mylite_sql_ast_node *length_expression = NULL;
    struct session_scalar_cell source_cell = {0};
    const char *source_charset = NULL;
    size_t source_length = 0U;
    size_t slice_length = 0U;
    int64_t requested_length = 0;
    bool length_is_null = false;
    int rc = MYLITE_OK;

    if (target_info == NULL || out_cell == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LEFT_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        return MYLITE_OK;
    }

    value = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (value == NULL ||
        (value->kind != MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION &&
         value->kind != MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION)) {
        return MYLITE_OK;
    }
    *out_handled = true;

    length_expression = child_at(expression, 1U);
    rc = mylite_execution_string_slice_signed_length_value(
        database,
        length_expression,
        &requested_length,
        &length_is_null
    );
    if (rc != MYLITE_OK || length_is_null) {
        return rc;
    }
    if (requested_length <= 0) {
        return assign_scalar_cell_text(database, out_cell, "", 0U);
    }

    rc = convert_using_charset_source_cell_value(
        database,
        value,
        &source_cell,
        &source_charset
    );
    if (rc != MYLITE_OK || source_cell.value == NULL) {
        session_scalar_cell_deinit(&source_cell);
        return rc;
    }

    source_length = scalar_cell_text_length(&source_cell);
    rc = scalar_convert_charset_left_slice_length(
        database,
        source_charset,
        source_cell.value,
        source_length,
        requested_length,
        &slice_length
    );
    if (rc == MYLITE_OK && strcmp(target_info->charset, "big5") == 0 &&
        strcmp(source_charset, "binary") == 0) {
        slice_length = scalar_convert_big5_complete_prefix_length(source_cell.value, slice_length);
    }
    if (rc == MYLITE_OK && target_info->ascii_only_value) {
        rc = scalar_convert_charset_enforce_ascii(database, source_cell.value, slice_length);
    }
    if (rc == MYLITE_OK) {
        rc = assign_scalar_cell_text(database, out_cell, source_cell.value, slice_length);
    }

    session_scalar_cell_deinit(&source_cell);
    return rc;
}

static int convert_using_charset_source_cell_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    const char **out_charset
) {
    struct scalar_convert_charset_info info = {0};
    int rc = MYLITE_OK;

    if (out_cell == NULL || out_charset == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    *out_charset = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return MYLITE_MISUSE;
    }
    if (expression->kind == MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION) {
        *out_charset = "binary";
        return convert_using_binary_value(database, expression, out_cell);
    }
    if (expression->kind != MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION) {
        return MYLITE_MISUSE;
    }

    rc = scalar_convert_charset_info_for_expression(database, expression, &info);
    if (rc == MYLITE_OK) {
        rc = convert_using_charset_value(database, expression, out_cell);
    }
    if (rc == MYLITE_OK) {
        *out_charset = info.charset;
    }
    return rc;
}

static int scalar_convert_charset_left_slice_length(
    struct mylite_db *database,
    const char *charset,
    const char *text,
    size_t text_length,
    int64_t requested_length,
    size_t *out_slice_length
) {
    if (charset == NULL || text == NULL || out_slice_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_slice_length = 0U;
    if (strcmp(charset, "big5") == 0) {
        *out_slice_length = scalar_convert_big5_left_slice_length(
            text,
            text_length,
            requested_length
        );
        return MYLITE_OK;
    }
    if (scalar_convert_charset_left_uses_utf8_characters(charset)) {
        return scalar_convert_utf8_left_slice_length(
            database,
            text,
            text_length,
            requested_length,
            out_slice_length
        );
    }

    (void)database;
    if ((uint64_t)requested_length >= (uint64_t)text_length) {
        *out_slice_length = text_length;
    } else {
        *out_slice_length = (size_t)requested_length;
    }
    return MYLITE_OK;
}

static int scalar_convert_utf8_left_slice_length(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    int64_t requested_length,
    size_t *out_slice_length
) {
    size_t index = 0U;
    int64_t characters = 0;

    if (text == NULL || out_slice_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_slice_length = 0U;
    while (index < text_length && characters < requested_length) {
        size_t width = 0U;
        int rc = mylite_execution_utf8_sequence_width(text, text_length, index, &width);

        if (rc != MYLITE_OK) {
            set_runtime_error(database, "invalid UTF-8 value in string slice function");
            return MYLITE_ERROR;
        }
        index += width;
        ++characters;
    }
    *out_slice_length = index;
    return MYLITE_OK;
}

static size_t scalar_convert_big5_left_slice_length(
    const char *text,
    size_t text_length,
    int64_t requested_length
) {
    size_t offset = 0U;
    int64_t characters = 0;

    if (text == NULL || requested_length <= 0) {
        return 0U;
    }
    while (offset < text_length && characters < requested_length) {
        if (scalar_convert_big5_byte_is_lead((unsigned char)text[offset]) &&
            offset + 1U < text_length) {
            offset += 2U;
        } else {
            ++offset;
        }
        ++characters;
    }
    return offset;
}

static size_t scalar_convert_big5_complete_prefix_length(const char *text, size_t text_length) {
    size_t offset = 0U;

    if (text == NULL) {
        return 0U;
    }
    while (offset < text_length) {
        if (scalar_convert_big5_byte_is_lead((unsigned char)text[offset])) {
            if (offset + 1U >= text_length) {
                return offset;
            }
            offset += 2U;
        } else {
            ++offset;
        }
    }
    return offset;
}

static bool scalar_convert_big5_byte_is_lead(unsigned char byte) {
    return byte >= scalar_convert_big5_lead_byte_min &&
           byte <= scalar_convert_big5_lead_byte_max;
}

static bool scalar_convert_charset_left_uses_utf8_characters(const char *charset) {
    return charset != NULL &&
           (strcmp(charset, "utf8mb4") == 0 || strcmp(charset, "utf8mb3") == 0 ||
            strcmp(charset, "ujis") == 0) != 0;
}

static int scalar_convert_charset_enforce_ascii(
    struct mylite_db *database,
    const char *text,
    size_t text_length
) {
    if (text == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        if (((const unsigned char *)text)[index] > utf8_ascii_max) {
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

    rc = mylite_execution_scalar_collation_info_for_expression(database, expression, &collation_info);
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
    if (text_equals_ascii_case_insensitive(charset_name, "big5")) {
        *out_info = (struct scalar_convert_charset_info){
            .charset = "big5",
            .collation = "big5_chinese_ci",
            .ascii_only_value = false,
            .warning = SCALAR_CONVERT_CHARSET_WARNING_NONE,
        };
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(charset_name, "cp1251")) {
        *out_info = (struct scalar_convert_charset_info){
            .charset = "cp1251",
            .collation = "cp1251_general_ci",
            .ascii_only_value = false,
            .warning = SCALAR_CONVERT_CHARSET_WARNING_NONE,
        };
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(charset_name, "hebrew")) {
        *out_info = (struct scalar_convert_charset_info){
            .charset = "hebrew",
            .collation = "hebrew_general_ci",
            .ascii_only_value = false,
            .warning = SCALAR_CONVERT_CHARSET_WARNING_NONE,
        };
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(charset_name, "koi8r")) {
        *out_info = (struct scalar_convert_charset_info){
            .charset = "koi8r",
            .collation = "koi8r_general_ci",
            .ascii_only_value = false,
            .warning = SCALAR_CONVERT_CHARSET_WARNING_NONE,
        };
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(charset_name, "tis620")) {
        *out_info = (struct scalar_convert_charset_info){
            .charset = "tis620",
            .collation = "tis620_thai_ci",
            .ascii_only_value = false,
            .warning = SCALAR_CONVERT_CHARSET_WARNING_NONE,
        };
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(charset_name, "ujis")) {
        *out_info = (struct scalar_convert_charset_info){
            .charset = "ujis",
            .collation = "ujis_japanese_ci",
            .ascii_only_value = false,
            .warning = SCALAR_CONVERT_CHARSET_WARNING_NONE,
        };
        return MYLITE_OK;
    }

    set_unknown_character_set_error(database, charset_name);
    return MYLITE_ERROR;
}

static int apply_scalar_placeholder_cast(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum scalar_placeholder_cast_target default_target,
    bool convert_syntax,
    struct session_scalar_cell *inout_cell
) {
    struct scalar_placeholder_cast_info info = {0};
    int rc = parse_scalar_placeholder_cast_info(expression, default_target, convert_syntax, &info);

    if (rc != MYLITE_OK || inout_cell == NULL || inout_cell->value == NULL) {
        return rc;
    }

    switch (info.target) {
    case SCALAR_PLACEHOLDER_CAST_BINARY:
    case SCALAR_PLACEHOLDER_CAST_TEXT:
        return apply_scalar_placeholder_text_length(database, &info, inout_cell);
    case SCALAR_PLACEHOLDER_CAST_DATE:
        return apply_scalar_placeholder_date(database, inout_cell);
    case SCALAR_PLACEHOLDER_CAST_TIME:
        return apply_scalar_placeholder_time(database, inout_cell);
    case SCALAR_PLACEHOLDER_CAST_DATETIME:
        return apply_scalar_placeholder_datetime(database, inout_cell);
    case SCALAR_PLACEHOLDER_CAST_DECIMAL:
        return apply_scalar_placeholder_decimal(database, &info, inout_cell);
    case SCALAR_PLACEHOLDER_CAST_JSON:
        return apply_scalar_placeholder_json(database, inout_cell);
    default:
        break;
    }
    return MYLITE_OK;
}

static int parse_scalar_placeholder_cast_info(
    const struct mylite_sql_ast_node *expression,
    enum scalar_placeholder_cast_target default_target,
    bool convert_syntax,
    struct scalar_placeholder_cast_info *out_info
) {
    struct mylite_sql_source_span span = {0};
    struct mylite_sql_source_span probe = {0};
    int rc = MYLITE_OK;

    if (out_info == NULL) {
        return MYLITE_MISUSE;
    }
    *out_info = (struct scalar_placeholder_cast_info){
        .target = default_target,
        .decimal_scale = decimal_default_scale,
    };

    rc = scalar_placeholder_target_span(expression, convert_syntax, &span);
    if (rc != MYLITE_OK || span.text == NULL || span.length == 0U) {
        return rc;
    }

    probe = span;
    if (scalar_span_consume_keyword(&probe, "BINARY")) {
        out_info->target = SCALAR_PLACEHOLDER_CAST_BINARY;
        out_info->has_length =
            scalar_span_parse_optional_length(&probe, &out_info->length);
        return MYLITE_OK;
    }

    probe = span;
    if (scalar_span_consume_keyword(&probe, "CHAR")) {
        out_info->target = SCALAR_PLACEHOLDER_CAST_TEXT;
        out_info->has_length =
            scalar_span_parse_optional_length(&probe, &out_info->length);
        return MYLITE_OK;
    }

    probe = span;
    if (scalar_span_consume_keyword(&probe, "NCHAR")) {
        out_info->target = SCALAR_PLACEHOLDER_CAST_TEXT;
        out_info->has_length =
            scalar_span_parse_optional_length(&probe, &out_info->length);
        return MYLITE_OK;
    }

    probe = span;
    if (scalar_span_consume_keyword(&probe, "NATIONAL")) {
        if (scalar_span_consume_keyword(&probe, "CHARACTER") ||
            scalar_span_consume_keyword(&probe, "CHAR")) {
            out_info->target = SCALAR_PLACEHOLDER_CAST_TEXT;
            out_info->has_length =
                scalar_span_parse_optional_length(&probe, &out_info->length);
        }
        return MYLITE_OK;
    }

    probe = span;
    if (scalar_span_consume_keyword(&probe, "DATE")) {
        out_info->target = SCALAR_PLACEHOLDER_CAST_DATE;
        return MYLITE_OK;
    }

    probe = span;
    if (scalar_span_consume_keyword(&probe, "TIME")) {
        out_info->target = SCALAR_PLACEHOLDER_CAST_TIME;
        return MYLITE_OK;
    }

    probe = span;
    if (scalar_span_consume_keyword(&probe, "DATETIME") ||
        scalar_span_consume_keyword(&probe, "TIMESTAMP")) {
        out_info->target = SCALAR_PLACEHOLDER_CAST_DATETIME;
        return MYLITE_OK;
    }

    probe = span;
    if (scalar_span_consume_keyword(&probe, "DECIMAL") ||
        scalar_span_consume_keyword(&probe, "DEC") ||
        scalar_span_consume_keyword(&probe, "NUMERIC") ||
        scalar_span_consume_keyword(&probe, "FIXED")) {
        out_info->target = SCALAR_PLACEHOLDER_CAST_DECIMAL;
        out_info->has_decimal_scale =
            scalar_span_parse_decimal_scale(&probe, &out_info->decimal_scale);
        return MYLITE_OK;
    }

    probe = span;
    if (scalar_span_consume_keyword(&probe, "JSON")) {
        out_info->target = SCALAR_PLACEHOLDER_CAST_JSON;
        return MYLITE_OK;
    }
    return MYLITE_OK;
}

static int scalar_placeholder_target_span(
    const struct mylite_sql_ast_node *expression,
    bool convert_syntax,
    struct mylite_sql_source_span *out_span
) {
    const struct mylite_sql_ast_node *value = NULL;
    const char *expression_begin = NULL;
    const char *expression_end = NULL;
    const char *value_end = NULL;
    const char *cursor = NULL;
    const char *target_end = NULL;
    size_t keyword_end = 0U;

    if (out_span == NULL) {
        return MYLITE_MISUSE;
    }
    *out_span = (struct mylite_sql_source_span){0};

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->span.text == NULL ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        return MYLITE_OK;
    }
    value = child_at(expression, 0U);
    if (value == NULL || value->span.text == NULL) {
        return MYLITE_OK;
    }

    expression_begin = expression->span.text;
    expression_end = expression_begin + expression->span.length;
    value_end = value->span.text + value->span.length;
    if (value->span.text < expression_begin || value_end > expression_end) {
        return MYLITE_OK;
    }

    cursor = value_end;
    while (cursor < expression_end && isspace((unsigned char)*cursor)) {
        ++cursor;
    }
    if (convert_syntax) {
        if (cursor >= expression_end || *cursor != ',') {
            return MYLITE_OK;
        }
        ++cursor;
    } else {
        if (!scalar_span_keyword_at(
                expression_begin,
                (size_t)(expression_end - expression_begin),
                (size_t)(cursor - expression_begin),
                "AS",
                &keyword_end
            )) {
            return MYLITE_OK;
        }
        cursor = expression_begin + keyword_end;
    }

    target_end = expression_end;
    while (target_end > cursor && isspace((unsigned char)*(target_end - 1))) {
        --target_end;
    }
    if (target_end > cursor && *(target_end - 1) == ')') {
        --target_end;
    }
    *out_span = (struct mylite_sql_source_span){
        .text = cursor,
        .length = (size_t)(target_end - cursor),
    };
    trim_scalar_span(out_span);
    return MYLITE_OK;
}

static int apply_scalar_placeholder_text_length(
    struct mylite_db *database,
    const struct scalar_placeholder_cast_info *info,
    struct session_scalar_cell *inout_cell
) {
    size_t value_length = scalar_cell_text_length(inout_cell);

    if (info == NULL || inout_cell == NULL || inout_cell->value == NULL) {
        return MYLITE_MISUSE;
    }
    if (!info->has_length || info->length >= value_length) {
        return MYLITE_OK;
    }
    return assign_scalar_cell_text(database, inout_cell, inout_cell->value, info->length);
}

static int apply_scalar_placeholder_date(
    struct mylite_db *database,
    struct session_scalar_cell *inout_cell
) {
    size_t value_length = scalar_cell_text_length(inout_cell);

    if (inout_cell == NULL || inout_cell->value == NULL) {
        return MYLITE_MISUSE;
    }
    if (value_length < date_text_length) {
        return MYLITE_OK;
    }
    return assign_scalar_cell_text(database, inout_cell, inout_cell->value, date_text_length);
}

static int apply_scalar_placeholder_time(
    struct mylite_db *database,
    struct session_scalar_cell *inout_cell
) {
    static const char zero_time[] = "00:00:00";
    size_t value_length = scalar_cell_text_length(inout_cell);
    const char *value = inout_cell == NULL ? NULL : inout_cell->value;

    if (inout_cell == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (value_length >= datetime_text_length &&
        (value[datetime_date_time_separator_offset] == ' ' ||
         value[datetime_date_time_separator_offset] == 'T')) {
        return assign_scalar_cell_text(
            database,
            inout_cell,
            value + datetime_hour_text_offset,
            sizeof(zero_time) - 1U
        );
    }
    if (value_length == date_text_length && value[date_first_separator_offset] == '-' &&
        value[date_second_separator_offset] == '-') {
        return assign_scalar_cell_text(database, inout_cell, zero_time, sizeof(zero_time) - 1U);
    }
    if (value_length >= sizeof(zero_time) - 1U) {
        return assign_scalar_cell_text(database, inout_cell, value, sizeof(zero_time) - 1U);
    }
    return MYLITE_OK;
}

static int apply_scalar_placeholder_datetime(
    struct mylite_db *database,
    struct session_scalar_cell *inout_cell
) {
    char datetime[datetime_text_length + 1U];
    size_t value_length = scalar_cell_text_length(inout_cell);
    const char *value = inout_cell == NULL ? NULL : inout_cell->value;

    if (inout_cell == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (value_length >= datetime_text_length) {
        return assign_scalar_cell_text(database, inout_cell, value, datetime_text_length);
    }
    if (value_length == date_text_length && value[date_first_separator_offset] == '-' &&
        value[date_second_separator_offset] == '-') {
        memcpy(datetime, value, date_text_length);
        memcpy(datetime + date_text_length, " 00:00:00", sizeof(" 00:00:00"));
        return assign_scalar_cell_text(database, inout_cell, datetime, datetime_text_length);
    }
    return MYLITE_OK;
}

static int apply_scalar_placeholder_decimal(
    struct mylite_db *database,
    const struct scalar_placeholder_cast_info *info,
    struct session_scalar_cell *inout_cell
) {
    enum { scalar_placeholder_decimal_text_capacity = 128U };
    char format[sizeof("%.30Lf")];
    char text[scalar_placeholder_decimal_text_capacity];
    char *end = NULL;
    long double value = 0.0L;
    size_t scale = decimal_default_scale;
    int written = 0;

    if (info == NULL || inout_cell == NULL || inout_cell->value == NULL) {
        return MYLITE_MISUSE;
    }
    if (info->has_decimal_scale) {
        scale = info->decimal_scale;
    }
    if (scale > scalar_format_max_decimals) {
        scale = scalar_format_max_decimals;
    }

    errno = 0;
    value = strtold(inout_cell->value, &end);
    if (end == inout_cell->value || errno == ERANGE || !isfinite(value)) {
        value = 0.0L;
    }

    written = snprintf(format, sizeof(format), "%%.%zuLf", scale);
    if (written < 0 || (size_t)written >= sizeof(format)) {
        set_runtime_error(database, "failed to format decimal cast precision");
        return MYLITE_ERROR;
    }
    written = snprintf(text, sizeof(text), format, value);
    if (written < 0 || (size_t)written >= sizeof(text)) {
        set_unsupported_error(database, "DECIMAL cast output is out of range");
        return MYLITE_ERROR;
    }
    return assign_scalar_cell_text(database, inout_cell, text, (size_t)written);
}

static int apply_scalar_placeholder_json(
    struct mylite_db *database,
    struct session_scalar_cell *inout_cell
) {
    char *normalized = NULL;
    size_t normalized_length = 0U;
    struct mylite_json_normalize_result result = {0};
    size_t value_length = scalar_cell_text_length(inout_cell);
    int rc = MYLITE_OK;

    if (inout_cell == NULL || inout_cell->value == NULL) {
        return MYLITE_MISUSE;
    }

    rc = mylite_json_normalize(
        inout_cell->value,
        value_length,
        &normalized,
        &normalized_length,
        &result
    );
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        if (result.status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
            set_unsupported_error(
                database,
                "CAST AS JSON supports only objects, arrays, strings, signed integer numbers, "
                "booleans, and null in this baseline"
            );
        } else {
            set_invalid_json_text_error(database, result.position, "CAST AS JSON");
        }
        return MYLITE_ERROR;
    }
    return assign_scalar_cell_owned_text(database, inout_cell, normalized, normalized_length);
}

static int assign_scalar_cell_text(
    struct mylite_db *database,
    struct session_scalar_cell *inout_cell,
    const char *text,
    size_t text_length
) {
    char *copy = NULL;

    if (inout_cell == NULL || text == NULL) {
        return MYLITE_MISUSE;
    }
    copy = (char *)malloc(text_length + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(copy, text, text_length);
    copy[text_length] = '\0';
    return assign_scalar_cell_owned_text(database, inout_cell, copy, text_length);
}

static int assign_scalar_cell_owned_text(
    struct mylite_db *database,
    struct session_scalar_cell *inout_cell,
    char *text,
    size_t text_length
) {
    if (inout_cell == NULL || text == NULL) {
        free(text);
        return MYLITE_MISUSE;
    }

    (void)database;
    free(inout_cell->owned_text);
    inout_cell->owned_text = text;
    inout_cell->value = inout_cell->owned_text;
    inout_cell->value_size = text_length;
    inout_cell->has_value_size = true;
    return MYLITE_OK;
}

static size_t scalar_cell_text_length(const struct session_scalar_cell *cell) {
    if (cell == NULL || cell->value == NULL) {
        return 0U;
    }
    if (cell->has_value_size) {
        return cell->value_size;
    }
    return strlen(cell->value);
}

static void trim_scalar_span(struct mylite_sql_source_span *span) {
    if (span == NULL || span->text == NULL) {
        return;
    }
    while (span->length > 0U && isspace((unsigned char)span->text[0])) {
        ++span->text;
        --span->length;
    }
    while (span->length > 0U && isspace((unsigned char)span->text[span->length - 1U])) {
        --span->length;
    }
}

static bool scalar_span_consume_keyword(
    struct mylite_sql_source_span *span,
    const char *keyword
) {
    size_t end = 0U;

    if (span == NULL || span->text == NULL || keyword == NULL) {
        return false;
    }
    trim_scalar_span(span);
    if (!scalar_span_keyword_at(span->text, span->length, 0U, keyword, &end)) {
        return false;
    }
    span->text += end;
    span->length -= end;
    trim_scalar_span(span);
    return true;
}

static bool scalar_span_parse_optional_length(
    struct mylite_sql_source_span *span,
    size_t *out_length
) {
    size_t index = 0U;
    size_t value = 0U;

    if (span == NULL || span->text == NULL || out_length == NULL) {
        return false;
    }
    *out_length = 0U;
    trim_scalar_span(span);
    while (index < span->length && isspace((unsigned char)span->text[index])) {
        ++index;
    }
    if (index >= span->length || span->text[index] != '(') {
        return false;
    }
    ++index;
    if (!scalar_span_parse_unsigned(span, &index, &value)) {
        return false;
    }
    while (index < span->length && isspace((unsigned char)span->text[index])) {
        ++index;
    }
    if (index >= span->length || span->text[index] != ')') {
        return false;
    }
    *out_length = value;
    return true;
}

static bool scalar_span_parse_decimal_scale(
    struct mylite_sql_source_span *span,
    size_t *out_scale
) {
    size_t index = 0U;
    size_t precision = 0U;
    size_t scale = decimal_default_scale;

    if (span == NULL || span->text == NULL || out_scale == NULL) {
        return false;
    }
    *out_scale = decimal_default_scale;
    trim_scalar_span(span);
    while (index < span->length && isspace((unsigned char)span->text[index])) {
        ++index;
    }
    if (index >= span->length || span->text[index] != '(') {
        return false;
    }
    ++index;
    if (!scalar_span_parse_unsigned(span, &index, &precision)) {
        return false;
    }
    (void)precision;
    while (index < span->length && isspace((unsigned char)span->text[index])) {
        ++index;
    }
    if (index < span->length && span->text[index] == ',') {
        ++index;
        if (!scalar_span_parse_unsigned(span, &index, &scale)) {
            return false;
        }
    }
    while (index < span->length && isspace((unsigned char)span->text[index])) {
        ++index;
    }
    if (index >= span->length || span->text[index] != ')') {
        return false;
    }
    *out_scale = scale;
    return true;
}

static bool scalar_span_parse_unsigned(
    const struct mylite_sql_source_span *span,
    size_t *inout_index,
    size_t *out_value
) {
    size_t index = 0U;
    size_t value = 0U;
    bool saw_digit = false;

    if (span == NULL || span->text == NULL || inout_index == NULL || out_value == NULL) {
        return false;
    }
    index = *inout_index;
    while (index < span->length && isspace((unsigned char)span->text[index])) {
        ++index;
    }
    while (index < span->length && isdigit((unsigned char)span->text[index])) {
        size_t digit = (size_t)(span->text[index] - '0');

        saw_digit = true;
        if (value <= (SIZE_MAX - digit) / decimal_base) {
            value = (value * decimal_base) + digit;
        } else {
            value = SIZE_MAX;
        }
        ++index;
    }
    if (!saw_digit) {
        return false;
    }
    *inout_index = index;
    *out_value = value;
    return true;
}

static bool scalar_span_keyword_at(
    const char *text,
    size_t text_length,
    size_t index,
    const char *keyword,
    size_t *out_end
) {
    size_t keyword_length = 0U;

    if (out_end != NULL) {
        *out_end = 0U;
    }
    if (text == NULL || keyword == NULL) {
        return false;
    }
    keyword_length = strlen(keyword);
    if (keyword_length == 0U || index > text_length || keyword_length > text_length - index) {
        return false;
    }
    if (index > 0U && scalar_span_is_identifier_byte((unsigned char)text[index - 1U])) {
        return false;
    }
    for (size_t offset = 0U; offset < keyword_length; ++offset) {
        unsigned char actual = (unsigned char)text[index + offset];
        unsigned char expected = (unsigned char)keyword[offset];

        if (toupper(actual) != toupper(expected)) {
            return false;
        }
    }
    if (index + keyword_length < text_length &&
        scalar_span_is_identifier_byte((unsigned char)text[index + keyword_length])) {
        return false;
    }
    if (out_end != NULL) {
        *out_end = index + keyword_length;
    }
    return true;
}

static bool scalar_span_is_identifier_byte(unsigned char byte) {
    return (isalnum(byte) || byte == '_') != 0;
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
    switch (expression->kind) {
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
        return cast_binary_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CAST_CHAR_EXPRESSION:
        return cast_char_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
        return convert_binary_type_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION:
        return convert_char_type_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        return convert_using_binary_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
        return convert_using_charset_value(database, expression, out_cell);
    default:
        break;
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
            out_cell->value_size = text_length;
            out_cell->has_value_size = true;
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
