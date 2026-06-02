static int decode_sql_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    const char *unsupported_message,
    const char *nul_message,
    char **out_text,
    size_t *out_text_length
) {
    return decode_sql_string_literal_with_policy(
        database,
        literal_node,
        unsupported_message,
        nul_message,
        false,
        out_text,
        out_text_length
    );
}

static int decode_sql_string_literal_with_policy(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    const char *unsupported_message,
    const char *nul_message,
    bool allow_nul,
    char **out_text,
    size_t *out_text_length
) {
    struct mylite_dynamic_string string;
    const char *text = NULL;
    size_t length = 0U;
    char quote = '\0';
    bool allow_backslash = false;
    int rc = MYLITE_OK;

    if (unsupported_message == NULL || nul_message == NULL || out_text == NULL ||
        out_text_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    text = literal_node->span.text;
    length = literal_node->span.length;
    if (text == NULL || length < 2U) {
        set_runtime_error(database, "invalid string literal");
        return MYLITE_ERROR;
    }

    quote = text[0];
    allow_backslash = true;
    if (session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES)) {
        allow_backslash = false;
    }
    mylite_dynamic_string_init(&string);
    for (size_t index = 1U; rc == MYLITE_OK && index + 1U < length; ++index) {
        char byte = text[index];

        if (byte == quote && index + 2U < length && text[index + 1U] == quote) {
            rc = mylite_dynamic_string_append_char(&string, quote);
            ++index;
        } else if (allow_backslash && byte == '\\' && index + 2U < length) {
            ++index;
            rc = append_decoded_sql_string_escape(
                database,
                &string,
                text[index],
                nul_message,
                allow_nul
            );
        } else {
            rc = mylite_dynamic_string_append_char(&string, byte);
        }
    }
    if (rc == MYLITE_OK && string.text == NULL) {
        rc = mylite_dynamic_string_append(&string, "");
    }
    if (rc == MYLITE_OK) {
        *out_text_length = string.length;
        *out_text = mylite_dynamic_string_take(&string);
        if (*out_text == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_decoded_sql_string_escape(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    char escaped_byte,
    const char *nul_message,
    bool allow_nul
) {
    switch (escaped_byte) {
    case '0':
        if (allow_nul) {
            return mylite_dynamic_string_append_char(string, '\0');
        }
        set_unsupported_error(database, nul_message);
        return MYLITE_ERROR;
    case 'n':
        return mylite_dynamic_string_append_char(string, '\n');
    case 'r':
        return mylite_dynamic_string_append_char(string, '\r');
    case 't':
        return mylite_dynamic_string_append_char(string, '\t');
    case 'b':
        return mylite_dynamic_string_append_char(string, '\b');
    case 'Z':
        return mylite_dynamic_string_append_char(string, '\032');
    case '\\':
        return mylite_dynamic_string_append_char(string, '\\');
    case '\'':
        return mylite_dynamic_string_append_char(string, '\'');
    case '"':
        return mylite_dynamic_string_append_char(string, '"');
    case '%':
    case '_': {
        int rc = mylite_dynamic_string_append_char(string, '\\');

        if (rc != MYLITE_OK) {
            return rc;
        }
        return mylite_dynamic_string_append_char(string, escaped_byte);
    }
    default:
        return mylite_dynamic_string_append_char(string, escaped_byte);
    }
}

static int validate_varchar_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct varchar_text_validation validation
) {
    size_t declared_length = 0U;
    size_t character_count = 0U;
    int rc = MYLITE_OK;

    if (validation.text == NULL) {
        set_runtime_error(database, "invalid VARCHAR value");
        return MYLITE_ERROR;
    }
    if (validation.text_length > (size_t)INT_MAX) {
        set_unsupported_error(database, "VARCHAR values are too large for this build");
        return MYLITE_ERROR;
    }
    if (memchr(validation.text, '\0', validation.text_length) != NULL) {
        set_unsupported_error(database, "VARCHAR values do not support NUL bytes");
        return MYLITE_ERROR;
    }

    rc = varchar_length_for_column(database, column, &declared_length);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_utf8_text(validation.text, validation.text_length, &character_count);
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "VARCHAR values must be valid UTF-8");
        return MYLITE_ERROR;
    }
    if (character_count > declared_length) {
        set_data_too_long_error(database, column->name, validation.row_number);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int convert_varchar_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct char_text_conversion *conversion,
    bool allow_nonspace_truncation
) {
    size_t declared_length = 0U;
    size_t character_count = 0U;
    size_t truncated_length = 0U;
    int rc = MYLITE_OK;

    if (conversion == NULL || conversion->text == NULL) {
        set_runtime_error(database, "invalid VARCHAR value");
        return MYLITE_ERROR;
    }
    if (conversion->text_length > (size_t)INT_MAX) {
        set_unsupported_error(database, "VARCHAR values are too large for this build");
        return MYLITE_ERROR;
    }
    if (memchr(conversion->text, '\0', conversion->text_length) != NULL) {
        set_unsupported_error(database, "VARCHAR values do not support NUL bytes");
        return MYLITE_ERROR;
    }
    if (column == NULL || !column_descriptor_is_varchar(column)) {
        set_unsupported_error(database, "statement supports only baseline VARCHAR descriptors");
        return MYLITE_ERROR;
    }

    rc = varchar_length_for_column(database, column, &declared_length);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = utf8_prefix_byte_length_for_character_count(
        (struct utf8_prefix_request){
            .text = conversion->text,
            .text_length = conversion->text_length,
            .character_limit = declared_length,
        },
        &truncated_length,
        &character_count
    );
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "VARCHAR values must be valid UTF-8");
        return MYLITE_ERROR;
    }
    if (character_count <= declared_length) {
        return MYLITE_OK;
    }
    if (text_is_ascii_spaces(
            conversion->text + truncated_length,
            conversion->text_length - truncated_length
        )) {
        rc = append_data_truncated_note(database, column->name, conversion->row_number);
    } else if (allow_nonspace_truncation) {
        rc = append_data_truncated_warning(database, column->name, conversion->row_number);
    } else {
        set_data_too_long_error(database, column->name, conversion->row_number);
        return MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    conversion->text[truncated_length] = '\0';
    conversion->text_length = truncated_length;
    return MYLITE_OK;
}

