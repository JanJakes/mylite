static int convert_char_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_nonspace_truncation,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "CHAR values support only string, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "CHAR values support only string literals",
        "CHAR values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK) {
        struct char_text_conversion conversion = {
            .text = text,
            .text_length = text_length,
            .row_number = row_number,
        };

        rc = convert_char_text(database, column, &conversion, allow_nonspace_truncation);
        text_length = conversion.text_length;
    }
    if (rc != MYLITE_OK) {
        free(text);
        return rc;
    }

    out_value->is_null = false;
    out_value->is_text = true;
    out_value->integer = 0;
    out_value->text = text;
    out_value->text_length = text_length;
    return MYLITE_OK;
}

static int convert_varchar_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_nonspace_truncation,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "VARCHAR values support only string, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "VARCHAR values support only string literals",
        "VARCHAR values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK) {
        struct char_text_conversion conversion = {
            .text = text,
            .text_length = text_length,
            .row_number = row_number,
        };

        rc = convert_varchar_text(database, column, &conversion, allow_nonspace_truncation);
        text_length = conversion.text_length;
    }
    if (rc != MYLITE_OK) {
        free(text);
        return rc;
    }

    out_value->is_null = false;
    out_value->is_text = true;
    out_value->integer = 0;
    out_value->text = text;
    out_value->text_length = text_length;
    return MYLITE_OK;
}

static int convert_text_family_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_nonspace_truncation,
    bool warn_on_trailing_space_truncation,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "TEXT values support only string, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "TEXT values support only string literals",
        "TEXT values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK) {
        struct char_text_conversion conversion = {
            .text = text,
            .text_length = text_length,
            .row_number = row_number,
        };

        rc = convert_text_family_text(
            database,
            column,
            &conversion,
            allow_nonspace_truncation,
            warn_on_trailing_space_truncation
        );
        if (rc == MYLITE_OK) {
            text_length = conversion.text_length;
        }
    }
    if (rc != MYLITE_OK) {
        free(text);
        return rc;
    }

    out_value->is_null = false;
    out_value->is_text = true;
    out_value->integer = 0;
    out_value->text = text;
    out_value->text_length = text_length;
    return MYLITE_OK;
}

static int convert_binary_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = decode_binary_string_literal(database, value_node, &bytes, &byte_count);

    if (rc == MYLITE_OK) {
        rc = convert_binary_bytes_for_column(
            database,
            column,
            row_number,
            ignore_errors,
            &bytes,
            &byte_count
        );
    }
    if (rc != MYLITE_OK) {
        free(bytes);
        return rc;
    }

    return assign_blob_value(database, bytes, byte_count, out_value);
}

static int decode_binary_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    char **out_bytes,
    size_t *out_byte_count
) {
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "Binary string values support only string, hex, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING) {
        return decode_sql_string_literal_with_policy(
            database,
            value_node,
            "Binary string values support only string and hex literals",
            "invalid binary string literal",
            true,
            out_bytes,
            out_byte_count
        );
    }
    if (mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_HEX) {
        return decode_binary_hex_literal(database, value_node, out_bytes, out_byte_count);
    }

    set_unsupported_error(
        database,
        "Binary string values support only string, hex, NULL, and DEFAULT values"
    );
    return MYLITE_ERROR;
}

static int decode_binary_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_bytes,
    size_t *out_byte_count
) {
    const struct mylite_sql_source_span *span = NULL;

    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal_node) != MYLITE_SQL_AST_LITERAL_HEX) {
        set_unsupported_error(database, "Binary string values support only hex literals");
        return MYLITE_ERROR;
    }

    span = &literal_node->span;
    if (span->text == NULL || span->length < 3U) {
        set_unsupported_error(database, "Binary string values support only valid hex literals");
        return MYLITE_ERROR;
    }
    if (span->text[0] == '0' && (span->text[1] == 'x' || span->text[1] == 'X')) {
        return decode_0x_hex_literal(database, span, out_bytes, out_byte_count);
    }
    return decode_quoted_hex_literal(database, span, out_bytes, out_byte_count);
}

