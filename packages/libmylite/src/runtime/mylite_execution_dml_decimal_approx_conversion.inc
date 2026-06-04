static int convert_decimal_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    static const char true_text[] = "1";
    static const char false_text[] = "0";

    const struct mylite_sql_ast_node *literal = value_node;
    struct decimal_type_info info = {0};
    bool is_negative = false;
    uint64_t boolean_magnitude = 0U;
    int rc = decimal_type_info_for_logical_type(column->logical_type, &info);

    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "DECIMAL values require baseline decimal descriptors");
        return MYLITE_ERROR;
    }
    if (value_node == NULL) {
        set_unsupported_error(
            database,
            "DECIMAL values support only numeric, boolean, hex, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "DECIMAL values support only numeric, boolean, hex, NULL, and DEFAULT values"
            );
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (boolean_literal_magnitude(literal, &boolean_magnitude)) {
        return canonicalize_decimal_text(
            database,
            boolean_magnitude == 0U ? false_text : true_text,
            1U,
            is_negative,
            &info,
            column->name,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        (mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER &&
         mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_DECIMAL &&
         mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_STRING &&
         mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_HEX)) {
        set_unsupported_error(
            database,
            "DECIMAL values support only numeric, boolean, hex, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
        return convert_decimal_string_literal(
            database,
            literal,
            column,
            row_number,
            ignore_errors,
            &info,
            out_value
        );
    }
    if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_HEX) {
        return convert_decimal_hex_literal(
            database,
            literal,
            column,
            row_number,
            ignore_errors,
            is_negative,
            &info,
            out_value
        );
    }

    return canonicalize_decimal_text(
        database,
        literal->span.text,
        literal->span.length,
        is_negative,
        &info,
        column->name,
        row_number,
        ignore_errors,
        out_value
    );
}

static int convert_decimal_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    const struct decimal_type_info *info,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    struct mylite_execution_dml_numeric_token_scan scan = {0};
    bool adjust_errors = dml_allows_missing_default_adjustment(database, ignore_errors);
    enum mylite_execution_dml_numeric_parse_result parse_result =
        MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
    int rc = decode_sql_string_literal(
        database,
        literal,
        "DECIMAL values support only numeric, boolean, hex, NULL, DEFAULT, and quoted numeric "
        "string values",
        "invalid DECIMAL string literal",
        &text,
        &text_length
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    parse_result = mylite_execution_scan_dml_numeric_string_text(text, text_length, &scan);
    if (parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID) {
        if (!adjust_errors) {
            set_incorrect_decimal_value_error(database, text, column->name, row_number);
            rc = MYLITE_ERROR;
        } else {
            rc = append_incorrect_decimal_value_warning(database, text, column->name, row_number);
            if (rc == MYLITE_OK) {
                rc = build_decimal_zero_value(database, info, out_value);
            }
        }
    } else if (parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_TRUNCATED && !adjust_errors) {
        set_incorrect_decimal_value_error(database, text, column->name, row_number);
        rc = MYLITE_ERROR;
    } else {
        rc = convert_decimal_scanned_string_literal(
            database,
            text,
            &scan,
            parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_TRUNCATED,
            column,
            row_number,
            adjust_errors,
            info,
            out_value
        );
    }
    free(text);
    return rc;
}

static int convert_decimal_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool is_negative,
    const struct decimal_type_info *info,
    struct planned_value *out_value
) {
    char text[sizeof("18446744073709551615")];
    uint64_t magnitude = 0U;
    bool overflow = false;
    int rc = parse_dml_hex_literal_magnitude(database, literal, &magnitude, &overflow);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (overflow) {
        return handle_decimal_out_of_range(
            database,
            info,
            column->name,
            row_number,
            is_negative,
            ignore_errors,
            out_value
        );
    }
    snprintf(text, sizeof(text), "%" PRIu64, magnitude);
    return canonicalize_decimal_text(
        database,
        text,
        strlen(text),
        is_negative,
        info,
        column->name,
        row_number,
        ignore_errors,
        out_value
    );
}