static int convert_char_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct char_text_conversion *conversion,
    bool allow_nonspace_truncation
) {
    size_t declared_length = 0U;
    size_t character_count = 0U;
    size_t truncated_length = 0U;
    size_t trimmed_length = 0U;
    size_t trailing_space_count = 0U;
    int rc = MYLITE_OK;

    if (conversion == NULL || conversion->text == NULL) {
        set_runtime_error(database, "invalid CHAR value");
        return MYLITE_ERROR;
    }
    if (conversion->text_length > (size_t)INT_MAX) {
        set_unsupported_error(database, "CHAR values are too large for this build");
        return MYLITE_ERROR;
    }
    if (memchr(conversion->text, '\0', conversion->text_length) != NULL) {
        set_unsupported_error(database, "CHAR values do not support NUL bytes");
        return MYLITE_ERROR;
    }
    if (column == NULL || !column_descriptor_is_char(column)) {
        set_unsupported_error(database, "statement supports only baseline CHAR descriptors");
        return MYLITE_ERROR;
    }

    rc = char_length_for_column(database, column, &declared_length);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_utf8_text(conversion->text, conversion->text_length, &character_count);
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "CHAR values must be valid UTF-8");
        return MYLITE_ERROR;
    }

    trimmed_length = conversion->text_length;
    while (trimmed_length > 0U && conversion->text[trimmed_length - 1U] == ' ') {
        --trimmed_length;
        ++trailing_space_count;
    }
    if (trailing_space_count > character_count ||
        character_count - trailing_space_count > declared_length) {
        if (!allow_nonspace_truncation) {
            set_data_too_long_error(database, column->name, conversion->row_number);
            return MYLITE_ERROR;
        }
        rc = append_data_truncated_warning(database, column->name, conversion->row_number);
        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = utf8_prefix_byte_length_for_character_count(
            (struct utf8_prefix_request){
                .text = conversion->text,
                .text_length = conversion->text_length,
                .character_limit = declared_length,
            },
            &truncated_length,
            &character_count
        );
        if (rc != MYLITE_OK) {
            set_unsupported_error(database, "CHAR values must be valid UTF-8");
            return MYLITE_ERROR;
        }
        conversion->text[truncated_length] = '\0';
        conversion->text_length = truncated_length;
        trim_trailing_ascii_spaces(conversion->text, &conversion->text_length);
        return MYLITE_OK;
    }

    conversion->text[trimmed_length] = '\0';
    conversion->text_length = trimmed_length;
    return MYLITE_OK;
}

