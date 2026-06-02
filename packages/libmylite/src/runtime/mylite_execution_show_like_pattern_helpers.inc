static int decode_show_like_pattern(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *pattern_node,
    char **out_pattern
) {
    struct mylite_dynamic_string string;
    const char *text = NULL;
    size_t length = 0U;
    char quote = '\0';
    int rc = MYLITE_OK;

    if (out_pattern == NULL) {
        set_runtime_error(database, "invalid SHOW LIKE pattern");
        return MYLITE_ERROR;
    }
    *out_pattern = NULL;
    if (pattern_node == NULL || pattern_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(pattern_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "SHOW LIKE supports only string literal patterns");
        return MYLITE_ERROR;
    }

    text = pattern_node->span.text;
    length = pattern_node->span.length;
    if (text == NULL || length < 2U) {
        set_runtime_error(database, "invalid SHOW LIKE pattern");
        return MYLITE_ERROR;
    }

    quote = text[0];
    mylite_dynamic_string_init(&string);
    for (size_t index = 1U; rc == MYLITE_OK && index + 1U < length; ++index) {
        char byte = text[index];

        if (byte == quote && index + 2U < length && text[index + 1U] == quote) {
            rc = mylite_dynamic_string_append_char(&string, quote);
            ++index;
        } else if (byte == '\\' && index + 2U < length) {
            ++index;
            rc = append_decoded_string_escape(database, &string, text[index]);
        } else {
            rc = mylite_dynamic_string_append_char(&string, byte);
        }
    }
    if (rc == MYLITE_OK && string.text == NULL) {
        rc = mylite_dynamic_string_append(&string, "");
    }
    if (rc == MYLITE_OK) {
        *out_pattern = mylite_dynamic_string_take(&string);
        if (*out_pattern == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_decoded_string_escape(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    char escaped_byte
) {
    switch (escaped_byte) {
    case '0':
        set_unsupported_error(database, "SHOW LIKE does not support NUL bytes in patterns");
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
        if (mylite_dynamic_string_append_char(string, '\\') != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        return mylite_dynamic_string_append_char(string, '%');
    case '_':
        if (mylite_dynamic_string_append_char(string, '\\') != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        return mylite_dynamic_string_append_char(string, '_');
    default:
        return mylite_dynamic_string_append_char(string, escaped_byte);
    }
}

static bool show_like_pattern_matches(
    const char *pattern,
    size_t pattern_length,
    const char *value,
    size_t value_length,
    bool case_sensitive
) {
    return show_like_pattern_matches_with_escape(
        pattern,
        pattern_length,
        value,
        value_length,
        case_sensitive,
        true
    );
}

static bool show_like_pattern_matches_with_escape(
    const char *pattern,
    size_t pattern_length,
    const char *value,
    size_t value_length,
    bool case_sensitive,
    bool backslash_escapes
) {
    const size_t no_retry_pattern = SIZE_MAX;
    size_t pattern_index = 0U;
    size_t value_index = 0U;
    size_t retry_pattern_index = no_retry_pattern;
    size_t retry_value_index = 0U;

    while (value_index < value_length) {
        size_t next_pattern_index = pattern_index;

        if (pattern_index < pattern_length && pattern[pattern_index] == '%') {
            pattern_index = show_like_skip_percent_run(pattern, pattern_length, pattern_index);
            if (pattern_index == pattern_length) {
                return true;
            }
            retry_pattern_index = pattern_index;
            retry_value_index = value_index;
            continue;
        }
        if (show_like_pattern_item_matches(
                (struct show_like_pattern_item_request){
                    .pattern = pattern,
                    .pattern_length = pattern_length,
                    .pattern_index = pattern_index,
                    .value_byte = value[value_index],
                    .case_sensitive = case_sensitive,
                    .backslash_escapes = backslash_escapes,
                },
                &next_pattern_index
            )) {
            pattern_index = next_pattern_index;
            ++value_index;
            continue;
        }
        if (retry_pattern_index == no_retry_pattern || retry_value_index >= value_length) {
            return false;
        }
        ++retry_value_index;
        value_index = retry_value_index;
        pattern_index = retry_pattern_index;
    }

    pattern_index = show_like_skip_percent_run(pattern, pattern_length, pattern_index);

    return pattern_index == pattern_length;
}

static size_t show_like_skip_percent_run(
    const char *pattern,
    size_t pattern_length,
    size_t pattern_index
) {
    while (pattern_index < pattern_length && pattern[pattern_index] == '%') {
        ++pattern_index;
    }
    return pattern_index;
}

static bool show_like_pattern_item_matches(
    struct show_like_pattern_item_request request,
    size_t *out_next_pattern_index
) {
    char pattern_byte = '\0';
    size_t next_pattern_index = request.pattern_index;

    if (request.pattern_index >= request.pattern_length || out_next_pattern_index == NULL) {
        return false;
    }

    pattern_byte = request.pattern[request.pattern_index];
    if (pattern_byte == '_') {
        *out_next_pattern_index = request.pattern_index + 1U;
        return true;
    }
    if (request.backslash_escapes && pattern_byte == '\\' &&
        request.pattern_index + 1U < request.pattern_length) {
        ++next_pattern_index;
        pattern_byte = request.pattern[next_pattern_index];
    }
    ++next_pattern_index;
    if (!show_like_bytes_equal(pattern_byte, request.value_byte, request.case_sensitive)) {
        return false;
    }

    *out_next_pattern_index = next_pattern_index;
    return true;
}

static bool show_like_bytes_equal(char left, char right, bool case_sensitive) {
    if (case_sensitive) {
        return left == right;
    }

    return show_like_ascii_lower(left) == show_like_ascii_lower(right);
}

static char show_like_ascii_lower(char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte - 'A' + 'a');
    }
    return byte;
}