static int decode_quoted_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_bytes,
    size_t *out_byte_count
) {
    size_t hex_count = span->length - 3U;
    size_t byte_count = hex_count / 2U;
    char *bytes = NULL;

    if ((span->text[0] != 'x' && span->text[0] != 'X') || span->text[1] != '\'' ||
        span->text[span->length - 1U] != '\'' || (hex_count % 2U) != 0U) {
        set_unsupported_error(database, "Binary string values support only valid hex literals");
        return MYLITE_ERROR;
    }
    bytes = malloc(byte_count + 1U);
    if (bytes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t byte_index = 0U; byte_index < byte_count; ++byte_index) {
        unsigned char high = 0U;
        unsigned char low = 0U;
        size_t hex_index = 2U + (byte_index * 2U);

        if (hex_nibble_value(span->text[hex_index], &high) != MYLITE_OK ||
            hex_nibble_value(span->text[hex_index + 1U], &low) != MYLITE_OK) {
            free(bytes);
            set_unsupported_error(database, "Binary string values support only valid hex literals");
            return MYLITE_ERROR;
        }
        bytes[byte_index] = (char)((high << 4U) | low);
    }
    bytes[byte_count] = '\0';
    *out_bytes = bytes;
    *out_byte_count = byte_count;
    return MYLITE_OK;
}