static int validate_canonical_char_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct varchar_text_validation validation
) {
    size_t declared_length = 0U;
    size_t character_count = 0U;
    size_t trimmed_length = 0U;
    size_t trailing_space_count = 0U;
    int rc = MYLITE_OK;

    if (validation.text == NULL) {
        set_runtime_error(database, "invalid CHAR value");
        return MYLITE_ERROR;
    }
    if (validation.text_length > (size_t)INT_MAX) {
        set_unsupported_error(database, "CHAR values are too large for this build");
        return MYLITE_ERROR;
    }
    if (memchr(validation.text, '\0', validation.text_length) != NULL) {
        set_unsupported_error(database, "CHAR values do not support NUL bytes");
        return MYLITE_ERROR;
    }
    if (column == NULL || !column_descriptor_is_char(column)) {
        set_unsupported_error(database, "statement supports only baseline CHAR descriptors");
        return MYLITE_ERROR;
    }

    rc = char_length_for_column(database, column, &declared_length);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_utf8_text(validation.text, validation.text_length, &character_count);
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "CHAR values must be valid UTF-8");
        return MYLITE_ERROR;
    }

    trimmed_length = validation.text_length;
    while (trimmed_length > 0U && validation.text[trimmed_length - 1U] == ' ') {
        --trimmed_length;
        ++trailing_space_count;
    }
    if (trailing_space_count > character_count ||
        character_count - trailing_space_count > declared_length) {
        set_data_too_long_error(database, column->name, validation.row_number);
        return MYLITE_ERROR;
    }
    if (trimmed_length != validation.text_length) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT into CHAR supports only already-canonical CHAR values"
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_text_family_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct varchar_text_validation validation
) {
    const struct text_family_type_info *info = NULL;
    size_t character_count = 0U;
    int rc = MYLITE_OK;

    if (validation.text == NULL) {
        set_runtime_error(database, "invalid TEXT value");
        return MYLITE_ERROR;
    }
    if (validation.text_length > (size_t)INT_MAX) {
        set_unsupported_error(database, "TEXT values are too large for this build");
        return MYLITE_ERROR;
    }
    if (memchr(validation.text, '\0', validation.text_length) != NULL) {
        set_unsupported_error(database, "TEXT values do not support NUL bytes");
        return MYLITE_ERROR;
    }
    if (column == NULL || !column_descriptor_is_text_family(column)) {
        set_unsupported_error(database, "statement supports only baseline TEXT descriptors");
        return MYLITE_ERROR;
    }

    rc = validate_utf8_text(validation.text, validation.text_length, &character_count);
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "TEXT values must be valid UTF-8");
        return MYLITE_ERROR;
    }
    info = text_family_type_info_for_logical_type(column->logical_type);
    if (info == NULL) {
        set_unsupported_error(database, "statement supports only baseline TEXT descriptors");
        return MYLITE_ERROR;
    }
    if (validation.text_length > info->maximum_length) {
        set_data_too_long_error(database, column->name, validation.row_number);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int convert_text_family_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct char_text_conversion *conversion,
    bool allow_nonspace_truncation,
    bool warn_on_trailing_space_truncation
) {
    const struct text_family_type_info *info = NULL;
    size_t character_count = 0U;
    size_t truncated_length = 0U;
    int rc = MYLITE_OK;

    if (conversion == NULL || conversion->text == NULL) {
        set_runtime_error(database, "invalid TEXT value");
        return MYLITE_ERROR;
    }
    if (conversion->text_length > (size_t)INT_MAX) {
        set_unsupported_error(database, "TEXT values are too large for this build");
        return MYLITE_ERROR;
    }
    if (memchr(conversion->text, '\0', conversion->text_length) != NULL) {
        set_unsupported_error(database, "TEXT values do not support NUL bytes");
        return MYLITE_ERROR;
    }
    if (column == NULL || !column_descriptor_is_text_family(column)) {
        set_unsupported_error(database, "statement supports only baseline TEXT descriptors");
        return MYLITE_ERROR;
    }

    rc = validate_utf8_text(conversion->text, conversion->text_length, &character_count);
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "TEXT values must be valid UTF-8");
        return MYLITE_ERROR;
    }
    (void)character_count;
    info = text_family_type_info_for_logical_type(column->logical_type);
    if (info == NULL) {
        set_unsupported_error(database, "statement supports only baseline TEXT descriptors");
        return MYLITE_ERROR;
    }
    if (conversion->text_length <= info->maximum_length) {
        return MYLITE_OK;
    }
    if (info->maximum_length > SIZE_MAX) {
        set_unsupported_error(database, "TEXT values are too large for this build");
        return MYLITE_ERROR;
    }

    rc = utf8_prefix_byte_length_for_byte_limit(
        (struct utf8_byte_prefix_request){
            .text = conversion->text,
            .text_length = conversion->text_length,
            .byte_limit = (size_t)info->maximum_length,
        },
        &truncated_length
    );
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "TEXT values must be valid UTF-8");
        return MYLITE_ERROR;
    }

    if (text_is_ascii_spaces(
            conversion->text + truncated_length,
            conversion->text_length - truncated_length
        )) {
        if (warn_on_trailing_space_truncation) {
            rc = append_data_truncated_warning(database, column->name, conversion->row_number);
        } else {
            rc = append_data_truncated_note(database, column->name, conversion->row_number);
        }
    } else if (allow_nonspace_truncation) {
        rc = append_data_truncated_warning(database, column->name, conversion->row_number);
    } else {
        set_data_too_long_error(database, column->name, conversion->row_number);
        return MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    conversion->text[truncated_length] = '\0';
    conversion->text_length = truncated_length;
    return MYLITE_OK;
}

