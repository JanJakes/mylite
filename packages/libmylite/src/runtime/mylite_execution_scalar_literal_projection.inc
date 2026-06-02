
static int literal_projection_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *literal = expression;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool has_sign = false;
    bool is_negative = false;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "SELECT literal projection supports only integer, boolean, string, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        has_sign = true;
        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "SELECT literal projection supports only signed integer literals"
            );
            return MYLITE_ERROR;
        }
        literal = child_at(expression, 0U);
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "SELECT literal projection supports only integer, boolean, string, and NULL literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (has_sign && literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "SELECT literal projection supports only signed integer literals"
        );
        return MYLITE_ERROR;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_cell->value = NULL;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        out_cell->value = "1";
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_cell->value = "0";
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        int rc = decode_sql_string_literal_with_policy(
            database,
            literal,
            "SELECT literal projection supports only ordinary string literals",
            "SELECT literal projection does not support NUL bytes in string literals",
            true,
            &out_cell->owned_text,
            &out_cell->value_size
        );

        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->owned_text;
            out_cell->has_value_size = true;
        }
        return rc;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "SELECT literal projection supports only integer, boolean, string, and NULL literals"
        );
        return MYLITE_ERROR;
    }

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

static int normalize_decimal_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    bool is_negative,
    char *buffer,
    size_t buffer_size
) {
    size_t first_significant = 0U;
    size_t significant_digit_count = 0U;
    size_t output_offset = 0U;
    size_t required_size = 0U;

    if (span == NULL || span->text == NULL || span->length == 0U || buffer == NULL ||
        buffer_size == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    first_significant = span->length;
    for (size_t index = 0U; index < span->length; ++index) {
        unsigned char byte = (unsigned char)span->text[index];

        if (byte < '0' || byte > '9') {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        if (byte != '0' && first_significant == span->length) {
            first_significant = index;
        }
    }
    if (first_significant == span->length) {
        if (buffer_size < sizeof("0")) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        buffer[0] = '0';
        buffer[1] = '\0';
        return MYLITE_OK;
    }

    significant_digit_count = span->length - first_significant;
    if (significant_digit_count > literal_projection_max_significant_digits) {
        set_unsupported_error(
            database,
            "SELECT literal projection supports at most 81 significant decimal digits"
        );
        return MYLITE_ERROR;
    }
    required_size = significant_digit_count + 1U;
    if (is_negative) {
        ++required_size;
    }
    if (required_size > buffer_size) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (is_negative) {
        buffer[output_offset] = '-';
        ++output_offset;
    }
    memcpy(&buffer[output_offset], &span->text[first_significant], significant_digit_count);
    output_offset += significant_digit_count;
    buffer[output_offset] = '\0';

    return MYLITE_OK;
}