static int decode_0x_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_bytes,
    size_t *out_byte_count
) {
    size_t hex_count = span->length - 2U;
    size_t byte_count = (hex_count + 1U) / 2U;
    size_t hex_index = 2U;
    size_t byte_index = 0U;
    char *bytes = NULL;

    bytes = malloc(byte_count + 1U);
    if (bytes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if ((hex_count % 2U) != 0U) {
        unsigned char low = 0U;

        if (hex_nibble_value(span->text[hex_index], &low) != MYLITE_OK) {
            free(bytes);
            set_unsupported_error(database, "Binary string values support only valid hex literals");
            return MYLITE_ERROR;
        }
        bytes[byte_index] = (char)low;
        ++byte_index;
        ++hex_index;
    }
    while (hex_index < span->length) {
        unsigned char high = 0U;
        unsigned char low = 0U;

        if (hex_nibble_value(span->text[hex_index], &high) != MYLITE_OK ||
            hex_nibble_value(span->text[hex_index + 1U], &low) != MYLITE_OK) {
            free(bytes);
            set_unsupported_error(database, "Binary string values support only valid hex literals");
            return MYLITE_ERROR;
        }
        bytes[byte_index] = (char)((high << 4U) | low);
        ++byte_index;
        hex_index += 2U;
    }
    bytes[byte_count] = '\0';
    *out_bytes = bytes;
    *out_byte_count = byte_count;
    return MYLITE_OK;
}

static int hex_nibble_value(char byte, unsigned char *out_value) {
    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (byte >= '0' && byte <= '9') {
        *out_value = (unsigned char)(byte - '0');
        return MYLITE_OK;
    }
    if (byte >= 'a' && byte <= 'f') {
        *out_value = (unsigned char)(byte - 'a' + decimal_base);
        return MYLITE_OK;
    }
    if (byte >= 'A' && byte <= 'F') {
        *out_value = (unsigned char)(byte - 'A' + decimal_base);
        return MYLITE_OK;
    }
    return MYLITE_ERROR;
}

static int convert_binary_bytes_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    char **bytes,
    size_t *byte_count
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info = NULL;

    if (column == NULL || bytes == NULL || *bytes == NULL || byte_count == NULL) {
        return MYLITE_MISUSE;
    }

    info = binary_string_type_info_for_logical_type(column->logical_type, &storage);
    if (info == NULL) {
        set_unsupported_error(database, "statement supports only baseline binary descriptors");
        return MYLITE_ERROR;
    }
    if (*byte_count > info->maximum_length) {
        if (!ignore_errors) {
            set_data_too_long_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        int rc = append_data_truncated_warning(database, column->name, row_number);

        if (rc != MYLITE_OK) {
            return rc;
        }
        *byte_count = (size_t)info->maximum_length;
        (*bytes)[*byte_count] = '\0';
    }
    if (info->fixed_length && *byte_count < info->maximum_length) {
        char *padded = malloc((size_t)info->maximum_length + 1U);

        if (padded == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        memcpy(padded, *bytes, *byte_count);
        memset(padded + *byte_count, 0, (size_t)info->maximum_length - *byte_count);
        padded[info->maximum_length] = '\0';
        free(*bytes);
        *bytes = padded;
        *byte_count = (size_t)info->maximum_length;
    }

    return MYLITE_OK;
}

static int convert_bit_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    uint64_t magnitude = 0U;
    bool is_negative = false;
    bool parse_overflow = false;
    int rc = MYLITE_OK;

    if (value_node == NULL || column == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        (mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING ||
         mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_HEX)) {
        char *bytes = NULL;
        size_t byte_count = 0U;

        if (mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = decode_sql_string_literal_with_policy(
                database,
                value_node,
                "BIT values support only integer, boolean, bit, string, hex, NULL, and DEFAULT "
                "values",
                "invalid BIT string literal",
                true,
                &bytes,
                &byte_count
            );
        } else {
            rc = decode_binary_hex_literal(database, value_node, &bytes, &byte_count);
        }
        if (rc == MYLITE_OK) {
            rc = bit_string_bytes_to_magnitude(bytes, byte_count, &magnitude, &parse_overflow);
        }
        free(bytes);
        if (rc != MYLITE_OK) {
            set_unsupported_error(
                database,
                "BIT values support only integer, boolean, bit, string, hex, NULL, and DEFAULT "
                "values"
            );
            return MYLITE_ERROR;
        }
        if (parse_overflow) {
            if (!ignore_errors) {
                set_data_too_long_error(database, column->name, row_number);
                return MYLITE_ERROR;
            }
            return clip_bit_value_for_column(database, column, row_number, false, out_value);
        }
        return convert_bit_value_magnitude(
            database,
            column,
            magnitude,
            false,
            row_number,
            ignore_errors,
            out_value
        );
    }

    rc = parse_bit_value_magnitude(value_node, &magnitude, &is_negative, &parse_overflow);
    if (rc != MYLITE_OK) {
        set_unsupported_error(
            database,
            "BIT values support only integer, boolean, bit, string, hex, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }
    if (parse_overflow) {
        if (!ignore_errors) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        return clip_bit_value_for_column(database, column, row_number, true, out_value);
    }

    return convert_bit_value_magnitude(
        database,
        column,
        magnitude,
        is_negative,
        row_number,
        ignore_errors,
        out_value
    );
}

static int make_implicit_bit_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    uint64_t width = 0U;
    int rc = bit_width_for_logical_type(column == NULL ? NULL : column->logical_type, &width);

    if (rc != MYLITE_OK) {
        return MYLITE_MISUSE;
    }
    return make_bit_blob_value(
        database,
        &(struct bit_blob_value){.width = width, .magnitude = 0U},
        out_value
    );
}

static int convert_bit_value_magnitude(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    uint64_t magnitude,
    bool is_negative,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    uint64_t width = 0U;
    uint64_t max_value = 0U;
    int rc = bit_width_for_logical_type(column == NULL ? NULL : column->logical_type, &width);

    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "BIT values require baseline BIT descriptors");
        return MYLITE_ERROR;
    }
    max_value = bit_width_max_value(width);
    if ((is_negative && magnitude != 0U) || magnitude > max_value) {
        if (!ignore_errors) {
            set_data_too_long_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        rc = append_data_too_long_warning(database, column->name, row_number);
        if (rc != MYLITE_OK) {
            return rc;
        }
        magnitude = max_value;
    }

    return make_bit_blob_value(
        database,
        &(struct bit_blob_value){.width = width, .magnitude = magnitude},
        out_value
    );
}

static int clip_bit_value_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool include_out_of_range_warning,
    struct planned_value *out_value
) {
    uint64_t width = 0U;
    int rc = MYLITE_OK;

    if (include_out_of_range_warning) {
        rc = append_out_of_range_warning(database, column->name, row_number);
    }
    if (rc == MYLITE_OK) {
        rc = append_data_too_long_warning(database, column->name, row_number);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = bit_width_for_logical_type(column == NULL ? NULL : column->logical_type, &width);
    if (rc != MYLITE_OK) {
        return MYLITE_MISUSE;
    }
    return make_bit_blob_value(
        database,
        &(struct bit_blob_value){.width = width, .magnitude = bit_width_max_value(width)},
        out_value
    );
}

static int parse_bit_value_magnitude(
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_magnitude,
    bool *out_is_negative,
    bool *out_parse_overflow
) {
    const struct mylite_sql_ast_node *literal = value_node;
    uint64_t boolean_magnitude = 0U;

    if (value_node == NULL || out_magnitude == NULL || out_is_negative == NULL ||
        out_parse_overflow == NULL) {
        return MYLITE_MISUSE;
    }
    *out_magnitude = 0U;
    *out_is_negative = false;
    *out_parse_overflow = false;
    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            *out_is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (boolean_literal_magnitude(literal, &boolean_magnitude)) {
        *out_magnitude = boolean_magnitude;
        return MYLITE_OK;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) {
        if (parse_unsigned_integer_literal(&literal->span, out_magnitude) != MYLITE_OK) {
            *out_parse_overflow = true;
        }
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_BIT) {
        return parse_bit_literal_magnitude(&literal->span, out_magnitude, out_parse_overflow);
    }
    return MYLITE_ERROR;
}

static int parse_bit_literal_magnitude(
    const struct mylite_sql_source_span *span,
    uint64_t *out_magnitude,
    bool *out_parse_overflow
) {
    size_t bit_start = 0U;
    size_t bit_end = 0U;
    bool significant = false;
    size_t significant_bits = 0U;
    uint64_t magnitude = 0U;

    if (span == NULL || span->text == NULL || out_magnitude == NULL || out_parse_overflow == NULL ||
        span->length < 2U) {
        return MYLITE_ERROR;
    }
    *out_magnitude = 0U;
    *out_parse_overflow = false;
    if (span->length >= 3U && (span->text[0] == 'b' || span->text[0] == 'B') &&
        span->text[1] == '\'' && span->text[span->length - 1U] == '\'') {
        bit_start = 2U;
        bit_end = span->length - 1U;
    } else if (
        span->length >= 3U && span->text[0] == '0' && (span->text[1] == 'b' || span->text[1] == 'B')
    ) {
        bit_start = 2U;
        bit_end = span->length;
    } else {
        return MYLITE_ERROR;
    }

    for (size_t index = bit_start; index < bit_end; ++index) {
        char byte = span->text[index];

        if (byte != '0' && byte != '1') {
            return MYLITE_ERROR;
        }
        if (!significant && byte == '0') {
            continue;
        }
        significant = true;
        if (significant_bits >= bit_max_length) {
            *out_parse_overflow = true;
            return MYLITE_OK;
        }
        magnitude = (magnitude << 1U) | (byte == '1' ? 1U : 0U);
        ++significant_bits;
    }

    *out_magnitude = magnitude;
    return MYLITE_OK;
}

static int bit_string_bytes_to_magnitude(
    const char *bytes,
    size_t byte_count,
    uint64_t *out_magnitude,
    bool *out_parse_overflow
) {
    uint64_t magnitude = 0U;

    if ((bytes == NULL && byte_count != 0U) || out_magnitude == NULL ||
        out_parse_overflow == NULL) {
        return MYLITE_MISUSE;
    }
    *out_magnitude = 0U;
    *out_parse_overflow = false;
    if (byte_count > sizeof(uint64_t)) {
        *out_parse_overflow = true;
        return MYLITE_OK;
    }
    for (size_t index = 0U; index < byte_count; ++index) {
        magnitude = (magnitude << CHAR_BIT) | (uint64_t)(unsigned char)bytes[index];
    }
    *out_magnitude = magnitude;
    return MYLITE_OK;
}

static int make_bit_blob_value(
    struct mylite_db *database,
    const struct bit_blob_value *bit_value,
    struct planned_value *out_value
) {
    size_t byte_count = bit_value == NULL ? 0U : bit_byte_width(bit_value->width);
    char *bytes = NULL;

    if (bit_value == NULL || bit_value->width == 0U || bit_value->width > bit_max_length ||
        out_value == NULL) {
        return MYLITE_MISUSE;
    }
    bytes = calloc(byte_count + 1U, sizeof(*bytes));
    if (bytes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < byte_count; ++index) {
        size_t shift = (byte_count - index - 1U) * byte_bit_count;

        bytes[index] = (char)((bit_value->magnitude >> shift) & byte_all_bits_mask);
    }
    return assign_blob_value(database, bytes, byte_count, out_value);
}

static int format_bit_default_text(
    struct mylite_db *database,
    const char *column_name,
    uint64_t magnitude,
    char *buffer,
    size_t buffer_size
) {
    size_t position = 0U;
    bool wrote_one = false;

    if (column_name == NULL || buffer == NULL || buffer_size < bit_literal_default_capacity) {
        set_invalid_default_error(database, column_name == NULL ? "" : column_name);
        return MYLITE_ERROR;
    }

    buffer[position++] = 'b';
    buffer[position++] = '\'';
    for (int bit_index = bit_max_length - 1; bit_index >= 0; --bit_index) {
        bool bit_set = ((magnitude >> (unsigned int)bit_index) & 1U) != 0U;

        if (!wrote_one && !bit_set && bit_index != 0) {
            continue;
        }
        if (bit_set) {
            buffer[position++] = '1';
        } else {
            buffer[position++] = '0';
        }
        wrote_one = true;
    }
    buffer[position++] = '\'';
    buffer[position] = '\0';
    return MYLITE_OK;
}