static int validate_canonical_decimal_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const char *text,
    size_t text_length,
    size_t row_number
) {
    struct decimal_type_info info = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    bool is_negative = false;
    size_t start = 0U;
    int rc = MYLITE_OK;

    if (text == NULL || column == NULL || !column_descriptor_is_decimal(column)) {
        set_unsupported_error(database, "statement supports only baseline DECIMAL descriptors");
        return MYLITE_ERROR;
    }
    if (text_length > 0U && text[0] == '-') {
        is_negative = true;
        start = 1U;
    }
    rc = decimal_type_info_for_logical_type(column->logical_type, &info);
    if (rc == MYLITE_OK) {
        rc = canonicalize_decimal_text(
            database,
            text + start,
            text_length - start,
            is_negative,
            &info,
            column->name,
            row_number,
            false,
            &value
        );
    }
    if (rc == MYLITE_OK &&
        (value.text_length != text_length || memcmp(value.text, text, text_length) != 0)) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT into DECIMAL supports only already-canonical DECIMAL values"
        );
        rc = MYLITE_ERROR;
    }
    planned_value_deinit(&value);
    return rc;
}

static int validate_utf8_text(const char *text, size_t text_length, size_t *out_character_count) {
    size_t index = 0U;
    size_t character_count = 0U;
    int rc = MYLITE_OK;

    if (text == NULL || out_character_count == NULL) {
        return MYLITE_MISUSE;
    }

    while (index < text_length) {
        size_t width = 0U;

        rc = utf8_sequence_width(text, text_length, index, &width);
        if (rc != MYLITE_OK) {
            return MYLITE_ERROR;
        }

        index += width;
        ++character_count;
    }

    *out_character_count = character_count;
    return MYLITE_OK;
}

