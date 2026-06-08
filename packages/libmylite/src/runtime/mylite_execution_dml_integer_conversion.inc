static int convert_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;
    bool is_negative = false;
    bool adjust_errors = dml_allows_missing_default_adjustment(database, ignore_errors);
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "INSERT supports only integer, boolean, string, hex, NULL, and DEFAULT values"
            );
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (!boolean_literal_magnitude(literal, &magnitude)) {
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            (mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER &&
             mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_STRING &&
             mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_HEX)) {
            set_unsupported_error(
                database,
                "INSERT supports only integer, boolean, string, hex, NULL, and DEFAULT values"
            );
            return MYLITE_ERROR;
        }
        if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
            return convert_integer_string_literal(
                database,
                literal,
                column,
                row_number,
                ignore_errors,
                out_value
            );
        }
        if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_HEX) {
            return convert_integer_hex_literal(
                database,
                literal,
                column,
                row_number,
                ignore_errors,
                is_negative,
                out_value
            );
        }

        rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
        if (rc != MYLITE_OK) {
            if (!adjust_errors) {
                set_out_of_range_error(database, column->name, row_number);
                return MYLITE_ERROR;
            }
            out_value->is_null = false;
            return clip_integer_for_column(
                database,
                is_negative,
                column,
                row_number,
                &out_value->integer
            );
        }
    }

    out_value->is_null = false;
    rc = convert_integer_for_column_with_policy(
        database,
        magnitude,
        is_negative,
        column,
        row_number,
        adjust_errors,
        &out_value->integer
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return MYLITE_OK;
}

static int convert_integer_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = decode_sql_string_literal(
        database,
        literal,
        "Integer values support only integer, boolean, hex, NULL, DEFAULT, and exact quoted "
        "strings",
        "invalid integer string literal",
        &text,
        &text_length
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = convert_integer_string_text(
        database,
        text,
        text_length,
        column,
        row_number,
        ignore_errors,
        out_value
    );
    free(text);
    return rc;
}

static int convert_integer_string_text(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    bool is_negative = false;
    bool adjust_errors = dml_allows_missing_default_adjustment(database, ignore_errors);
    uint64_t magnitude = 0U;
    enum mylite_execution_dml_numeric_parse_result parse_result =
        MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;

    if (text == NULL || column == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    parse_result =
        mylite_execution_parse_dml_integer_string_text(text, text_length, &magnitude, &is_negative);
    if (parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID) {
        if (!adjust_errors) {
            set_incorrect_integer_value_error(database, text, column->name, row_number);
            return MYLITE_ERROR;
        }
        int rc = append_incorrect_integer_value_warning(database, text, column->name, row_number);
        if (rc == MYLITE_OK) {
            *out_value = (struct planned_value){
                .is_null = false,
                .is_text = false,
                .integer = 0,
            };
        }
        return rc;
    }
    if (parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_OVERFLOW) {
        if (!adjust_errors) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        out_value->is_null = false;
        return clip_integer_for_column(
            database,
            is_negative,
            column,
            row_number,
            &out_value->integer
        );
    }

    bool exceeds_range = false;
    int rc = dml_integer_value_exceeds_column_range(
        database,
        magnitude,
        is_negative,
        column,
        &exceeds_range
    );

    if (rc == MYLITE_OK && parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_TRUNCATED &&
        !exceeds_range) {
        if (!adjust_errors) {
            set_data_truncated_error(database, column->name, row_number);
            rc = MYLITE_ERROR;
        } else {
            rc = append_data_truncated_warning(database, column->name, row_number);
        }
    }
    if (rc == MYLITE_OK) {
        out_value->is_null = false;
        rc = convert_integer_for_column_with_policy(
            database,
            magnitude,
            is_negative,
            column,
            row_number,
            adjust_errors,
            &out_value->integer
        );
    }
    return rc;
}

static int convert_integer_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool is_negative,
    struct planned_value *out_value
) {
    uint64_t magnitude = 0U;
    bool overflow = false;
    int rc = parse_dml_hex_literal_magnitude(database, literal, &magnitude, &overflow);

    if (rc != MYLITE_OK) {
        return rc;
    }

    out_value->is_null = false;
    if (overflow) {
        if (ignore_errors) {
            return clip_integer_for_column(
                database,
                is_negative,
                column,
                row_number,
                &out_value->integer
            );
        }
        set_out_of_range_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }
    return convert_integer_for_column_with_policy(
        database,
        magnitude,
        is_negative,
        column,
        row_number,
        ignore_errors,
        &out_value->integer
    );
}

static int parse_dml_hex_literal_magnitude(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_magnitude,
    bool *out_overflow
) {
    char *bytes = NULL;
    size_t byte_count = 0U;
    size_t first_nonzero_index = 0U;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (out_magnitude == NULL || out_overflow == NULL) {
        return MYLITE_MISUSE;
    }
    *out_magnitude = 0U;
    *out_overflow = false;

    rc = decode_binary_hex_literal(database, literal, &bytes, &byte_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    while (first_nonzero_index < byte_count && (unsigned char)bytes[first_nonzero_index] == 0U) {
        ++first_nonzero_index;
    }
    if (byte_count - first_nonzero_index > sizeof(magnitude)) {
        *out_overflow = true;
    } else {
        for (size_t byte_index = first_nonzero_index; byte_index < byte_count; ++byte_index) {
            magnitude = (magnitude << byte_bit_count) | (uint64_t)(unsigned char)bytes[byte_index];
        }
        *out_magnitude = magnitude;
    }

    free(bytes);
    return MYLITE_OK;
}

static bool ascii_decimal_digit(unsigned char byte) {
    return (byte >= '0' && byte <= '9') != 0;
}

static int copy_dml_numeric_scan_digits(
    struct mylite_db *database,
    const char *text,
    const struct mylite_execution_dml_numeric_token_scan *scan,
    char **out_digits
) {
    char *digits = NULL;
    size_t digit_index = 0U;

    if (text == NULL || scan == NULL || out_digits == NULL) {
        return MYLITE_MISUSE;
    }
    *out_digits = NULL;
    if (scan->digit_count > SIZE_MAX - 1U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    digits = calloc(scan->digit_count + 1U, sizeof(*digits));
    if (digits == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t index = scan->mantissa_start; index < scan->mantissa_end; ++index) {
        if (ascii_decimal_digit((unsigned char)text[index])) {
            if (digit_index >= scan->digit_count) {
                free(digits);
                set_runtime_error(database, "invalid numeric scan digit count");
                return MYLITE_ERROR;
            }
            digits[digit_index] = text[index];
            ++digit_index;
        }
    }
    if (digit_index != scan->digit_count) {
        free(digits);
        set_runtime_error(database, "invalid numeric scan digit count");
        return MYLITE_ERROR;
    }
    *out_digits = digits;
    return MYLITE_OK;
}

static int dml_integer_value_exceeds_column_range(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    bool *out_exceeds_range
) {
    struct integer_column_range range = {0};
    int rc = MYLITE_OK;

    if (out_exceeds_range == NULL) {
        return MYLITE_MISUSE;
    }
    *out_exceeds_range = false;
    rc = integer_range_for_column(
        database,
        column,
        "DML integer string conversion supports only baseline integer columns",
        &range
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_negative) {
        *out_exceeds_range = ((range.negative_abs_max == 0U && magnitude != 0U) ||
                              magnitude > range.negative_abs_max) != 0;
    } else {
        *out_exceeds_range = magnitude > range.positive_max;
    }
    return MYLITE_OK;
}
