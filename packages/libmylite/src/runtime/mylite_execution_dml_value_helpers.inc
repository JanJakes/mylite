static int assign_text_value(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    struct planned_value *out_value
) {
    if (text == NULL || out_value == NULL) {
        free(text);
        return MYLITE_MISUSE;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = false,
        .is_text = true,
        .is_blob = false,
        .integer = 0,
        .text = text,
        .text_length = text_length,
    };
    (void)database;
    return MYLITE_OK;
}

static int make_json_null_value(struct mylite_db *database, struct planned_value *out_value) {
    return copy_text_value(database, "null", out_value);
}

static int make_empty_blob_value(struct mylite_db *database, struct planned_value *out_value) {
    return copy_blob_value(database, NULL, 0U, out_value);
}

static int convert_json_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    const char *diagnostic_column_name,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "JSON values support only string, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "JSON values support only string literals",
        "JSON values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return normalize_json_value(
        database,
        text,
        text_length,
        diagnostic_column_name == NULL ? column->name : diagnostic_column_name,
        out_value
    );
}

static int normalize_json_value(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    struct planned_value *out_value
) {
    char *normalized = NULL;
    size_t normalized_length = 0U;
    struct mylite_json_normalize_result result = {0};
    int rc = mylite_json_normalize(text, text_length, &normalized, &normalized_length, &result);

    free(text);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        if (result.status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
            set_unsupported_error(
                database,
                "JSON values support only objects, arrays, strings, signed integer numbers, "
                "booleans, and null in this baseline"
            );
        } else {
            set_invalid_json_text_error(database, result.position, column_name);
        }
        return MYLITE_ERROR;
    }

    return assign_text_value(database, normalized, normalized_length, out_value);
}

static int assign_blob_value(
    struct mylite_db *database,
    char *bytes,
    size_t byte_count,
    struct planned_value *out_value
) {
    if (bytes == NULL || out_value == NULL) {
        free(bytes);
        return MYLITE_MISUSE;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = false,
        .is_text = false,
        .is_blob = true,
        .integer = 0,
        .text = bytes,
        .text_length = byte_count,
    };
    (void)database;
    return MYLITE_OK;
}

static int copy_decimal_text_value(
    struct mylite_db *database,
    const char *text,
    struct planned_value *out_value
) {
    size_t text_length = text == NULL ? 0U : strlen(text);
    char *copy = NULL;

    if (text == NULL) {
        return MYLITE_MISUSE;
    }
    copy = malloc(text_length + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(copy, text, text_length + 1U);
    return assign_text_value(database, copy, text_length, out_value);
}

static int copy_text_value(
    struct mylite_db *database,
    const char *text,
    struct planned_value *out_value
) {
    size_t text_length = text == NULL ? 0U : strlen(text);
    char *copy = NULL;

    if (text == NULL) {
        return MYLITE_MISUSE;
    }
    copy = malloc(text_length + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(copy, text, text_length + 1U);
    return assign_text_value(database, copy, text_length, out_value);
}

static int copy_blob_value(
    struct mylite_db *database,
    const void *bytes,
    size_t byte_count,
    struct planned_value *out_value
) {
    char *copy = NULL;

    if (bytes == NULL && byte_count != 0U) {
        return MYLITE_MISUSE;
    }
    copy = malloc(byte_count + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (byte_count != 0U) {
        memcpy(copy, bytes, byte_count);
    }
    copy[byte_count] = '\0';
    return assign_blob_value(database, copy, byte_count, out_value);
}

static bool decimal_digits_are_zero(const char *digits, size_t digit_count) {
    for (size_t index = 0U; index < digit_count; ++index) {
        if (digits[index] != '0') {
            return false;
        }
    }
    return true;
}

static int increment_decimal_digits(char *digits, size_t *digit_count, size_t capacity) {
    size_t index = digit_count == NULL ? 0U : *digit_count;

    if (digits == NULL || digit_count == NULL || capacity == 0U) {
        return MYLITE_MISUSE;
    }
    while (index > 0U) {
        --index;
        if (digits[index] < '9') {
            ++digits[index];
            return MYLITE_OK;
        }
        digits[index] = '0';
    }
    if (*digit_count + 1U > capacity) {
        return MYLITE_NOMEM;
    }
    memmove(digits + 1, digits, *digit_count);
    digits[0] = '1';
    ++(*digit_count);
    return MYLITE_OK;
}

static int format_decimal_digits(
    struct mylite_db *database,
    const char *digits,
    size_t digit_count,
    bool is_negative,
    uint64_t scale,
    struct planned_value *out_value
) {
    size_t scale_size = (size_t)scale;
    size_t integer_digit_count = digit_count > scale_size ? digit_count - scale_size : 0U;
    size_t leading_fraction_zeroes = digit_count < scale_size ? scale_size - digit_count : 0U;
    size_t sign_length = 0U;
    size_t output_length = 0U;
    char *text = NULL;
    size_t output_index = 0U;

    if (is_negative) {
        sign_length = 1U;
    }
    output_length = sign_length + (integer_digit_count == 0U ? 1U : integer_digit_count);
    if (scale_size > 0U) {
        output_length += 1U + scale_size;
    }
    text = malloc(output_length + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (is_negative) {
        text[output_index] = '-';
        ++output_index;
    }
    if (integer_digit_count == 0U) {
        text[output_index] = '0';
        ++output_index;
    } else {
        memcpy(text + output_index, digits, integer_digit_count);
        output_index += integer_digit_count;
    }
    if (scale_size > 0U) {
        text[output_index] = '.';
        ++output_index;
        memset(text + output_index, '0', leading_fraction_zeroes);
        output_index += leading_fraction_zeroes;
        if (digit_count > integer_digit_count) {
            size_t fraction_digit_count = digit_count - integer_digit_count;

            memcpy(text + output_index, digits + integer_digit_count, fraction_digit_count);
            output_index += fraction_digit_count;
        }
    }
    text[output_index] = '\0';
    return assign_decimal_text_value(database, text, output_index, out_value);
}