static int utf8_prefix_byte_length_for_character_count(
    struct utf8_prefix_request request,
    size_t *out_prefix_length,
    size_t *out_character_count
) {
    size_t index = 0U;
    size_t character_count = 0U;
    size_t prefix_length = 0U;
    int rc = MYLITE_OK;

    if (request.text == NULL || out_prefix_length == NULL || out_character_count == NULL) {
        return MYLITE_MISUSE;
    }

    while (index < request.text_length) {
        size_t width = 0U;

        rc = utf8_sequence_width(request.text, request.text_length, index, &width);
        if (rc != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        if (character_count < request.character_limit) {
            prefix_length = index + width;
        }

        index += width;
        ++character_count;
    }

    if (character_count <= request.character_limit) {
        prefix_length = request.text_length;
    }
    *out_prefix_length = prefix_length;
    *out_character_count = character_count;
    return MYLITE_OK;
}

static int utf8_prefix_byte_length_for_byte_limit(
    struct utf8_byte_prefix_request request,
    size_t *out_prefix_length
) {
    size_t index = 0U;
    size_t prefix_length = 0U;
    int rc = MYLITE_OK;

    if (request.text == NULL || out_prefix_length == NULL) {
        return MYLITE_MISUSE;
    }

    while (index < request.text_length) {
        size_t width = 0U;

        rc = utf8_sequence_width(request.text, request.text_length, index, &width);
        if (rc != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        if (index > request.byte_limit || width > request.byte_limit - index) {
            break;
        }

        prefix_length = index + width;
        index += width;
    }

    *out_prefix_length = prefix_length;
    return MYLITE_OK;
}

static bool text_is_ascii_spaces(const char *text, size_t text_length) {
    if (text == NULL && text_length != 0U) {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        if (text[index] != ' ') {
            return false;
        }
    }
    return true;
}

static int utf8_sequence_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
) {
    unsigned char byte = (unsigned char)text[index];

    if (byte <= utf8_ascii_max) {
        *out_width = 1U;
        return MYLITE_OK;
    }
    if (byte >= utf8_two_byte_min && byte <= utf8_two_byte_max) {
        return utf8_two_byte_width(text, text_length, index, out_width);
    }
    if (byte == utf8_three_byte_e0) {
        return utf8_three_byte_e0_width(text, text_length, index, out_width);
    }
    if ((byte >= utf8_three_byte_min && byte < utf8_three_byte_first_gap_start) ||
        (byte > utf8_three_byte_first_gap_end && byte <= utf8_three_byte_max)) {
        return utf8_three_byte_plain_width(text, text_length, index, out_width);
    }
    if (byte == utf8_three_byte_first_gap_start) {
        return utf8_three_byte_surrogate_gap_width(text, text_length, index, out_width);
    }
    if (byte == utf8_four_byte_f0) {
        return utf8_four_byte_f0_width(text, text_length, index, out_width);
    }
    if (byte >= utf8_four_byte_min && byte <= utf8_four_byte_max_before_last) {
        return utf8_four_byte_plain_width(text, text_length, index, out_width);
    }
    if (byte == utf8_four_byte_last) {
        return utf8_four_byte_last_width(text, text_length, index, out_width);
    }

    return MYLITE_ERROR;
}

static int utf8_two_byte_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
) {
    if (index + 1U >= text_length || !utf8_byte_is_continuation((unsigned char)text[index + 1U])) {
        return MYLITE_ERROR;
    }
    *out_width = 2U;
    return MYLITE_OK;
}

static int utf8_three_byte_e0_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
) {
    if (index + 2U >= text_length ||
        (unsigned char)text[index + 1U] < utf8_three_byte_e0_second_min ||
        (unsigned char)text[index + 1U] > utf8_three_byte_second_max ||
        !utf8_byte_is_continuation((unsigned char)text[index + 2U])) {
        return MYLITE_ERROR;
    }
    *out_width = 3U;
    return MYLITE_OK;
}

static int utf8_three_byte_plain_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
) {
    if (index + 2U >= text_length || !utf8_byte_is_continuation((unsigned char)text[index + 1U]) ||
        !utf8_byte_is_continuation((unsigned char)text[index + 2U])) {
        return MYLITE_ERROR;
    }
    *out_width = 3U;
    return MYLITE_OK;
}

static int utf8_three_byte_surrogate_gap_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
) {
    if (index + 2U >= text_length || (unsigned char)text[index + 1U] < utf8_continuation_tag ||
        (unsigned char)text[index + 1U] > utf8_surrogate_second_max ||
        !utf8_byte_is_continuation((unsigned char)text[index + 2U])) {
        return MYLITE_ERROR;
    }
    *out_width = 3U;
    return MYLITE_OK;
}

