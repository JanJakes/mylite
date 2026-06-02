static int make_empty_text_value(struct mylite_db *database, struct planned_value *out_value) {
    char *text = NULL;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }

    planned_value_deinit(out_value);
    text = malloc(1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    text[0] = '\0';
    *out_value = (struct planned_value){
        .is_null = false,
        .is_text = true,
        .integer = 0,
        .text = text,
        .text_length = 0U,
    };
    return MYLITE_OK;
}

static int make_implicit_binary_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info =
        column == NULL ? NULL
                       : binary_string_type_info_for_logical_type(column->logical_type, &storage);
    size_t byte_count = 0U;
    char *bytes = NULL;

    if (out_value == NULL || info == NULL) {
        return MYLITE_MISUSE;
    }

    if (info->fixed_length) {
        byte_count = (size_t)info->maximum_length;
    }
    planned_value_deinit(out_value);
    bytes = calloc(byte_count + 1U, sizeof(*bytes));
    if (bytes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    *out_value = (struct planned_value){
        .is_null = false,
        .is_text = false,
        .is_blob = true,
        .integer = 0,
        .text = bytes,
        .text_length = byte_count,
    };
    return MYLITE_OK;
}

static int clip_integer_for_column(
    struct mylite_db *database,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    int64_t *out_value
) {
    const uint64_t bigint_signed_negative_abs_max = 9223372036854775808ULL;
    struct integer_column_range range = {0};
    int rc = integer_range_for_column(
        database,
        column,
        "INSERT supports only baseline integer columns",
        &range
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = append_out_of_range_warning(database, column->name, row_number);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (is_negative) {
        if (range.negative_abs_max == 0U) {
            *out_value = 0;
        } else if (range.negative_abs_max == bigint_signed_negative_abs_max) {
            *out_value = INT64_MIN;
        } else {
            *out_value = -(int64_t)range.negative_abs_max;
        }
        return MYLITE_OK;
    }

    *out_value = (int64_t)range.positive_max;
    return MYLITE_OK;
}

static bool boolean_literal_magnitude(
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_value
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL || out_value == NULL) {
        return false;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_value = 1U;
        return true;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_value = 0U;
        return true;
    }

    return false;
}

static int parse_unsigned_integer_literal(
    const struct mylite_sql_source_span *span,
    uint64_t *out_value
) {
    uint64_t value = 0U;

    if (span == NULL || span->text == NULL || span->length == 0U || out_value == NULL) {
        return MYLITE_ERROR;
    }

    for (size_t index = 0U; index < span->length; ++index) {
        unsigned char byte = (unsigned char)span->text[index];
        uint64_t digit = 0U;

        if (byte < '0' || byte > '9') {
            return MYLITE_ERROR;
        }
        digit = (uint64_t)(byte - '0');
        if (value > (UINT64_MAX - digit) / decimal_base) {
            return MYLITE_ERROR;
        }
        value = (value * decimal_base) + digit;
    }

    *out_value = value;

    return MYLITE_OK;
}

static int convert_integer_for_column(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    int64_t *out_value
) {
    return convert_integer_for_column_with_policy(
        database,
        magnitude,
        is_negative,
        column,
        row_number,
        false,
        out_value
    );
}

static int convert_integer_for_column_with_policy(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    int64_t *out_value
) {
    const uint64_t bigint_signed_negative_abs_max = 9223372036854775808ULL;
    struct integer_column_range range = {0};
    int rc = integer_range_for_column(
        database,
        column,
        "INSERT supports only baseline integer columns",
        &range
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_negative) {
        if ((range.negative_abs_max == 0U && magnitude != 0U) ||
            magnitude > range.negative_abs_max) {
            if (ignore_errors) {
                return clip_integer_for_column(database, true, column, row_number, out_value);
            }
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        if (magnitude == bigint_signed_negative_abs_max) {
            *out_value = INT64_MIN;
        } else {
            *out_value = -(int64_t)magnitude;
        }
        return MYLITE_OK;
    }

    if (magnitude > range.positive_max) {
        if (ignore_errors) {
            return clip_integer_for_column(database, false, column, row_number, out_value);
        }
        set_out_of_range_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }
    *out_value = (int64_t)magnitude;

    return MYLITE_OK;
}