static int convert_decimal_scanned_string_literal(
    struct mylite_db *database,
    const char *text,
    const struct mylite_execution_dml_numeric_token_scan *scan,
    bool truncated,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool adjust_errors,
    const struct decimal_type_info *info,
    struct planned_value *out_value
) {
    char *decimal_text = NULL;
    bool is_out_of_range = false;
    bool value_done = false;
    int rc = MYLITE_OK;

    if (text == NULL || scan == NULL || column == NULL || info == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (truncated) {
        rc = append_data_truncated_note(database, column->name, row_number);
    }
    if (rc == MYLITE_OK && !mylite_execution_dml_numeric_scan_has_nonzero_digits(scan)) {
        rc = build_decimal_zero_value(database, info, out_value);
        value_done = rc == MYLITE_OK;
    }
    if (rc == MYLITE_OK && !value_done) {
        rc = build_decimal_text_from_numeric_scan(
            database,
            text,
            scan,
            info,
            &decimal_text,
            &is_out_of_range
        );
        if (rc == MYLITE_OK && is_out_of_range) {
            rc = handle_decimal_out_of_range(
                database,
                info,
                column->name,
                row_number,
                scan->is_negative,
                adjust_errors,
                out_value
            );
        } else if (rc == MYLITE_OK) {
            rc = canonicalize_decimal_text(
                database,
                decimal_text,
                strlen(decimal_text),
                scan->is_negative,
                info,
                column->name,
                row_number,
                adjust_errors,
                out_value
            );
        }
    }
    free(decimal_text);
    return rc;
}

static int build_decimal_text_from_numeric_scan(
    struct mylite_db *database,
    const char *text,
    const struct mylite_execution_dml_numeric_token_scan *scan,
    const struct decimal_type_info *info,
    char **out_text,
    bool *out_is_out_of_range
) {
    int64_t decimal_position = 0;
    int64_t fraction_start_digit = 0;
    size_t output_size = 0U;
    size_t write_index = 0U;
    uint64_t fraction_digit_count = 0U;
    char *digits = NULL;
    char *output = NULL;

    if (text == NULL || scan == NULL || info == NULL || out_text == NULL ||
        out_is_out_of_range == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_is_out_of_range = false;
    if (dml_numeric_scan_exceeds_decimal_range(scan, info)) {
        *out_is_out_of_range = true;
        return MYLITE_OK;
    }

    decimal_position = dml_numeric_scan_decimal_position(scan);
    {
        int rc = copy_dml_numeric_scan_digits(database, text, scan, &digits);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    output_size = dml_numeric_scan_decimal_text_size(scan, info);
    output = calloc(output_size + 1U, sizeof(*output));
    if (output == NULL) {
        free(digits);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    if (decimal_position <= (int64_t)scan->first_nonzero_digit_index) {
        output[write_index++] = '0';
    } else {
        for (int64_t digit_index = (int64_t)scan->first_nonzero_digit_index;
             digit_index < decimal_position;
             ++digit_index) {
            output[write_index++] = dml_numeric_scan_virtual_digit(digits, scan, digit_index);
        }
    }

    fraction_digit_count = info->scale + 1U;
    if (fraction_digit_count > 0U) {
        output[write_index++] = '.';
    }
    fraction_start_digit = decimal_position;
    for (uint64_t index = 0U; index < fraction_digit_count; ++index) {
        int64_t digit_index = fraction_start_digit + (int64_t)index;
        char digit = dml_numeric_scan_virtual_digit(digits, scan, digit_index);

        if (index == info->scale && digit == '0' &&
            dml_numeric_scan_has_nonzero_virtual_digit_at_or_after(scan, digit_index + 1)) {
            digit = '1';
        }
        output[write_index++] = digit;
    }
    output[write_index] = '\0';
    free(digits);
    *out_text = output;
    return MYLITE_OK;
}

static int64_t dml_numeric_scan_decimal_position(
    const struct mylite_execution_dml_numeric_token_scan *scan
) {
    int64_t integer_digits = 0;

    if (scan == NULL) {
        return 0;
    }
    integer_digits = scan->integer_digit_count > (size_t)dml_integer_decimal_shift_limit
                         ? dml_integer_decimal_shift_limit
                         : (int64_t)scan->integer_digit_count;
    if (scan->exponent > dml_integer_decimal_shift_limit - integer_digits) {
        return dml_integer_decimal_shift_limit;
    }
    if (scan->exponent < -dml_integer_decimal_shift_limit + integer_digits) {
        return -dml_integer_decimal_shift_limit;
    }
    return integer_digits + scan->exponent;
}

static bool dml_numeric_scan_exceeds_decimal_range(
    const struct mylite_execution_dml_numeric_token_scan *scan,
    const struct decimal_type_info *info
) {
    int64_t decimal_position = dml_numeric_scan_decimal_position(scan);
    int64_t integer_digit_count = 0;
    uint64_t integer_capacity = info == NULL ? 0U : info->precision - info->scale;

    if (scan == NULL || info == NULL || !scan->has_nonzero_digits) {
        return false;
    }
    if (info->is_unsigned && scan->is_negative) {
        return true;
    }
    if (decimal_position <= (int64_t)scan->first_nonzero_digit_index) {
        return false;
    }
    integer_digit_count = decimal_position - (int64_t)scan->first_nonzero_digit_index;
    return integer_digit_count > (int64_t)integer_capacity;
}

static size_t dml_numeric_scan_decimal_text_size(
    const struct mylite_execution_dml_numeric_token_scan *scan,
    const struct decimal_type_info *info
) {
    int64_t decimal_position = dml_numeric_scan_decimal_position(scan);
    size_t integer_output_digits =
        scan == NULL || decimal_position <= (int64_t)scan->first_nonzero_digit_index
            ? 1U
            : (size_t)(decimal_position - (int64_t)scan->first_nonzero_digit_index);
    size_t fraction_output_digits = info == NULL ? 0U : (size_t)info->scale + 1U;

    return integer_output_digits + (fraction_output_digits == 0U ? 0U : 1U) +
           fraction_output_digits;
}

static char dml_numeric_scan_virtual_digit(
    const char *digits,
    const struct mylite_execution_dml_numeric_token_scan *scan,
    int64_t digit_index
) {
    if (digit_index < 0 || digits == NULL || scan == NULL ||
        (size_t)digit_index >= scan->digit_count) {
        return '0';
    }
    return digits[digit_index];
}

static bool dml_numeric_scan_has_nonzero_virtual_digit_at_or_after(
    const struct mylite_execution_dml_numeric_token_scan *scan,
    int64_t digit_index
) {
    if (scan == NULL || !scan->has_nonzero_digits) {
        return false;
    }
    if (digit_index <= 0) {
        return true;
    }
    return (uint64_t)digit_index <= scan->last_nonzero_digit_index;
}

static int convert_approximate_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    static const char true_text[] = "1";
    static const char false_text[] = "0";

    const struct mylite_sql_ast_node *literal = value_node;
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    bool is_negative = false;
    uint64_t boolean_magnitude = 0U;
    double value = 0.0;
    int rc = approximate_type_info_for_logical_type(column->logical_type, &info);

    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "FLOAT and DOUBLE values require baseline descriptors");
        return MYLITE_ERROR;
    }
    if (value_node == NULL) {
        set_unsupported_error(
            database,
            "FLOAT and DOUBLE values support only numeric, boolean, hex, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "FLOAT and DOUBLE values support only numeric, boolean, hex, NULL, and DEFAULT "
                "values"
            );
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (boolean_literal_magnitude(literal, &boolean_magnitude)) {
        rc = parse_approximate_literal_value(
            database,
            boolean_magnitude == 0U ? false_text : true_text,
            1U,
            is_negative,
            &info,
            column->name,
            row_number,
            &value
        );
        if (rc == MYLITE_OK) {
            rc = assign_approximate_value(value, out_value);
        }
        return rc;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        (mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER &&
         mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_DECIMAL &&
         mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_FLOAT &&
         mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_STRING &&
         mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_HEX)) {
        set_unsupported_error(
            database,
            "FLOAT and DOUBLE values support only numeric, boolean, hex, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
        return convert_approximate_string_literal(
            database,
            literal,
            column,
            row_number,
            ignore_errors,
            &info,
            out_value
        );
    }
    if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_HEX) {
        return convert_approximate_hex_literal(
            database,
            literal,
            column,
            row_number,
            ignore_errors,
            is_negative,
            &info,
            out_value
        );
    }

    rc = parse_approximate_literal_value(
        database,
        literal->span.text,
        literal->span.length,
        is_negative,
        &info,
        column->name,
        row_number,
        &value
    );
    if (rc == MYLITE_OK) {
        rc = assign_approximate_value(value, out_value);
    }
    return rc;
}

static int convert_approximate_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool is_negative,
    const struct approximate_type_info *info,
    struct planned_value *out_value
) {
    uint64_t magnitude = 0U;
    bool overflow = false;
    double value = 0.0;
    int rc = parse_dml_hex_literal_magnitude(database, literal, &magnitude, &overflow);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (overflow) {
        if (!ignore_errors) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        return convert_adjusted_approximate_range_value(
            database,
            info,
            is_negative,
            column->name,
            row_number,
            out_value
        );
    }

    value = (double)magnitude;
    if (is_negative) {
        value = -value;
    }
    if (approximate_value_is_out_of_range(value, info)) {
        if (!ignore_errors) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        return convert_adjusted_approximate_range_value(
            database,
            info,
            is_negative,
            column->name,
            row_number,
            out_value
        );
    }
    return assign_approximate_value(approximate_storage_value(value, info), out_value);
}

static int convert_approximate_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    const struct approximate_type_info *info,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    struct mylite_execution_dml_numeric_token_scan scan = {0};
    bool adjust_errors = dml_allows_missing_default_adjustment(database, ignore_errors);
    enum mylite_execution_dml_numeric_parse_result parse_result =
        MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
    int rc = decode_sql_string_literal(
        database,
        literal,
        "FLOAT and DOUBLE values support only numeric, boolean, hex, NULL, DEFAULT, and quoted "
        "numeric string values",
        "invalid FLOAT or DOUBLE string literal",
        &text,
        &text_length
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    parse_result = mylite_execution_scan_dml_numeric_string_text(text, text_length, &scan);
    if (parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID) {
        if (!adjust_errors) {
            set_data_truncated_error(database, column->name, row_number);
            rc = MYLITE_ERROR;
        } else {
            rc = append_data_truncated_warning(database, column->name, row_number);
            if (rc == MYLITE_OK) {
                rc = assign_approximate_value(0.0, out_value);
            }
        }
    } else {
        rc = convert_approximate_scanned_string_literal(
            database,
            text,
            &scan,
            parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_TRUNCATED,
            column,
            row_number,
            adjust_errors,
            info,
            out_value
        );
    }
    free(text);
    return rc;
}

static int convert_approximate_scanned_string_literal(
    struct mylite_db *database,
    const char *text,
    const struct mylite_execution_dml_numeric_token_scan *scan,
    bool truncated,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool adjust_errors,
    const struct approximate_type_info *info,
    struct planned_value *out_value
) {
    bool token_is_out_of_range = false;
    bool value_is_out_of_range = false;
    double value = 0.0;
    int rc = parse_approximate_scanned_value(database, text, scan, &value, &token_is_out_of_range);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (token_is_out_of_range) {
        if (!adjust_errors) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        return convert_adjusted_approximate_range_value(
            database,
            info,
            scan->is_negative,
            column->name,
            row_number,
            out_value
        );
    }

    value_is_out_of_range = approximate_value_is_out_of_range(value, info);
    if (truncated) {
        if (!adjust_errors) {
            set_data_truncated_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        rc = append_data_truncated_warning(database, column->name, row_number);
    }
    if (rc == MYLITE_OK && value_is_out_of_range) {
        if (!adjust_errors) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        rc = convert_adjusted_approximate_range_value(
            database,
            info,
            value < 0.0,
            column->name,
            row_number,
            out_value
        );
    } else if (rc == MYLITE_OK) {
        rc = assign_approximate_value(approximate_storage_value(value, info), out_value);
    }
    return rc;
}

static int parse_approximate_scanned_value(
    struct mylite_db *database,
    const char *text,
    const struct mylite_execution_dml_numeric_token_scan *scan,
    double *out_value,
    bool *out_is_out_of_range
) {
    size_t token_length = 0U;
    char *token = NULL;
    char *end = NULL;
    double parsed = 0.0;
    int rc = MYLITE_OK;

    if (text == NULL || scan == NULL || out_value == NULL || out_is_out_of_range == NULL ||
        scan->token_end <= scan->token_start) {
        return MYLITE_MISUSE;
    }
    *out_value = 0.0;
    *out_is_out_of_range = false;
    token_length = scan->token_end - scan->token_start;
    token = calloc(token_length + 1U, sizeof(*token));
    if (token == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(token, text + scan->token_start, token_length);
    token[token_length] = '\0';

    rc = mylite_execution_parse_c_locale_double(token, &end, &parsed);
    if (rc == MYLITE_OK && (end == token || end == NULL || *end != '\0')) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && !isfinite(parsed)) {
        *out_is_out_of_range = true;
    } else if (rc == MYLITE_OK) {
        *out_value = parsed == 0.0 ? 0.0 : parsed;
    }
    free(token);
    return rc;
}

static int convert_adjusted_approximate_range_value(
    struct mylite_db *database,
    const struct approximate_type_info *info,
    bool is_negative,
    const char *column_name,
    size_t row_number,
    struct planned_value *out_value
) {
    int rc = append_out_of_range_warning(database, column_name, row_number);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (info->is_unsigned && is_negative) {
        return assign_approximate_value(0.0, out_value);
    }
    return assign_approximate_value(approximate_endpoint_value(info, is_negative), out_value);
}

static bool approximate_value_is_out_of_range(
    double value,
    const struct approximate_type_info *info
) {
    if (info == NULL || !isfinite(value)) {
        return true;
    }
    if (info->is_unsigned && value < 0.0) {
        return true;
    }
    if (info->type_class == APPROXIMATE_TYPE_FLOAT) {
        float single = (float)value;

        return isfinite(single) == 0;
    }
    return false;
}

static double approximate_storage_value(double value, const struct approximate_type_info *info) {
    if (info != NULL && info->type_class == APPROXIMATE_TYPE_FLOAT) {
        float single = (float)value;

        return single == 0.0F ? 0.0 : (double)single;
    }
    return value == 0.0 ? 0.0 : value;
}

static double approximate_endpoint_value(const struct approximate_type_info *info, bool negative) {
    double value =
        info != NULL && info->type_class == APPROXIMATE_TYPE_FLOAT ? (double)FLT_MAX : DBL_MAX;

    if (negative) {
        return -value;
    }
    return value;
}

static int parse_approximate_literal_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    bool is_negative,
    const struct approximate_type_info *info,
    const char *column_name,
    size_t row_number,
    double *out_value
) {
    char buffer[approximate_numeric_text_capacity];
    char *end = NULL;
    double parsed = 0.0;

    if (text == NULL || info == NULL || column_name == NULL || out_value == NULL ||
        text_length == 0U || text_length >= sizeof(buffer)) {
        set_unsupported_error(
            database,
            "FLOAT and DOUBLE values support only numeric, boolean, hex, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }
    memcpy(buffer, text, text_length);
    buffer[text_length] = '\0';

    if (mylite_execution_parse_c_locale_double(buffer, &end, &parsed) != MYLITE_OK ||
        end == buffer || end == NULL || *end != '\0' || !isfinite(parsed)) {
        set_out_of_range_error(database, column_name, row_number);
        return MYLITE_ERROR;
    }
    if (is_negative) {
        parsed = -parsed;
    }
    if (parsed == 0.0) {
        parsed = 0.0;
    }
    if (info->is_unsigned && parsed < 0.0) {
        set_out_of_range_error(database, column_name, row_number);
        return MYLITE_ERROR;
    }
    if (info->type_class == APPROXIMATE_TYPE_FLOAT) {
        float single = (float)parsed;

        if (!isfinite(single)) {
            set_out_of_range_error(database, column_name, row_number);
            return MYLITE_ERROR;
        }
        *out_value = single == 0.0F ? 0.0 : (double)single;
        return MYLITE_OK;
    }

    *out_value = parsed;
    return MYLITE_OK;
}

static int assign_approximate_value(double value, struct planned_value *out_value) {
    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = false,
        .is_text = false,
        .is_real = true,
        .integer = 0,
        .real = value == 0.0 ? 0.0 : value,
        .text = NULL,
        .text_length = 0U,
    };
    return MYLITE_OK;
}

static int copy_approximate_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    double value = 0.0;
    size_t text_length = column == NULL ? 0U : strlen(column->default_text);
    int rc = column == NULL ? MYLITE_ERROR
                            : approximate_type_info_for_logical_type(column->logical_type, &info);

    if (rc == MYLITE_OK) {
        rc = parse_approximate_literal_value(
            database,
            column->default_text,
            text_length,
            false,
            &info,
            column->name,
            1U,
            &value
        );
    }
    if (rc == MYLITE_OK) {
        rc = assign_approximate_value(value, out_value);
    }

    return rc;
}

static int make_zero_approximate_value(struct planned_value *out_value) {
    return assign_approximate_value(0.0, out_value);
}

static int format_approximate_value_text(
    struct mylite_db *database,
    double value,
    const struct approximate_type_info *info,
    const char *context_name,
    char *buffer,
    size_t buffer_size
) {
    if (info == NULL) {
        return MYLITE_MISUSE;
    }
    if (info->has_scale) {
        return format_scaled_approximate_value_text(
            database,
            value,
            info,
            context_name,
            buffer,
            buffer_size
        );
    }
    if (info->type_class == APPROXIMATE_TYPE_FLOAT) {
        return format_float_value_text(database, value, context_name, buffer, buffer_size);
    }

    return mylite_execution_format_double_text(
        database,
        value == 0.0 ? 0.0 : value,
        context_name,
        buffer,
        buffer_size
    );
}

static int format_scaled_approximate_value_text(
    struct mylite_db *database,
    double value,
    const struct approximate_type_info *info,
    const char *context_name,
    char *buffer,
    size_t buffer_size
) {
    int written = 0;

    if (info == NULL || context_name == NULL || buffer == NULL || buffer_size == 0U ||
        info->scale > decimal_max_scale) {
        return MYLITE_MISUSE;
    }
    written = mylite_execution_format_c_locale_text(
        buffer,
        buffer_size,
        "%.*f",
        (int)info->scale,
        value == 0.0 ? 0.0 : value
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        mylite_execution_set_double_format_error(database, context_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int format_float_value_text(
    struct mylite_db *database,
    double value,
    const char *context_name,
    char *buffer,
    size_t buffer_size
) {
    char candidate[approximate_numeric_text_capacity];
    int written = 0;

    if (context_name == NULL || buffer == NULL || buffer_size == 0U) {
        return MYLITE_MISUSE;
    }
    written = mylite_execution_format_c_locale_text(
        candidate,
        sizeof(candidate),
        "%.*g",
        float_text_max_significant_digits,
        value
    );
    if (written < 0 || (size_t)written >= sizeof(candidate)) {
        mylite_execution_set_double_format_error(database, context_name);
        return MYLITE_ERROR;
    }
    if (strchr(candidate, 'e') != NULL || strchr(candidate, 'E') != NULL) {
        return mylite_execution_copy_normalized_scientific_double_text(
            database,
            candidate,
            context_name,
            buffer,
            buffer_size
        );
    }
    return mylite_execution_copy_normalized_double_text(
        database,
        candidate,
        context_name,
        buffer,
        buffer_size
    );
}

static int canonicalize_decimal_text(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    bool is_negative,
    const struct decimal_type_info *info,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    struct decimal_literal_parts parts = {0};
    struct decimal_digit_buffer buffer = {0};
    int rc = MYLITE_OK;

    if (text == NULL || info == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

    rc = parse_decimal_literal_parts(database, text, text_length, &parts);
    if (rc == MYLITE_OK) {
        rc = build_decimal_digit_buffer(database, text, info, &parts, &buffer);
    }
    if (rc == MYLITE_OK &&
        decimal_value_is_out_of_range(info, is_negative, buffer.digits, buffer.digit_count)) {
        rc = handle_decimal_out_of_range(
            database,
            info,
            column_name,
            row_number,
            is_negative,
            ignore_errors,
            out_value
        );
        free(buffer.digits);
        return rc;
    }
    if (rc == MYLITE_OK && buffer.discarded_nonzero) {
        rc = append_decimal_truncated_note(database, column_name, row_number);
    }
    if (rc == MYLITE_OK) {
        bool keep_negative_sign =
            (is_negative && !decimal_digits_are_zero(buffer.digits, buffer.digit_count)) != 0;

        rc = format_decimal_digits(
            database,
            buffer.digits,
            buffer.digit_count,
            keep_negative_sign,
            info->scale,
            out_value
        );
    }
    free(buffer.digits);
    return rc;
}

static int parse_decimal_literal_parts(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    struct decimal_literal_parts *out_parts
) {
    size_t integer_end = 0U;
    size_t fraction_start = 0U;
    size_t fraction_end = 0U;
    bool saw_dot = false;
    bool saw_digit = false;

    if (text == NULL || out_parts == NULL) {
        return MYLITE_MISUSE;
    }

    while (integer_end < text_length && text[integer_end] >= '0' && text[integer_end] <= '9') {
        ++integer_end;
        saw_digit = true;
    }
    fraction_start = integer_end;
    fraction_end = fraction_start;
    if (fraction_start < text_length && text[fraction_start] == '.') {
        saw_dot = true;
        ++fraction_start;
        fraction_end = fraction_start;
        while (fraction_end < text_length && text[fraction_end] >= '0' &&
               text[fraction_end] <= '9') {
            ++fraction_end;
            saw_digit = true;
        }
    }
    if (!saw_digit || fraction_end != text_length || (!saw_dot && integer_end == 0U)) {
        set_unsupported_error(
            database,
            "DECIMAL values support only fixed decimal and integer literals"
        );
        return MYLITE_ERROR;
    }

    *out_parts = (struct decimal_literal_parts){
        .integer_end = integer_end,
        .fraction_start = fraction_start,
        .fraction_end = fraction_end,
        .integer_digit_start = 0U,
    };
    while (out_parts->integer_digit_start < integer_end &&
           text[out_parts->integer_digit_start] == '0') {
        ++out_parts->integer_digit_start;
    }

    return MYLITE_OK;
}

static int build_decimal_digit_buffer(
    struct mylite_db *database,
    const char *text,
    const struct decimal_type_info *info,
    const struct decimal_literal_parts *parts,
    struct decimal_digit_buffer *out_buffer
) {
    size_t digit_capacity = 0U;
    size_t digit_count = 0U;
    size_t integer_digit_count = 0U;
    size_t scale = 0U;

    if (text == NULL || info == NULL || parts == NULL || out_buffer == NULL) {
        return MYLITE_MISUSE;
    }
    if (parts->integer_digit_start > parts->integer_end ||
        parts->integer_end > parts->fraction_start || parts->fraction_start > parts->fraction_end ||
        info->scale > (uint64_t)(SIZE_MAX - 2U)) {
        set_runtime_error(database, "invalid DECIMAL digit buffer bounds");
        return MYLITE_ERROR;
    }
    integer_digit_count = parts->integer_end - parts->integer_digit_start;
    scale = (size_t)info->scale;
    if (integer_digit_count > SIZE_MAX - scale - 2U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    digit_capacity = integer_digit_count + scale + 2U;

    *out_buffer = (struct decimal_digit_buffer){
        .digits = malloc(digit_capacity),
        .digit_count = 0U,
        .digit_capacity = digit_capacity,
        .discarded_nonzero = false,
    };
    if (out_buffer->digits == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t index = parts->integer_digit_start; index < parts->integer_end; ++index) {
        if (digit_count >= out_buffer->digit_capacity) {
            set_runtime_error(database, "DECIMAL digit buffer overflow");
            return MYLITE_ERROR;
        }
        out_buffer->digits[digit_count] = text[index];
        ++digit_count;
    }
    for (size_t scale_index = 0U; scale_index < scale; ++scale_index) {
        size_t fraction_index = parts->fraction_start + scale_index;

        if (digit_count >= out_buffer->digit_capacity) {
            set_runtime_error(database, "DECIMAL digit buffer overflow");
            return MYLITE_ERROR;
        }
        if (fraction_index < parts->fraction_end) {
            out_buffer->digits[digit_count] = text[fraction_index];
        } else {
            out_buffer->digits[digit_count] = '0';
        }
        ++digit_count;
    }
    out_buffer->digit_count = digit_count;

    return round_decimal_digit_buffer_if_needed(database, text, info, parts, out_buffer);
}

static int round_decimal_digit_buffer_if_needed(
    struct mylite_db *database,
    const char *text,
    const struct decimal_type_info *info,
    const struct decimal_literal_parts *parts,
    struct decimal_digit_buffer *buffer
) {
    size_t round_index =
        parts == NULL || info == NULL ? 0U : parts->fraction_start + (size_t)info->scale;
    int rc = MYLITE_OK;

    if (text == NULL || info == NULL || parts == NULL || buffer == NULL) {
        return MYLITE_MISUSE;
    }
    if ((uint64_t)(parts->fraction_end - parts->fraction_start) <= info->scale) {
        return MYLITE_OK;
    }

    for (size_t index = round_index; index < parts->fraction_end; ++index) {
        if (text[index] != '0') {
            buffer->discarded_nonzero = true;
            break;
        }
    }
    if (text[round_index] < '5') {
        return MYLITE_OK;
    }

    rc = increment_decimal_digits(buffer->digits, &buffer->digit_count, buffer->digit_capacity);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static bool decimal_value_is_out_of_range(
    const struct decimal_type_info *info,
    bool is_negative,
    const char *digits,
    size_t digit_count
) {
    uint64_t integer_digits = 0U;
    uint64_t integer_capacity = info == NULL ? 0U : info->precision - info->scale;

    if (info == NULL || decimal_digits_are_zero(digits, digit_count)) {
        return false;
    }
    if (digit_count > (size_t)info->scale) {
        integer_digits = (uint64_t)(digit_count - (size_t)info->scale);
    }

    return (integer_digits > integer_capacity || (info->is_unsigned && is_negative)) != 0;
}

static int handle_decimal_out_of_range(
    struct mylite_db *database,
    const struct decimal_type_info *info,
    const char *column_name,
    size_t row_number,
    bool is_negative,
    bool ignore_errors,
    struct planned_value *out_value
) {
    int rc = MYLITE_OK;

    if (!ignore_errors) {
        set_out_of_range_error(database, column_name, row_number);
        return MYLITE_ERROR;
    }

    rc = append_out_of_range_warning(database, column_name, row_number);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (info->is_unsigned && is_negative) {
        return build_decimal_zero_value(database, info, out_value);
    }

    return build_decimal_endpoint_value(database, info, is_negative, out_value);
}

static int build_decimal_zero_value(
    struct mylite_db *database,
    const struct decimal_type_info *info,
    struct planned_value *out_value
) {
    return format_decimal_digits(database, "", 0U, false, info->scale, out_value);
}

static int build_decimal_endpoint_value(
    struct mylite_db *database,
    const struct decimal_type_info *info,
    bool is_negative,
    struct planned_value *out_value
) {
    char digits[decimal_max_precision];

    if (info == NULL || info->precision == 0U || info->precision > decimal_max_precision) {
        return MYLITE_ERROR;
    }
    memset(digits, '9', (size_t)info->precision);
    return format_decimal_digits(
        database,
        digits,
        (size_t)info->precision,
        (is_negative && !info->is_unsigned) != 0,
        info->scale,
        out_value
    );
}

static int assign_decimal_text_value(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    struct planned_value *out_value
) {
    return assign_text_value(database, text, text_length, out_value);
}