static int utf8_four_byte_f0_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
) {
    if (index + 3U >= text_length ||
        (unsigned char)text[index + 1U] < utf8_four_byte_f0_second_min ||
        (unsigned char)text[index + 1U] > utf8_three_byte_second_max ||
        !utf8_byte_is_continuation((unsigned char)text[index + 2U]) ||
        !utf8_byte_is_continuation((unsigned char)text[index + 3U])) {
        return MYLITE_ERROR;
    }
    *out_width = 4U;
    return MYLITE_OK;
}

static int utf8_four_byte_plain_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
) {
    if (index + 3U >= text_length || !utf8_byte_is_continuation((unsigned char)text[index + 1U]) ||
        !utf8_byte_is_continuation((unsigned char)text[index + 2U]) ||
        !utf8_byte_is_continuation((unsigned char)text[index + 3U])) {
        return MYLITE_ERROR;
    }
    *out_width = 4U;
    return MYLITE_OK;
}

static int utf8_four_byte_last_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
) {
    if (index + 3U >= text_length || (unsigned char)text[index + 1U] < utf8_continuation_tag ||
        (unsigned char)text[index + 1U] > utf8_four_byte_last_second_max ||
        !utf8_byte_is_continuation((unsigned char)text[index + 2U]) ||
        !utf8_byte_is_continuation((unsigned char)text[index + 3U])) {
        return MYLITE_ERROR;
    }
    *out_width = 4U;
    return MYLITE_OK;
}

static bool utf8_byte_is_continuation(unsigned char byte) {
    return (byte & utf8_continuation_mask) == utf8_continuation_tag;
}

static int char_length_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t *out_length
) {
    if (column == NULL || out_length == NULL || !column_descriptor_is_char(column)) {
        set_unsupported_error(database, "statement supports only baseline CHAR descriptors");
        return MYLITE_ERROR;
    }

    return parse_char_descriptor_length(
        database,
        column->logical_type,
        "statement supports only baseline CHAR descriptors",
        out_length
    );
}

static int varchar_length_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t *out_length
) {
    if (column == NULL || out_length == NULL || !column_descriptor_is_varchar(column)) {
        set_unsupported_error(database, "statement supports only baseline VARCHAR descriptors");
        return MYLITE_ERROR;
    }

    return parse_varchar_descriptor_length(
        database,
        column->logical_type,
        "statement supports only baseline VARCHAR descriptors",
        out_length
    );
}

static int parse_char_descriptor_length(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    size_t *out_length
) {
    struct mylite_sql_source_span length_span = {0};
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);
    size_t prefix_length = char_descriptor_prefix_length(logical_type);
    size_t syntax_overhead = char_descriptor_syntax_overhead(logical_type);
    uint64_t length = 0U;
    int rc = MYLITE_OK;

    if (out_length == NULL || logical_type == NULL || unsupported_message == NULL ||
        prefix_length == 0U || logical_length <= prefix_length + 1U ||
        logical_type[logical_length - 1U] != ')') {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    length_span = (struct mylite_sql_source_span){
        .text = &logical_type[prefix_length],
        .length = logical_length - syntax_overhead,
    };
    rc = parse_unsigned_integer_literal(&length_span, &length);
    if (rc != MYLITE_OK || length > char_max_length) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    *out_length = (size_t)length;
    return MYLITE_OK;
}

static int parse_varchar_descriptor_length(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    size_t *out_length
) {
    struct mylite_sql_source_span length_span = {0};
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);
    size_t prefix_length = varchar_descriptor_prefix_length(logical_type);
    size_t syntax_overhead = varchar_descriptor_syntax_overhead(logical_type);
    uint64_t length = 0U;
    int rc = MYLITE_OK;

    if (out_length == NULL || logical_type == NULL || unsupported_message == NULL ||
        prefix_length == 0U || logical_length <= prefix_length + 1U ||
        logical_type[logical_length - 1U] != ')') {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    length_span = (struct mylite_sql_source_span){
        .text = &logical_type[prefix_length],
        .length = logical_length - syntax_overhead,
    };
    rc = parse_unsigned_integer_literal(&length_span, &length);
    if (rc != MYLITE_OK || length > varchar_max_length) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    *out_length = (size_t)length;
    return MYLITE_OK;
}
