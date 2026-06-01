static int scalar_division_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct scalar_arithmetic_value left = {.is_null = false, .integer = 0};
    struct scalar_arithmetic_value right = {.is_null = false, .integer = 0};
    size_t warning_count = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (!is_scalar_division_projection_expression(expression)) {
        set_scalar_division_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_scalar_arithmetic_expression(database, child_at(expression, 0U), &left);
    if (rc == MYLITE_OK &&
        scalar_division_left_null_short_circuits(child_at(expression, 0U), &left)) {
        out_cell->staged_division_by_zero_warning_count = left.division_by_zero_warning_count;
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = evaluate_scalar_arithmetic_expression(database, child_at(expression, 1U), &right);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = accumulate_staged_division_by_zero_warnings(
        database,
        left.division_by_zero_warning_count,
        &warning_count
    );
    if (rc == MYLITE_OK) {
        rc = accumulate_staged_division_by_zero_warnings(
            database,
            right.division_by_zero_warning_count,
            &warning_count
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (left.is_null || right.is_null) {
        out_cell->staged_division_by_zero_warning_count = warning_count;
        return MYLITE_OK;
    }
    if (right.integer == 0) {
        rc = accumulate_staged_division_by_zero_warnings(database, 1U, &warning_count);
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = warning_count;
        }
        return rc;
    }

    rc = format_scalar_division_value(
        database,
        left.integer,
        right.integer,
        out_cell->integer_text,
        sizeof(out_cell->integer_text)
    );
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->integer_text;
        out_cell->staged_division_by_zero_warning_count = warning_count;
    }
    return rc;
}

static bool scalar_division_left_null_short_circuits(
    const struct mylite_sql_ast_node *expression,
    const struct scalar_arithmetic_value *left
) {
    if (left == NULL || !left->is_null) {
        return false;
    }
    if (left->division_by_zero_warning_count != 0U) {
        return true;
    }

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_IF_FUNCTION) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_NULLIF_FUNCTION) {
        return true;
    }
    return false;
}

static int format_scalar_division_value(
    struct mylite_db *database,
    int64_t numerator,
    int64_t denominator,
    char *buffer,
    size_t buffer_size
) {
    uint64_t numerator_magnitude = absolute_int64_magnitude(numerator);
    uint64_t denominator_magnitude = absolute_int64_magnitude(denominator);
    uint64_t integer_part = 0U;
    uint64_t remainder = 0U;
    unsigned int fraction = 0U;
    int round_digit = 0;
    bool is_negative = (numerator < 0) != (denominator < 0);
    int written = 0;

    if (buffer == NULL || denominator_magnitude == 0U) {
        return MYLITE_MISUSE;
    }

    integer_part = numerator_magnitude / denominator_magnitude;
    remainder = numerator_magnitude % denominator_magnitude;
    for (size_t digit_index = 0U; digit_index < avg_fraction_digits; ++digit_index) {
        int digit = next_decimal_digit(&remainder, denominator_magnitude);

        if (digit < 0) {
            set_runtime_error(database, "failed to format scalar division value");
            return MYLITE_ERROR;
        }
        fraction = (fraction * decimal_base) + (unsigned int)digit;
    }
    round_digit = next_decimal_digit(&remainder, denominator_magnitude);
    if (round_digit < 0) {
        set_runtime_error(database, "failed to format scalar division value");
        return MYLITE_ERROR;
    }
    if (round_digit >= avg_round_half_digit) {
        ++fraction;
        if (fraction == avg_fraction_scale) {
            fraction = 0U;
            ++integer_part;
        }
    }

    written = snprintf(
        buffer,
        buffer_size,
        "%s%" PRIu64 ".%04u",
        is_negative && (integer_part != 0U || fraction != 0U) ? "-" : "",
        integer_part,
        fraction
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format scalar division value");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int scalar_bitwise_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct scalar_bitwise_value value = {.is_null = false, .integer = 0U};
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    rc = evaluate_scalar_bitwise_expression(database, expression, &value);
    if (rc != MYLITE_OK || value.is_null) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    rc = format_uint64(
        database,
        value.integer,
        out_cell->integer_text,
        sizeof(out_cell->integer_text)
    );
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->integer_text;
        out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
    }
    return rc;
}

static int abs_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct scalar_bitwise_value value = {.is_null = false, .integer = 0U};
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || mylite_sql_ast_node_child_count(expression) != 1U) {
        set_abs_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_abs_operand(database, child_at(expression, 0U), &value);
    if (rc != MYLITE_OK || value.is_null) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    rc = format_uint64(
        database,
        value.integer,
        out_cell->integer_text,
        sizeof(out_cell->integer_text)
    );
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->integer_text;
        out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
    }
    return rc;
}

static int evaluate_abs_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
) {
    struct scalar_arithmetic_value arithmetic = {.is_null = false, .integer = 0};
    bool handled = false;
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_bitwise_value){.is_null = false, .integer = 0U};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_abs_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_abs_direct_literal_operand(database, expression, out_value, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (is_scalar_bitwise_projection_expression(expression)) {
        return evaluate_scalar_bitwise_expression(database, expression, out_value);
    }
    if (!is_scalar_arithmetic_projection_expression(expression)) {
        set_abs_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_scalar_arithmetic_expression(database, expression, &arithmetic);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_value->is_null = arithmetic.is_null;
    out_value->division_by_zero_warning_count = arithmetic.division_by_zero_warning_count;
    if (arithmetic.is_null) {
        return MYLITE_OK;
    }
    if (arithmetic.integer == INT64_MIN) {
        set_abs_signed_minimum_overflow_error(database);
        return MYLITE_ERROR;
    }
    if (arithmetic.integer < 0) {
        out_value->integer = (uint64_t)(-arithmetic.integer);
    } else {
        out_value->integer = (uint64_t)arithmetic.integer;
    }
    return MYLITE_OK;
}

static int evaluate_abs_direct_literal_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value,
    bool *out_handled
) {
    static const uint64_t int64_min_magnitude = 9223372036854775808ULL;
    const struct mylite_sql_ast_node *literal = expression;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;
    bool has_sign = false;
    uint64_t magnitude = 0U;

    if (out_value == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (expression == NULL) {
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return MYLITE_OK;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            return MYLITE_OK;
        }
        has_sign = true;
        is_negative = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_OK;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_value->is_null = true;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        out_value->integer = 1U;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_value->integer = 0U;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return MYLITE_OK;
    }
    *out_handled = true;

    if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        set_abs_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (is_negative && magnitude == int64_min_magnitude) {
        set_abs_signed_minimum_overflow_error(database);
        return MYLITE_ERROR;
    }
    out_value->integer = magnitude;
    return MYLITE_OK;
}

static int sign_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || mylite_sql_ast_node_child_count(expression) != 1U) {
        set_sign_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_sign_operand(database, child_at(expression, 0U), &value);
    if (rc != MYLITE_OK || value.is_null) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    if (value.integer < 0) {
        out_cell->value = "-1";
    } else if (value.integer > 0) {
        out_cell->value = "1";
    } else {
        out_cell->value = "0";
    }
    out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
    return MYLITE_OK;
}

static int evaluate_sign_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
) {
    struct scalar_arithmetic_value arithmetic = {.is_null = false, .integer = 0};
    bool handled = false;
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){.is_null = false, .integer = 0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_sign_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_sign_direct_literal_operand(database, expression, out_value, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (is_scalar_bitwise_projection_expression(expression)) {
        struct scalar_bitwise_value bitwise = {.is_null = false, .integer = 0U};

        rc = evaluate_scalar_bitwise_expression(database, expression, &bitwise);
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_value->is_null = bitwise.is_null;
        out_value->division_by_zero_warning_count = bitwise.division_by_zero_warning_count;
        if (!bitwise.is_null) {
            out_value->integer = bitwise.integer == 0U ? 0 : 1;
        }
        return MYLITE_OK;
    }
    if (!is_scalar_arithmetic_projection_expression(expression)) {
        set_sign_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_scalar_arithmetic_expression(database, expression, &arithmetic);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_value->is_null = arithmetic.is_null;
    out_value->division_by_zero_warning_count = arithmetic.division_by_zero_warning_count;
    if (arithmetic.is_null) {
        return MYLITE_OK;
    }
    if (arithmetic.integer < 0) {
        out_value->integer = -1;
    } else if (arithmetic.integer > 0) {
        out_value->integer = 1;
    }
    return MYLITE_OK;
}

static int evaluate_sign_direct_literal_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value,
    bool *out_handled
) {
    const struct mylite_sql_ast_node *literal = expression;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;
    bool has_sign = false;

    if (out_value == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (expression == NULL) {
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return MYLITE_OK;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            return MYLITE_OK;
        }
        has_sign = true;
        is_negative = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_OK;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_value->is_null = true;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        out_value->integer = 1;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_value->integer = 0;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return MYLITE_OK;
    }
    *out_handled = true;

    return sign_of_decimal_integer_literal(database, &literal->span, is_negative, out_value);
}

static int sign_of_decimal_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    bool is_negative,
    struct scalar_arithmetic_value *out_value
) {
    bool is_zero = true;

    if (span == NULL || span->text == NULL || span->length == 0U || out_value == NULL) {
        set_sign_unsupported_error(database);
        return MYLITE_ERROR;
    }

    for (size_t index = 0U; index < span->length; ++index) {
        unsigned char byte = (unsigned char)span->text[index];

        if (byte < '0' || byte > '9') {
            set_sign_unsupported_error(database);
            return MYLITE_ERROR;
        }
        if (byte != '0') {
            is_zero = false;
        }
    }

    if (is_zero) {
        out_value->integer = 0;
    } else if (is_negative) {
        out_value->integer = -1;
    } else {
        out_value->integer = 1;
    }
    return MYLITE_OK;
}

static int rounding_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct scalar_arithmetic_value places = {.is_null = false, .integer = 0};
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL) {
        set_rounding_unsupported_error(database);
        return MYLITE_ERROR;
    }

    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count == 1U) {
        return evaluate_rounding_operand(database, child_at(expression, 0U), out_cell);
    }
    if (child_count != 2U) {
        set_rounding_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_rounding_places_operand(database, child_at(expression, 1U), &places);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (places.is_null || places.integer >= 0) {
        rc = evaluate_rounding_operand(database, child_at(expression, 0U), out_cell);
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count +=
                places.division_by_zero_warning_count;
            if (places.is_null) {
                out_cell->value = NULL;
            }
        }
        return rc;
    }

    struct rounding_signed_value value = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .staged_division_by_zero_warning_count = 0U,
    };

    rc = evaluate_rounding_signed_operand(database, child_at(expression, 0U), &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    value.staged_division_by_zero_warning_count += places.division_by_zero_warning_count;
    if (value.is_null) {
        out_cell->staged_division_by_zero_warning_count =
            value.staged_division_by_zero_warning_count;
        return MYLITE_OK;
    }

    return round_signed_value_to_negative_places(database, &value, places.integer, out_cell);
}

static int evaluate_rounding_places_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){.is_null = false, .integer = 0};
    if (expression == NULL || !is_scalar_arithmetic_projection_expression(expression)) {
        set_rounding_unsupported_error(database);
        return MYLITE_ERROR;
    }
    return evaluate_scalar_arithmetic_expression(database, expression, out_value);
}

static int evaluate_rounding_signed_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct rounding_signed_value *out_value
) {
    bool handled = false;
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct rounding_signed_value){
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .staged_division_by_zero_warning_count = 0U,
    };
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_rounding_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_rounding_direct_signed_operand(database, expression, out_value, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (is_scalar_arithmetic_projection_expression(expression)) {
        struct scalar_arithmetic_value arithmetic = {.is_null = false, .integer = 0};

        rc = evaluate_scalar_arithmetic_expression(database, expression, &arithmetic);
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_value->is_null = arithmetic.is_null;
        out_value->staged_division_by_zero_warning_count =
            arithmetic.division_by_zero_warning_count;
        if (!arithmetic.is_null) {
            if (arithmetic.integer < 0) {
                out_value->is_negative = true;
                out_value->magnitude = arithmetic.integer == INT64_MIN
                                           ? (uint64_t)INT64_MAX + 1U
                                           : (uint64_t)-arithmetic.integer;
            } else {
                out_value->magnitude = (uint64_t)arithmetic.integer;
            }
        }
        return MYLITE_OK;
    }
    if (is_scalar_bitwise_projection_expression(expression)) {
        struct scalar_bitwise_value bitwise = {.is_null = false, .integer = 0U};

        rc = evaluate_scalar_bitwise_expression(database, expression, &bitwise);
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_value->is_null = bitwise.is_null;
        out_value->staged_division_by_zero_warning_count = bitwise.division_by_zero_warning_count;
        if (!bitwise.is_null) {
            if (bitwise.integer > (uint64_t)INT64_MAX) {
                set_rounding_unsupported_error(database);
                return MYLITE_ERROR;
            }
            out_value->magnitude = bitwise.integer;
        }
        return MYLITE_OK;
    }

    set_rounding_unsupported_error(database);
    return MYLITE_ERROR;
}

static int evaluate_rounding_direct_signed_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct rounding_signed_value *out_value,
    bool *out_handled
) {
    const struct mylite_sql_ast_node *literal = expression;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;
    bool has_sign = false;
    uint64_t magnitude = 0U;

    if (out_value == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (expression == NULL) {
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return MYLITE_OK;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            return MYLITE_OK;
        }
        has_sign = true;
        is_negative = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_OK;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_handled = true;
        out_value->is_null = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_handled = true;
        out_value->magnitude = 1U;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_handled = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return MYLITE_OK;
    }
    *out_handled = true;
    if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        set_rounding_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (is_negative) {
        if (magnitude > (uint64_t)INT64_MAX + 1U) {
            set_rounding_unsupported_error(database);
            return MYLITE_ERROR;
        }
        out_value->is_negative = magnitude != 0U;
    } else if (magnitude > (uint64_t)INT64_MAX) {
        set_rounding_unsupported_error(database);
        return MYLITE_ERROR;
    }
    out_value->magnitude = magnitude;
    return MYLITE_OK;
}

static int round_signed_value_to_negative_places(
    struct mylite_db *database,
    const struct rounding_signed_value *value,
    int64_t places,
    struct session_scalar_cell *out_cell
) {
    uint64_t place_count = 0U;
    uint64_t divisor = 1U;
    uint64_t quotient = 0U;
    uint64_t remainder = 0U;
    uint64_t rounded = 0U;
    int written = 0;

    if (value == NULL || out_cell == NULL || places >= 0) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    out_cell->staged_division_by_zero_warning_count = value->staged_division_by_zero_warning_count;

    if (places <= -(int64_t)rounding_negative_places_zero_threshold) {
        out_cell->value = "0";
        return MYLITE_OK;
    }
    place_count = (uint64_t)-places;
    for (uint64_t index = 0U; index < place_count; ++index) {
        divisor *= decimal_base;
    }

    quotient = value->magnitude / divisor;
    remainder = value->magnitude % divisor;
    if (remainder >= divisor / 2U) {
        ++quotient;
    }
    if (quotient > UINT64_MAX / divisor) {
        return set_rounding_signed_overflow_error(database);
    }
    rounded = quotient * divisor;
    if (rounded == 0U) {
        out_cell->value = "0";
        return MYLITE_OK;
    }
    if (rounded > (uint64_t)INT64_MAX) {
        return set_rounding_signed_overflow_error(database);
    }

    if (value->is_negative) {
        written =
            snprintf(out_cell->integer_text, sizeof(out_cell->integer_text), "-%" PRIu64, rounded);
    } else {
        written =
            snprintf(out_cell->integer_text, sizeof(out_cell->integer_text), "%" PRIu64, rounded);
    }
    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        set_runtime_error(database, "failed to format scalar rounding value");
        return MYLITE_ERROR;
    }
    out_cell->value = out_cell->integer_text;
    return MYLITE_OK;
}

static int evaluate_rounding_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    bool handled = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_rounding_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_rounding_direct_literal_operand(database, expression, out_cell, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (is_scalar_bitwise_projection_expression(expression)) {
        struct scalar_bitwise_value bitwise = {.is_null = false, .integer = 0U};

        rc = evaluate_scalar_bitwise_expression(database, expression, &bitwise);
        if (rc != MYLITE_OK || bitwise.is_null) {
            if (rc == MYLITE_OK) {
                out_cell->staged_division_by_zero_warning_count =
                    bitwise.division_by_zero_warning_count;
            }
            return rc;
        }
        rc = format_uint64(
            database,
            bitwise.integer,
            out_cell->integer_text,
            sizeof(out_cell->integer_text)
        );
        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->integer_text;
            out_cell->staged_division_by_zero_warning_count =
                bitwise.division_by_zero_warning_count;
        }
        return rc;
    }
    if (is_scalar_arithmetic_projection_expression(expression)) {
        struct scalar_arithmetic_value arithmetic = {.is_null = false, .integer = 0};
        int written = 0;

        rc = evaluate_scalar_arithmetic_expression(database, expression, &arithmetic);
        if (rc != MYLITE_OK || arithmetic.is_null) {
            if (rc == MYLITE_OK) {
                out_cell->staged_division_by_zero_warning_count =
                    arithmetic.division_by_zero_warning_count;
            }
            return rc;
        }
        written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRId64,
            arithmetic.integer
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            set_runtime_error(database, "failed to format scalar rounding value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        out_cell->staged_division_by_zero_warning_count = arithmetic.division_by_zero_warning_count;
        return MYLITE_OK;
    }

    set_rounding_unsupported_error(database);
    return MYLITE_ERROR;
}

static int evaluate_rounding_direct_literal_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    bool *out_handled
) {
    const struct mylite_sql_ast_node *literal = expression;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;
    bool has_sign = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (expression == NULL) {
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return MYLITE_OK;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            return MYLITE_OK;
        }
        has_sign = true;
        is_negative = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_OK;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_handled = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        out_cell->value = "1";
        *out_handled = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_cell->value = "0";
        *out_handled = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return MYLITE_OK;
    }
    *out_handled = true;

    rc = normalize_decimal_integer_literal(
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

static int sqrt_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value value = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    double result = 0.0;
    uint64_t integer_result = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SQRT_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_sqrt_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_approximate_numeric_operand(
        database,
        child_at(expression, 0U),
        &value,
        set_sqrt_unsupported_error
    );
    if (rc != MYLITE_OK || value.is_null || value.is_negative) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    result = sqrt((double)value.magnitude);
    integer_result = (uint64_t)result;
    if ((double)integer_result == result) {
        rc = format_uint64(
            database,
            integer_result,
            out_cell->double_text,
            sizeof(out_cell->double_text)
        );
    } else {
        rc = format_double_text(
            database,
            result,
            "SQRT",
            out_cell->double_text,
            sizeof(out_cell->double_text)
        );
    }
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->double_text;
        out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
    }
    return rc;
}

static int angle_conversion_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value value = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    bool is_degrees = false;
    const char *function_name = NULL;
    double input = 0.0;
    double output = 0.0;
    double pi = acos(-1.0);
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL ||
        (expression->kind != MYLITE_SQL_AST_DEGREES_FUNCTION &&
         expression->kind != MYLITE_SQL_AST_RADIANS_FUNCTION) ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_angle_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }

    is_degrees = expression->kind == MYLITE_SQL_AST_DEGREES_FUNCTION;
    if (is_degrees) {
        function_name = "DEGREES";
    } else {
        function_name = "RADIANS";
    }
    rc = evaluate_approximate_numeric_operand(
        database,
        child_at(expression, 0U),
        &value,
        set_angle_conversion_unsupported_error
    );
    if (rc != MYLITE_OK || value.is_null) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    input = (double)value.magnitude;
    if (value.is_negative) {
        input = -input;
    }
    if (is_degrees) {
        output = input * (angle_conversion_half_turn_degrees / pi);
    } else {
        output = input * (pi / angle_conversion_half_turn_degrees);
    }
    rc = format_double_text(
        database,
        output,
        function_name,
        out_cell->double_text,
        sizeof(out_cell->double_text)
    );
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->double_text;
        out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
    }
    return rc;
}

static int inverse_trig_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value value = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    bool is_acos = false;
    const char *function_name = NULL;
    double input = 0.0;
    double output = 0.0;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL ||
        (expression->kind != MYLITE_SQL_AST_ACOS_FUNCTION &&
         expression->kind != MYLITE_SQL_AST_ASIN_FUNCTION) ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_inverse_trig_unsupported_error(database);
        return MYLITE_ERROR;
    }

    is_acos = expression->kind == MYLITE_SQL_AST_ACOS_FUNCTION;
    if (is_acos) {
        function_name = "ACOS";
    } else {
        function_name = "ASIN";
    }
    rc = evaluate_approximate_numeric_operand(
        database,
        child_at(expression, 0U),
        &value,
        set_inverse_trig_unsupported_error
    );
    if (rc != MYLITE_OK || value.is_null || value.magnitude > 1U) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    input = (double)value.magnitude;
    if (value.is_negative) {
        input = -input;
    }
    if (is_acos) {
        output = acos(input);
    } else {
        output = asin(input);
    }
    rc = format_double_text(
        database,
        output,
        function_name,
        out_cell->double_text,
        sizeof(out_cell->double_text)
    );
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->double_text;
        out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
    }
    return rc;
}

static int direct_trig_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value value = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    const char *function_name = NULL;
    size_t warning_count = 0U;
    double input = 0.0;
    double output = 0.0;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    function_name = direct_trig_function_name(expression);
    if (function_name == NULL || mylite_sql_ast_node_child_count(expression) != 1U) {
        set_direct_trig_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_approximate_numeric_operand(
        database,
        child_at(expression, 0U),
        &value,
        set_direct_trig_unsupported_error
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = accumulate_staged_warning_count(
        database,
        value.division_by_zero_warning_count,
        &warning_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value.is_null) {
        out_cell->staged_division_by_zero_warning_count = warning_count;
        return MYLITE_OK;
    }

    input = approximate_numeric_input_to_double(&value);
    switch (expression->kind) {
    case MYLITE_SQL_AST_SIN_FUNCTION:
        output = sin(input);
        break;
    case MYLITE_SQL_AST_COS_FUNCTION:
        output = cos(input);
        break;
    case MYLITE_SQL_AST_TAN_FUNCTION:
        output = tan(input);
        break;
    case MYLITE_SQL_AST_COT_FUNCTION:
        if (input == 0.0) {
            rc = append_division_by_zero_warnings(database, warning_count);
            if (rc != MYLITE_OK) {
                return rc;
            }
            return set_cot_zero_out_of_range_error(database, expression);
        }
        output = 1.0 / tan(input);
        break;
    default:
        set_direct_trig_unsupported_error(database);
        return MYLITE_ERROR;
    }

    return finish_direct_trig_function_value(
        database,
        output,
        function_name,
        warning_count,
        out_cell
    );
}

static const char *direct_trig_function_name(const struct mylite_sql_ast_node *expression) {
    if (expression == NULL) {
        return NULL;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_SIN_FUNCTION:
        return "SIN";
    case MYLITE_SQL_AST_COS_FUNCTION:
        return "COS";
    case MYLITE_SQL_AST_TAN_FUNCTION:
        return "TAN";
    case MYLITE_SQL_AST_COT_FUNCTION:
        return "COT";
    default:
        return NULL;
    }
}

static int finish_direct_trig_function_value(
    struct mylite_db *database,
    double output,
    const char *function_name,
    size_t warning_count,
    struct session_scalar_cell *out_cell
) {
    int rc = MYLITE_OK;

    if (out_cell == NULL || function_name == NULL) {
        return MYLITE_MISUSE;
    }
    rc = format_double_text(
        database,
        output,
        function_name,
        out_cell->double_text,
        sizeof(out_cell->double_text)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_cell->value = out_cell->double_text;
    out_cell->staged_division_by_zero_warning_count = warning_count;
    return MYLITE_OK;
}

static int set_cot_zero_out_of_range_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    enum { cot_error_expression_text_capacity = 64 };

    char text[cot_error_expression_text_capacity];
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t length = 0U;
    int written = 0;

    if (expression != NULL && expression->span.text != NULL && expression->span.length != 0U &&
        expression->span.length < sizeof(text)) {
        length = expression->span.length;
        for (size_t index = 0U; index < length; ++index) {
            char character = expression->span.text[index];

            if (character >= 'A' && character <= 'Z') {
                character = (char)(character - 'A' + 'a');
            }
            text[index] = character;
        }
        text[length] = '\0';
    } else {
        memcpy(text, "cot(0)", sizeof("cot(0)"));
    }

    written = snprintf(message, sizeof(message), "DOUBLE value is out of range in '%s'", text);
    if (written < 0 || (size_t)written >= sizeof(message)) {
        set_runtime_error(database, "DOUBLE value is out of range");
        return MYLITE_ERROR;
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_bigint_out_of_range,
        "22003",
        message
    );
    return MYLITE_ERROR;
}

static int atan_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value first = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    const char *function_name = NULL;
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || (expression->kind != MYLITE_SQL_AST_ATAN_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_ATAN2_FUNCTION)) {
        set_atan_unsupported_error(database);
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count != 1U && child_count != 2U) {
        set_atan_unsupported_error(database);
        return MYLITE_ERROR;
    }

    function_name = expression->kind == MYLITE_SQL_AST_ATAN_FUNCTION ? "ATAN" : "ATAN2";
    rc = evaluate_approximate_numeric_operand(
        database,
        child_at(expression, 0U),
        &first,
        set_atan_unsupported_error
    );
    if (rc != MYLITE_OK || first.is_null) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = first.division_by_zero_warning_count;
        }
        return rc;
    }

    if (child_count == 1U) {
        return atan_one_argument_function_value(database, &first, function_name, out_cell);
    }

    return atan_two_argument_function_value(database, expression, &first, function_name, out_cell);
}

static int atan_one_argument_function_value(
    struct mylite_db *database,
    const struct approximate_numeric_input_value *first,
    const char *function_name,
    struct session_scalar_cell *out_cell
) {
    size_t warning_count = 0U;
    double output = 0.0;
    int rc = MYLITE_OK;

    if (first == NULL) {
        return MYLITE_MISUSE;
    }
    rc = accumulate_staged_division_by_zero_warnings(
        database,
        first->division_by_zero_warning_count,
        &warning_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    output = atan2(approximate_numeric_input_to_double(first), 1.0);
    return finish_atan_function_value(database, output, function_name, warning_count, out_cell);
}

static int atan_two_argument_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct approximate_numeric_input_value *first,
    const char *function_name,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value second = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    size_t warning_count = 0U;
    double output = 0.0;
    int rc = MYLITE_OK;

    if (expression == NULL || first == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }

    rc = evaluate_approximate_numeric_operand(
        database,
        child_at(expression, 1U),
        &second,
        set_atan_unsupported_error
    );
    if (rc == MYLITE_OK) {
        rc = accumulate_staged_division_by_zero_warnings(
            database,
            first->division_by_zero_warning_count,
            &warning_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = accumulate_staged_division_by_zero_warnings(
            database,
            second.division_by_zero_warning_count,
            &warning_count
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (second.is_null) {
        out_cell->staged_division_by_zero_warning_count = warning_count;
        return MYLITE_OK;
    }

    output = atan2(
        approximate_numeric_input_to_double(first),
        approximate_numeric_input_to_double(&second)
    );
    return finish_atan_function_value(database, output, function_name, warning_count, out_cell);
}

static int finish_atan_function_value(
    struct mylite_db *database,
    double output,
    const char *function_name,
    size_t warning_count,
    struct session_scalar_cell *out_cell
) {
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    rc = format_double_text(
        database,
        output,
        function_name,
        out_cell->double_text,
        sizeof(out_cell->double_text)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_cell->value = out_cell->double_text;
    out_cell->staged_division_by_zero_warning_count = warning_count;
    return MYLITE_OK;
}

static int exp_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value value = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    double output = 0.0;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_EXP_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_exp_log_power_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_approximate_numeric_operand(
        database,
        child_at(expression, 0U),
        &value,
        set_exp_log_power_unsupported_error
    );
    if (rc != MYLITE_OK || value.is_null) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    output = exp(approximate_numeric_input_to_double(&value));
    if (!isfinite(output)) {
        return set_double_out_of_range_error(database, "exp");
    }
    return finish_exp_log_power_function_value(
        database,
        output,
        "EXP",
        value.division_by_zero_warning_count,
        out_cell
    );
}

static int logarithm_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value value = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    const char *function_name = NULL;
    double log_base = 0.0;
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL) {
        set_exp_log_power_unsupported_error(database);
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (expression->kind == MYLITE_SQL_AST_LN_FUNCTION && child_count == 1U) {
        function_name = "LN";
        log_base = 0.0;
    } else if (
        expression->kind == MYLITE_SQL_AST_LOG_FUNCTION && (child_count == 1U || child_count == 2U)
    ) {
        function_name = "LOG";
        log_base = 0.0;
    } else if (expression->kind == MYLITE_SQL_AST_LOG10_FUNCTION && child_count == 1U) {
        function_name = "LOG10";
        log_base = logarithm_base_ten;
    } else if (expression->kind == MYLITE_SQL_AST_LOG2_FUNCTION && child_count == 1U) {
        function_name = "LOG2";
        log_base = logarithm_base_two;
    } else {
        set_exp_log_power_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_approximate_numeric_operand(
        database,
        child_at(expression, 0U),
        &value,
        set_exp_log_power_unsupported_error
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (child_count == 2U) {
        return two_argument_logarithm_function_value(database, expression, &value, out_cell);
    }
    return one_argument_logarithm_function_value(
        database,
        &value,
        function_name,
        log_base,
        out_cell
    );
}

static int one_argument_logarithm_function_value(
    struct mylite_db *database,
    const struct approximate_numeric_input_value *value,
    const char *function_name,
    double log_base,
    struct session_scalar_cell *out_cell
) {
    size_t division_warning_count = 0U;
    double input = 0.0;
    double output = 0.0;
    int rc = MYLITE_OK;

    if (value == NULL || function_name == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    rc = accumulate_staged_warning_count(
        database,
        value->division_by_zero_warning_count,
        &division_warning_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value->is_null) {
        out_cell->staged_division_by_zero_warning_count = division_warning_count;
        return MYLITE_OK;
    }

    input = approximate_numeric_input_to_double(value);
    if (input <= 0.0) {
        out_cell->staged_division_by_zero_warning_count = division_warning_count;
        out_cell->staged_invalid_logarithm_warning_count = 1U;
        return MYLITE_OK;
    }
    if (log_base == 0.0) {
        output = log(input);
    } else {
        output = log(input) / log(log_base);
    }
    return finish_exp_log_power_function_value(
        database,
        output,
        function_name,
        division_warning_count,
        out_cell
    );
}

static int two_argument_logarithm_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct approximate_numeric_input_value *base,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value value = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    size_t division_warning_count = 0U;
    double base_input = 0.0;
    double value_input = 0.0;
    double output = 0.0;
    int rc = MYLITE_OK;

    if (expression == NULL || base == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    rc = accumulate_staged_warning_count(
        database,
        base->division_by_zero_warning_count,
        &division_warning_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (base->is_null) {
        out_cell->staged_division_by_zero_warning_count = division_warning_count;
        return MYLITE_OK;
    }

    base_input = approximate_numeric_input_to_double(base);
    if (base_input <= 0.0 || base_input == 1.0) {
        out_cell->staged_division_by_zero_warning_count = division_warning_count;
        out_cell->staged_invalid_logarithm_warning_count = 1U;
        return MYLITE_OK;
    }

    rc = evaluate_approximate_numeric_operand(
        database,
        child_at(expression, 1U),
        &value,
        set_exp_log_power_unsupported_error
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = accumulate_staged_warning_count(
        database,
        value.division_by_zero_warning_count,
        &division_warning_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value.is_null) {
        out_cell->staged_division_by_zero_warning_count = division_warning_count;
        return MYLITE_OK;
    }

    value_input = approximate_numeric_input_to_double(&value);
    if (value_input <= 0.0) {
        out_cell->staged_division_by_zero_warning_count = division_warning_count;
        out_cell->staged_invalid_logarithm_warning_count = 1U;
        return MYLITE_OK;
    }

    output = log(value_input) / log(base_input);
    return finish_exp_log_power_function_value(
        database,
        output,
        "LOG",
        division_warning_count,
        out_cell
    );
}

static int power_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value value = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    struct approximate_numeric_input_value exponent = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    const char *function_name = NULL;
    size_t division_warning_count = 0U;
    double output = 0.0;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL ||
        (expression->kind != MYLITE_SQL_AST_POW_FUNCTION &&
         expression->kind != MYLITE_SQL_AST_POWER_FUNCTION) ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_exp_log_power_unsupported_error(database);
        return MYLITE_ERROR;
    }

    function_name = expression->kind == MYLITE_SQL_AST_POW_FUNCTION ? "POW" : "POWER";
    rc = evaluate_approximate_numeric_operand(
        database,
        child_at(expression, 0U),
        &value,
        set_exp_log_power_unsupported_error
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_approximate_numeric_operand(
            database,
            child_at(expression, 1U),
            &exponent,
            set_exp_log_power_unsupported_error
        );
    }
    if (rc == MYLITE_OK) {
        rc = accumulate_staged_warning_count(
            database,
            value.division_by_zero_warning_count,
            &division_warning_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = accumulate_staged_warning_count(
            database,
            exponent.division_by_zero_warning_count,
            &division_warning_count
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value.is_null || exponent.is_null) {
        out_cell->staged_division_by_zero_warning_count = division_warning_count;
        return MYLITE_OK;
    }

    output =
        pow(approximate_numeric_input_to_double(&value),
            approximate_numeric_input_to_double(&exponent));
    if (!isfinite(output)) {
        return set_double_out_of_range_error(database, function_name);
    }
    return finish_exp_log_power_function_value(
        database,
        output,
        function_name,
        division_warning_count,
        out_cell
    );
}

static int finish_exp_log_power_function_value(
    struct mylite_db *database,
    double output,
    const char *function_name,
    size_t division_warning_count,
    struct session_scalar_cell *out_cell
) {
    int rc = MYLITE_OK;

    if (out_cell == NULL || function_name == NULL) {
        return MYLITE_MISUSE;
    }
    rc = format_double_text(
        database,
        output,
        function_name,
        out_cell->double_text,
        sizeof(out_cell->double_text)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_cell->value = out_cell->double_text;
    out_cell->staged_division_by_zero_warning_count = division_warning_count;
    return MYLITE_OK;
}

static int set_double_out_of_range_error(struct mylite_db *database, const char *function_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "DOUBLE value is out of range in '%s()'",
        function_name == NULL ? "math" : function_name
    );

    if (written < 0 || (size_t)written >= sizeof(message)) {
        set_runtime_error(database, "DOUBLE value is out of range");
        return MYLITE_ERROR;
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_bigint_out_of_range,
        "22003",
        message
    );
    return MYLITE_ERROR;
}

static double approximate_numeric_input_to_double(
    const struct approximate_numeric_input_value *value
) {
    double input = 0.0;

    if (value == NULL) {
        return 0.0;
    }

    input = (double)value->magnitude;
    if (value->is_negative) {
        input = -input;
    }
    return input;
}

static int evaluate_approximate_numeric_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct approximate_numeric_input_value *out_value,
    void (*set_unsupported_error)(struct mylite_db *)
) {
    struct scalar_arithmetic_value arithmetic = {.is_null = false, .integer = 0};
    bool handled = false;
    int rc = MYLITE_OK;

    if (out_value == NULL || set_unsupported_error == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct approximate_numeric_input_value){.is_null = false, .is_negative = false};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_approximate_numeric_direct_value_operand(
        database,
        expression,
        out_value,
        set_unsupported_error,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (is_scalar_bitwise_projection_expression(expression)) {
        struct scalar_bitwise_value bitwise = {.is_null = false, .integer = 0U};

        rc = evaluate_scalar_bitwise_expression(database, expression, &bitwise);
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_value->is_null = bitwise.is_null;
        out_value->magnitude = bitwise.integer;
        out_value->division_by_zero_warning_count = bitwise.division_by_zero_warning_count;
        return MYLITE_OK;
    }
    if (!is_scalar_arithmetic_projection_expression(expression)) {
        set_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_scalar_arithmetic_expression(database, expression, &arithmetic);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_value->is_null = arithmetic.is_null;
    out_value->division_by_zero_warning_count = arithmetic.division_by_zero_warning_count;
    if (arithmetic.is_null) {
        return MYLITE_OK;
    }
    if (arithmetic.integer < 0) {
        out_value->is_negative = true;
        out_value->magnitude = arithmetic.integer == INT64_MIN ? ((uint64_t)INT64_MAX + 1U)
                                                               : (uint64_t)(-arithmetic.integer);
    } else {
        out_value->magnitude = (uint64_t)arithmetic.integer;
    }
    return MYLITE_OK;
}

static int evaluate_approximate_numeric_direct_value_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct approximate_numeric_input_value *out_value,
    void (*set_unsupported_error)(struct mylite_db *),
    bool *out_handled
) {
    const struct mylite_sql_ast_node *literal = expression;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;
    bool has_sign = false;
    uint64_t magnitude = 0U;

    if (out_value == NULL || set_unsupported_error == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (expression == NULL) {
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return MYLITE_OK;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            return MYLITE_OK;
        }
        has_sign = true;
        is_negative = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_OK;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_value->is_null = true;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        out_value->magnitude = 1U;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_value->magnitude = 0U;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return MYLITE_OK;
    }
    *out_handled = true;

    if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        set_unsupported_error(database);
        return MYLITE_ERROR;
    }
    out_value->is_negative = false;
    if (is_negative && magnitude != 0U) {
        out_value->is_negative = true;
    }
    out_value->magnitude = magnitude;
    return MYLITE_OK;
}

static int format_double_text(
    struct mylite_db *database,
    double value,
    const char *function_name,
    char *buffer,
    size_t buffer_size
) {
    if (function_name == NULL || buffer == NULL || buffer_size == 0U) {
        return MYLITE_MISUSE;
    }

    if (should_format_scientific_integer_double(value)) {
        return format_scientific_double_text(database, value, function_name, buffer, buffer_size);
    }

    for (int precision = 1; precision <= double_text_max_significant_digits; ++precision) {
        char candidate[double_text_capacity];
        char *end = NULL;
        double parsed = 0.0;
        int written = format_c_locale_text(candidate, sizeof(candidate), "%.*g", precision, value);

        if (written < 0 || (size_t)written >= sizeof(candidate)) {
            set_double_format_error(database, function_name);
            return MYLITE_ERROR;
        }
        if (fabs(value) >= 1.0 && fabs(value) < double_scientific_integer_threshold &&
            (strchr(candidate, 'e') != NULL || strchr(candidate, 'E') != NULL)) {
            continue;
        }
        if (parse_c_locale_double(candidate, &end, &parsed) == MYLITE_OK && end != candidate &&
            end != NULL && *end == '\0' && parsed == value) {
            return copy_normalized_double_text(
                database,
                candidate,
                function_name,
                buffer,
                buffer_size
            );
        }
    }

    set_double_format_error(database, function_name);
    return MYLITE_ERROR;
}

static bool should_format_scientific_integer_double(double value) {
    double magnitude = fabs(value);

    if (magnitude < double_scientific_integer_threshold) {
        return false;
    }
    return trunc(value) == value;
}

static int format_scientific_double_text(
    struct mylite_db *database,
    double value,
    const char *function_name,
    char *buffer,
    size_t buffer_size
) {
    if (function_name == NULL || buffer == NULL || buffer_size == 0U) {
        return MYLITE_MISUSE;
    }

    for (int precision = 0; precision < double_text_max_significant_digits; ++precision) {
        char candidate[double_text_capacity];
        char *end = NULL;
        double parsed = 0.0;
        int written = format_c_locale_text(candidate, sizeof(candidate), "%.*e", precision, value);

        if (written < 0 || (size_t)written >= sizeof(candidate)) {
            set_double_format_error(database, function_name);
            return MYLITE_ERROR;
        }
        if (parse_c_locale_double(candidate, &end, &parsed) == MYLITE_OK && end != candidate &&
            end != NULL && *end == '\0' && parsed == value) {
            return copy_normalized_scientific_double_text(
                database,
                candidate,
                function_name,
                buffer,
                buffer_size
            );
        }
    }

    set_double_format_error(database, function_name);
    return MYLITE_ERROR;
}

static int format_c_locale_text(char *buffer, size_t buffer_size, const char *format, ...) {
    va_list arguments;
    int written = 0;

    if (buffer == NULL || buffer_size == 0U || format == NULL) {
        return -1;
    }

    va_start(arguments, format);
#ifdef _WIN32
    _locale_t c_locale = _create_locale(LC_NUMERIC, "C");

    if (c_locale == NULL) {
        va_end(arguments);
        return -1;
    }
    written = _vsnprintf_l(buffer, buffer_size, format, c_locale, arguments);
    _free_locale(c_locale);
#else
    locale_t c_locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    locale_t previous_locale = (locale_t)0;

    if (c_locale == (locale_t)0) {
        va_end(arguments);
        return -1;
    }
    previous_locale = uselocale(c_locale);
    if (previous_locale == (locale_t)0) {
        freelocale(c_locale);
        va_end(arguments);
        return -1;
    }
    written = vsnprintf(buffer, buffer_size, format, arguments);
    if (uselocale(previous_locale) == (locale_t)0) {
        written = -1;
    }
    freelocale(c_locale);
#endif
    va_end(arguments);
    return written;
}

static int parse_c_locale_double(const char *text, char **out_end, double *out_value) {
    if (text == NULL || out_end == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

#ifdef _WIN32
    _locale_t c_locale = _create_locale(LC_NUMERIC, "C");

    if (c_locale == NULL) {
        return MYLITE_ERROR;
    }
    *out_value = _strtod_l(text, out_end, c_locale);
    _free_locale(c_locale);
#else
    locale_t c_locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    locale_t previous_locale = (locale_t)0;
    int rc = MYLITE_OK;

    if (c_locale == (locale_t)0) {
        return MYLITE_ERROR;
    }
    previous_locale = uselocale(c_locale);
    if (previous_locale == (locale_t)0) {
        freelocale(c_locale);
        return MYLITE_ERROR;
    }
    *out_value = strtod(text, out_end);
    if (uselocale(previous_locale) == (locale_t)0) {
        rc = MYLITE_ERROR;
    }
    freelocale(c_locale);
    if (rc != MYLITE_OK) {
        return rc;
    }
#endif

    return MYLITE_OK;
}

static int copy_normalized_double_text(
    struct mylite_db *database,
    const char *candidate,
    const char *function_name,
    char *buffer,
    size_t buffer_size
) {
    size_t write_index = 0U;

    if (candidate == NULL || function_name == NULL || buffer == NULL || buffer_size == 0U) {
        return MYLITE_MISUSE;
    }
    for (size_t read_index = 0U; candidate[read_index] != '\0'; ++read_index) {
        if (write_index + 1U >= buffer_size) {
            set_double_format_error(database, function_name);
            return MYLITE_ERROR;
        }
        buffer[write_index] = candidate[read_index];
        ++write_index;
        if ((candidate[read_index] == 'e' || candidate[read_index] == 'E') &&
            candidate[read_index + 1U] == '+') {
            ++read_index;
        }
    }
    buffer[write_index] = '\0';
    return MYLITE_OK;
}

static int copy_normalized_scientific_double_text(
    struct mylite_db *database,
    const char *candidate,
    const char *function_name,
    char *buffer,
    size_t buffer_size
) {
    const char *exponent = NULL;
    size_t mantissa_end = 0U;
    size_t write_index = 0U;
    size_t exponent_index = 0U;

    if (candidate == NULL || function_name == NULL || buffer == NULL || buffer_size == 0U) {
        return MYLITE_MISUSE;
    }
    exponent = strchr(candidate, 'e');
    if (exponent == NULL) {
        exponent = strchr(candidate, 'E');
    }
    if (exponent == NULL) {
        set_double_format_error(database, function_name);
        return MYLITE_ERROR;
    }

    mantissa_end = (size_t)(exponent - candidate);
    while (mantissa_end > 0U && candidate[mantissa_end - 1U] == '0') {
        --mantissa_end;
    }
    if (mantissa_end > 0U && candidate[mantissa_end - 1U] == '.') {
        --mantissa_end;
    }
    for (size_t read_index = 0U; read_index < mantissa_end; ++read_index) {
        if (write_index + 1U >= buffer_size) {
            set_double_format_error(database, function_name);
            return MYLITE_ERROR;
        }
        buffer[write_index] = candidate[read_index];
        ++write_index;
    }

    if (write_index + 1U >= buffer_size) {
        set_double_format_error(database, function_name);
        return MYLITE_ERROR;
    }
    buffer[write_index] = 'e';
    ++write_index;

    exponent_index = (size_t)(exponent - candidate) + 1U;
    if (candidate[exponent_index] == '+') {
        ++exponent_index;
    } else if (candidate[exponent_index] == '-') {
        if (write_index + 1U >= buffer_size) {
            set_double_format_error(database, function_name);
            return MYLITE_ERROR;
        }
        buffer[write_index] = '-';
        ++write_index;
        ++exponent_index;
    }
    while (candidate[exponent_index] == '0' && candidate[exponent_index + 1U] != '\0') {
        ++exponent_index;
    }
    while (candidate[exponent_index] != '\0') {
        if (write_index + 1U >= buffer_size) {
            set_double_format_error(database, function_name);
            return MYLITE_ERROR;
        }
        buffer[write_index] = candidate[exponent_index];
        ++write_index;
        ++exponent_index;
    }
    buffer[write_index] = '\0';
    return MYLITE_OK;
}

static void set_double_format_error(struct mylite_db *database, const char *function_name) {
    char message[double_format_error_capacity];
    int written = snprintf(
        message,
        sizeof(message),
        "failed to format %s() value",
        function_name == NULL ? "scalar double" : function_name
    );

    if (written < 0 || (size_t)written >= sizeof(message)) {
        set_runtime_error(database, "failed to format scalar double value");
        return;
    }
    set_runtime_error(database, message);
}

static int base_conversion_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct scalar_bitwise_value value = {.is_null = false, .integer = 0U};
    unsigned int base = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || mylite_sql_ast_node_child_count(expression) != 1U) {
        set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_BIN_FUNCTION) {
        base = base_conversion_binary_base;
    } else if (expression->kind == MYLITE_SQL_AST_OCT_FUNCTION) {
        base = base_conversion_octal_base;
    } else {
        set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_base_conversion_operand(database, child_at(expression, 0U), &value);
    if (rc != MYLITE_OK || value.is_null) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    rc = format_base_conversion_value(
        database,
        value.integer,
        base,
        out_cell->base_conversion_text,
        sizeof(out_cell->base_conversion_text)
    );
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->base_conversion_text;
        out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
    }
    return rc;
}

static int conv_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct conv_arguments arguments;
    struct conv_output_format output = {0};
    uint64_t converted = 0U;
    unsigned int input_base = 0U;
    unsigned int output_base = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONV_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 3U) {
        set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_conv_arguments(database, expression, out_cell, &arguments, &is_null);
    if (rc != MYLITE_OK || is_null) {
        return rc;
    }

    input_base = absolute_conv_base(arguments.from_base.integer);
    output_base = absolute_conv_base(arguments.to_base.integer);
    if (input_base < base_conversion_binary_base || input_base > base_conversion_max_base ||
        output_base < base_conversion_binary_base || output_base > base_conversion_max_base) {
        return MYLITE_OK;
    }

    rc = convert_conv_value(
        database,
        &arguments.input,
        input_base,
        arguments.from_base.integer < 0,
        out_cell,
        &converted
    );
    if (rc == MYLITE_OK) {
        output.value = converted;
        output.base = output_base;
        output.signed_output = arguments.to_base.integer < 0;
        rc = format_conv_output_value(
            database,
            output,
            out_cell->base_conversion_text,
            sizeof(out_cell->base_conversion_text)
        );
    }
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->base_conversion_text;
    }
    return rc;
}

static int evaluate_conv_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    struct conv_arguments *out_arguments,
    bool *out_is_null
) {
    int rc = MYLITE_OK;

    if (expression == NULL || cell == NULL || out_arguments == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    out_arguments->input = (struct conv_input_value){
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    out_arguments->from_base = (struct scalar_arithmetic_value){
        .is_null = false,
        .integer = 0,
        .division_by_zero_warning_count = 0U,
    };
    out_arguments->to_base = (struct scalar_arithmetic_value){
        .is_null = false,
        .integer = 0,
        .division_by_zero_warning_count = 0U,
    };
    *out_is_null = false;

    rc = evaluate_conv_value_operand(database, child_at(expression, 0U), &out_arguments->input);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_arguments->input.is_null) {
        rc = set_conv_staged_division_by_zero_count(database, out_arguments, false, false, cell);
        *out_is_null = true;
        return rc;
    }

    rc = evaluate_conv_base_operand(database, child_at(expression, 1U), &out_arguments->from_base);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_arguments->from_base.is_null) {
        rc = set_conv_staged_division_by_zero_count(database, out_arguments, true, false, cell);
        *out_is_null = true;
        return rc;
    }

    rc = evaluate_conv_base_operand(database, child_at(expression, 2U), &out_arguments->to_base);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_arguments->to_base.is_null) {
        rc = set_conv_staged_division_by_zero_count(database, out_arguments, true, true, cell);
        *out_is_null = true;
        return rc;
    }

    return set_conv_staged_division_by_zero_count(database, out_arguments, true, true, cell);
}

static int set_conv_staged_division_by_zero_count(
    struct mylite_db *database,
    const struct conv_arguments *arguments,
    bool include_from_base,
    bool include_to_base,
    struct session_scalar_cell *cell
) {
    size_t warning_count = 0U;
    int rc = MYLITE_OK;

    if (arguments == NULL || cell == NULL) {
        return MYLITE_MISUSE;
    }

    rc = accumulate_staged_division_by_zero_warnings(
        database,
        arguments->input.division_by_zero_warning_count,
        &warning_count
    );
    if (rc == MYLITE_OK && include_from_base) {
        rc = accumulate_staged_division_by_zero_warnings(
            database,
            arguments->from_base.division_by_zero_warning_count,
            &warning_count
        );
    }
    if (rc == MYLITE_OK && include_to_base) {
        rc = accumulate_staged_division_by_zero_warnings(
            database,
            arguments->to_base.division_by_zero_warning_count,
            &warning_count
        );
    }
    if (rc == MYLITE_OK) {
        cell->staged_division_by_zero_warning_count = warning_count;
    }
    return rc;
}

static unsigned int absolute_conv_base(int64_t base) {
    if (base == INT64_MIN) {
        return base_conversion_max_base + 1U;
    }
    if (base < 0) {
        return (unsigned int)(-base);
    }
    return (unsigned int)base;
}

static int evaluate_conv_value_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct conv_input_value *out_value
) {
    struct scalar_arithmetic_value arithmetic = {.is_null = false, .integer = 0};
    bool handled = false;
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct conv_input_value){.is_null = false, .is_negative = false, .magnitude = 0U};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_conv_direct_value_operand(database, expression, out_value, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (is_scalar_bitwise_projection_expression(expression)) {
        struct scalar_bitwise_value bitwise = {.is_null = false, .integer = 0U};

        rc = evaluate_scalar_bitwise_expression(database, expression, &bitwise);
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_value->is_null = bitwise.is_null;
        out_value->magnitude = bitwise.integer;
        out_value->division_by_zero_warning_count = bitwise.division_by_zero_warning_count;
        return MYLITE_OK;
    }
    if (!is_scalar_arithmetic_projection_expression(expression)) {
        set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_scalar_arithmetic_expression(database, expression, &arithmetic);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_value->is_null = arithmetic.is_null;
    out_value->division_by_zero_warning_count = arithmetic.division_by_zero_warning_count;
    if (arithmetic.is_null) {
        return MYLITE_OK;
    }
    if (arithmetic.integer < 0) {
        out_value->is_negative = true;
        out_value->magnitude = arithmetic.integer == INT64_MIN ? ((uint64_t)INT64_MAX + 1U)
                                                               : (uint64_t)(-arithmetic.integer);
    } else {
        out_value->magnitude = (uint64_t)arithmetic.integer;
    }
    return MYLITE_OK;
}

static int evaluate_conv_direct_value_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct conv_input_value *out_value,
    bool *out_handled
) {
    static const uint64_t int64_min_magnitude = 9223372036854775808ULL;
    const struct mylite_sql_ast_node *literal = expression;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;
    bool has_sign = false;
    uint64_t magnitude = 0U;

    if (out_value == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (expression == NULL) {
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return MYLITE_OK;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            return MYLITE_OK;
        }
        has_sign = true;
        is_negative = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_OK;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_value->is_null = true;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        out_value->magnitude = 1U;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_value->magnitude = 0U;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return MYLITE_OK;
    }
    *out_handled = true;

    if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (is_negative && magnitude > int64_min_magnitude) {
        set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }
    out_value->is_negative = false;
    if (is_negative && magnitude != 0U) {
        out_value->is_negative = true;
    }
    out_value->magnitude = magnitude;
    return MYLITE_OK;
}

static int evaluate_conv_base_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){.is_null = false, .integer = 0};
    if (expression == NULL || !is_scalar_arithmetic_projection_expression(expression)) {
        set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }
    return evaluate_scalar_arithmetic_expression(database, expression, out_value);
}

static int convert_conv_value(
    struct mylite_db *database,
    const struct conv_input_value *input,
    unsigned int from_base,
    bool signed_input,
    struct session_scalar_cell *cell,
    uint64_t *out_value
) {
    uint64_t limit = UINT64_MAX;
    struct conv_digit_parse parse = {0};
    char input_text[integer_text_capacity];
    int rc = MYLITE_OK;

    if (input == NULL || cell == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (signed_input) {
        limit = (uint64_t)INT64_MAX;
        if (input->is_negative) {
            limit = (uint64_t)INT64_MAX + 1U;
        }
    }
    rc = format_conv_input_text(database, input, input_text, sizeof(input_text));
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = parse_conv_input_digits(input_text, from_base, limit, &parse);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!parse.saw_digit || parse.overflowed) {
        rc = stage_conv_truncated_decimal_warning(database, input_text, cell);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    if (!parse.saw_digit) {
        *out_value = 0U;
    } else if (input->is_negative) {
        *out_value = 0U - parse.value;
    } else {
        *out_value = parse.value;
    }
    return MYLITE_OK;
}

static int parse_conv_input_digits(
    const char *input_text,
    unsigned int from_base,
    uint64_t limit,
    struct conv_digit_parse *out_parse
) {
    struct conv_digit_parse parse = {0};
    size_t offset = 0U;

    if (input_text == NULL || out_parse == NULL) {
        return MYLITE_MISUSE;
    }
    if (input_text[offset] == '-') {
        ++offset;
    }
    while (input_text[offset] != '\0') {
        unsigned char byte = (unsigned char)input_text[offset];
        uint64_t digit = 0U;

        if (byte < '0' || byte > '9') {
            break;
        }
        digit = (uint64_t)(byte - '0');
        if (digit >= (uint64_t)from_base) {
            break;
        }
        parse.saw_digit = true;
        if (!parse.overflowed) {
            if (parse.value > (limit - digit) / (uint64_t)from_base) {
                parse.value = limit;
                parse.overflowed = true;
            } else {
                parse.value = (parse.value * (uint64_t)from_base) + digit;
            }
        }
        ++offset;
    }
    *out_parse = parse;
    return MYLITE_OK;
}

static int format_conv_input_text(
    struct mylite_db *database,
    const struct conv_input_value *input,
    char *buffer,
    size_t buffer_size
) {
    int written = 0;

    if (input == NULL || buffer == NULL || buffer_size == 0U) {
        return MYLITE_MISUSE;
    }
    if (!input->is_negative) {
        return format_uint64(database, input->magnitude, buffer, buffer_size);
    }
    written = snprintf(buffer, buffer_size, "-%" PRIu64, input->magnitude);
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format CONV() input value");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int stage_conv_truncated_decimal_warning(
    struct mylite_db *database,
    const char *input_text,
    struct session_scalar_cell *cell
) {
    int written = 0;

    if (input_text == NULL || cell == NULL) {
        return MYLITE_MISUSE;
    }
    written = snprintf(
        cell->staged_truncated_decimal_text,
        sizeof(cell->staged_truncated_decimal_text),
        "%s",
        input_text
    );
    if (written < 0 || (size_t)written >= sizeof(cell->staged_truncated_decimal_text)) {
        set_runtime_error(database, "failed to stage CONV() warning");
        return MYLITE_ERROR;
    }
    cell->has_staged_truncated_decimal_warning = true;
    return MYLITE_OK;
}

static int format_conv_output_value(
    struct mylite_db *database,
    struct conv_output_format format,
    char *buffer,
    size_t buffer_size
) {
    uint64_t magnitude = format.value;
    bool is_negative = false;
    int rc = MYLITE_OK;

    if (buffer == NULL || buffer_size == 0U || format.base < base_conversion_binary_base ||
        format.base > base_conversion_max_base) {
        set_runtime_error(database, "failed to format CONV() value");
        return MYLITE_ERROR;
    }
    if (format.signed_output && format.value > (uint64_t)INT64_MAX) {
        is_negative = true;
        magnitude = 0U - format.value;
    }
    if (is_negative) {
        if (buffer_size < 2U) {
            set_runtime_error(database, "failed to format CONV() value");
            return MYLITE_ERROR;
        }
        buffer[0] = '-';
        rc = format_base_conversion_value(
            database,
            magnitude,
            format.base,
            buffer + 1U,
            buffer_size - 1U
        );
    } else {
        rc = format_base_conversion_value(database, magnitude, format.base, buffer, buffer_size);
    }
    return rc;
}

static int format_base_conversion_value(
    struct mylite_db *database,
    uint64_t value,
    unsigned int base,
    char *buffer,
    size_t buffer_size
) {
    static const char base_conversion_digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char reversed[base_conversion_text_capacity];
    size_t digit_count = 0U;

    if (buffer == NULL || buffer_size == 0U || base < base_conversion_binary_base ||
        base > base_conversion_max_base) {
        set_runtime_error(database, "failed to format base conversion value");
        return MYLITE_ERROR;
    }
    if (value == 0U) {
        if (buffer_size < 2U) {
            set_runtime_error(database, "failed to format base conversion value");
            return MYLITE_ERROR;
        }
        buffer[0] = '0';
        buffer[1] = '\0';
        return MYLITE_OK;
    }

    while (value != 0U) {
        unsigned int digit = (unsigned int)(value % base);

        if (digit_count == sizeof(reversed)) {
            set_runtime_error(database, "failed to format base conversion value");
            return MYLITE_ERROR;
        }
        reversed[digit_count] = base_conversion_digits[digit];
        ++digit_count;
        value /= base;
    }
    if (digit_count >= buffer_size) {
        set_runtime_error(database, "failed to format base conversion value");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; index < digit_count; ++index) {
        buffer[index] = reversed[digit_count - index - 1U];
    }
    buffer[digit_count] = '\0';
    return MYLITE_OK;
}

static int evaluate_base_conversion_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
) {
    struct scalar_arithmetic_value arithmetic = {.is_null = false, .integer = 0};
    bool handled = false;
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_bitwise_value){.is_null = false, .integer = 0U};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_base_conversion_direct_literal_operand(database, expression, out_value, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (is_scalar_bitwise_projection_expression(expression)) {
        return evaluate_scalar_bitwise_expression(database, expression, out_value);
    }
    if (!is_scalar_arithmetic_projection_expression(expression)) {
        set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_scalar_arithmetic_expression(database, expression, &arithmetic);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_value->is_null = arithmetic.is_null;
    out_value->integer = (uint64_t)arithmetic.integer;
    out_value->division_by_zero_warning_count = arithmetic.division_by_zero_warning_count;
    return MYLITE_OK;
}

static int evaluate_base_conversion_direct_literal_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value,
    bool *out_handled
) {
    static const uint64_t int64_min_magnitude = 9223372036854775808ULL;
    const struct mylite_sql_ast_node *literal = expression;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;
    bool has_sign = false;
    uint64_t magnitude = 0U;

    if (out_value == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (expression == NULL) {
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return MYLITE_OK;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            return MYLITE_OK;
        }
        has_sign = true;
        is_negative = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_OK;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_value->is_null = true;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        out_value->integer = 1U;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_value->integer = 0U;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return MYLITE_OK;
    }
    *out_handled = true;

    if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (!is_negative) {
        out_value->integer = magnitude;
        return MYLITE_OK;
    }
    if (magnitude > int64_min_magnitude) {
        set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (magnitude == int64_min_magnitude) {
        out_value->integer = (uint64_t)INT64_MIN;
    } else if (magnitude != 0U) {
        out_value->integer = (uint64_t)(-(int64_t)magnitude);
    }
    return MYLITE_OK;
}

static int bit_count_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct scalar_bitwise_value value = {.is_null = false, .integer = 0U};
    uint64_t bit_count = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || mylite_sql_ast_node_child_count(expression) != 1U) {
        set_bit_count_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_bit_count_operand(database, child_at(expression, 0U), &value);
    if (rc != MYLITE_OK || value.is_null) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    bit_count = count_uint64_bits(value.integer);
    rc = format_uint64(database, bit_count, out_cell->integer_text, sizeof(out_cell->integer_text));
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->integer_text;
        out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
    }
    return rc;
}

static int crc32_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const unsigned char *bytes = NULL;
    char *owned_bytes = NULL;
    size_t byte_count = 0U;
    uint32_t checksum = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CRC32_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_crc32_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = crc32_argument_bytes(
        database,
        child_at(expression, 0U),
        out_cell,
        &bytes,
        &byte_count,
        &owned_bytes
    );
    if (rc != MYLITE_OK || bytes == NULL) {
        free(owned_bytes);
        return rc;
    }

    checksum = crc32_checksum(bytes, byte_count);
    free(owned_bytes);
    rc = format_uint64(
        database,
        (uint64_t)checksum,
        out_cell->integer_text,
        sizeof(out_cell->integer_text)
    );
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->integer_text;
    }
    return rc;
}

static int hex_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *argument = NULL;
    struct session_scalar_cell argument_cell = {0};
    const unsigned char *bytes = NULL;
    char *owned_bytes = NULL;
    size_t byte_count = 0U;
    struct scalar_bitwise_value numeric_value = {.is_null = false, .integer = 0U};
    bool handled_numeric = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_HEX_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_hex_unsupported_error(database);
        return MYLITE_ERROR;
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    rc = evaluate_base_conversion_direct_literal_operand(
        database,
        argument,
        &numeric_value,
        &handled_numeric
    );
    if (rc == MYLITE_OK && !handled_numeric) {
        rc =
            hex_numeric_scalar_argument_value(database, argument, &numeric_value, &handled_numeric);
    }
    if (rc != MYLITE_OK || handled_numeric) {
        if (rc == MYLITE_OK && !numeric_value.is_null) {
            rc = format_base_conversion_value(
                database,
                numeric_value.integer,
                base_conversion_hexadecimal_base,
                out_cell->base_conversion_text,
                sizeof(out_cell->base_conversion_text)
            );
            if (rc == MYLITE_OK) {
                out_cell->value = out_cell->base_conversion_text;
            }
        }
        return rc;
    }

    rc = hex_argument_bytes(database, argument, &argument_cell, &bytes, &byte_count, &owned_bytes);
    if (rc != MYLITE_OK || bytes == NULL) {
        free(owned_bytes);
        session_scalar_cell_deinit(&argument_cell);
        return rc;
    }

    rc = format_hex_bytes(database, bytes, byte_count, &out_cell->owned_text);
    free(owned_bytes);
    session_scalar_cell_deinit(&argument_cell);
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->owned_text;
    }
    return rc;
}

static int weight_string_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *argument = NULL;
    struct session_scalar_cell argument_cell = {0};
    const unsigned char *bytes = NULL;
    unsigned char *result = NULL;
    char *owned_bytes = NULL;
    size_t byte_count = 0U;
    size_t result_size = 0U;
    bool is_null = false;
    int64_t binary_length = 0;
    bool has_binary_length = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || (expression->kind != MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION)) {
        set_unsupported_error(database, "WEIGHT_STRING() expression is unsupported");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION &&
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(database, "WEIGHT_STRING() expression is unsupported");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION &&
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_unsupported_error(database, "WEIGHT_STRING() AS BINARY expression is unsupported");
        return MYLITE_ERROR;
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    rc = weight_string_argument_bytes(
        database,
        argument,
        &argument_cell,
        &bytes,
        &byte_count,
        &owned_bytes,
        &is_null
    );
    if (rc == MYLITE_OK && expression->kind == MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION) {
        has_binary_length = true;
        rc = weight_string_binary_length_value(database, child_at(expression, 1U), &binary_length);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_weight_string_value(
            database,
            bytes,
            byte_count,
            is_null,
            has_binary_length,
            binary_length,
            &result,
            &result_size,
            &is_null
        );
    }
    free(owned_bytes);
    session_scalar_cell_deinit(&argument_cell);
    if (rc != MYLITE_OK || is_null) {
        free(result);
        return rc;
    }

    out_cell->owned_text = (char *)result;
    out_cell->value = out_cell->owned_text;
    out_cell->value_size = result_size;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

static int weight_string_binary_length_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_length
) {
    uint64_t length = 0U;

    if (out_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_length = 0;
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_INTEGER ||
        parse_unsigned_integer_literal(&expression->span, &length) != MYLITE_OK ||
        length > (uint64_t)INT64_MAX) {
        set_unsupported_error(database, "WEIGHT_STRING() AS BINARY length must fit signed 64-bit");
        return MYLITE_ERROR;
    }
    *out_length = (int64_t)length;
    return MYLITE_OK;
}

static int weight_string_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
) {
    bool handled_scalar = false;
    int rc = MYLITE_OK;

    if (cell == NULL || out_bytes == NULL || out_byte_count == NULL || out_owned_bytes == NULL ||
        out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_bytes = NULL;
    *out_byte_count = 0U;
    *out_owned_bytes = NULL;
    *out_is_null = false;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_base64_argument_unsupported_error(database, "WEIGHT_STRING");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return base64_literal_argument_bytes(
            database,
            expression,
            "WEIGHT_STRING",
            out_bytes,
            out_byte_count,
            out_owned_bytes,
            out_is_null
        );
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return base64_unary_argument_bytes(
            database,
            expression,
            "WEIGHT_STRING",
            out_bytes,
            out_byte_count,
            out_owned_bytes,
            out_is_null
        );
    }

    rc = hex_non_weight_string_scalar_argument_value(database, expression, cell, &handled_scalar);
    if (rc != MYLITE_OK || handled_scalar) {
        if (rc == MYLITE_OK && cell->value == NULL) {
            *out_is_null = true;
        } else if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)cell->value;
            *out_byte_count = cell->has_value_size ? cell->value_size : strlen(cell->value);
        }
        return rc;
    }

    set_base64_argument_unsupported_error(database, "WEIGHT_STRING");
    return MYLITE_ERROR;
}

static int unhex_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *argument = NULL;
    const unsigned char *bytes = NULL;
    unsigned char *decoded = NULL;
    char *owned_bytes = NULL;
    size_t byte_count = 0U;
    size_t decoded_size = 0U;
    bool is_null = false;
    bool valid = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_UNHEX_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(
            database,
            "UNHEX() supports only one string, hex, integer, boolean, NULL, supported session "
            "scalar, or supported system variable argument"
        );
        return MYLITE_ERROR;
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    rc = unhex_argument_bytes(
        database,
        argument,
        out_cell,
        &bytes,
        &byte_count,
        &owned_bytes,
        &is_null
    );
    if (rc != MYLITE_OK || is_null) {
        free(owned_bytes);
        return rc;
    }

    rc = mylite_string_unhex_decode(bytes, byte_count, &decoded, &decoded_size, &valid);
    if (rc != MYLITE_OK) {
        free(owned_bytes);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
        return rc;
    }
    if (!valid) {
        rc = stage_unhex_incorrect_string_warning(database, out_cell, bytes, byte_count);
        free(owned_bytes);
        return rc;
    }

    out_cell->owned_text = (char *)decoded;
    out_cell->value = out_cell->owned_text;
    out_cell->value_size = decoded_size;
    out_cell->has_value_size = true;
    free(owned_bytes);
    return MYLITE_OK;
}

static int to_base64_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *argument = NULL;
    const unsigned char *bytes = NULL;
    char *encoded = NULL;
    char *owned_bytes = NULL;
    size_t byte_count = 0U;
    size_t encoded_size = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_TO_BASE64_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "TO_BASE64");
        return MYLITE_ERROR;
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    rc = base64_argument_bytes(
        database,
        argument,
        "TO_BASE64",
        out_cell,
        &bytes,
        &byte_count,
        &owned_bytes,
        &is_null
    );
    if (rc != MYLITE_OK || is_null) {
        free(owned_bytes);
        return rc;
    }

    rc = mylite_string_base64_encode(bytes, byte_count, &encoded, &encoded_size);
    free(owned_bytes);
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
        return rc;
    }

    out_cell->owned_text = encoded;
    out_cell->value = out_cell->owned_text;
    out_cell->value_size = encoded_size;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

static int from_base64_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *argument = NULL;
    const unsigned char *bytes = NULL;
    unsigned char *decoded = NULL;
    char *owned_bytes = NULL;
    size_t byte_count = 0U;
    size_t decoded_size = 0U;
    bool is_null = false;
    bool valid = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_FROM_BASE64_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "FROM_BASE64");
        return MYLITE_ERROR;
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    rc = base64_argument_bytes(
        database,
        argument,
        "FROM_BASE64",
        out_cell,
        &bytes,
        &byte_count,
        &owned_bytes,
        &is_null
    );
    if (rc != MYLITE_OK || is_null) {
        free(owned_bytes);
        return rc;
    }

    rc = mylite_string_base64_decode(bytes, byte_count, &decoded, &decoded_size, &valid);
    free(owned_bytes);
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
        return rc;
    }
    if (!valid) {
        return MYLITE_OK;
    }

    out_cell->owned_text = (char *)decoded;
    out_cell->value = out_cell->owned_text;
    out_cell->value_size = decoded_size;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

static int uuid_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_uuid_unsupported_error(database, "UUID conversion");
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_UUID_FUNCTION:
        return uuid_generate_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_IS_UUID_FUNCTION:
        return is_uuid_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION:
        return uuid_to_bin_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION:
        return bin_to_uuid_function_value(database, expression, out_cell);
    default:
        break;
    }

    set_uuid_unsupported_error(database, "UUID conversion");
    return MYLITE_ERROR;
}

static int uuid_generate_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    char uuid_text[MYLITE_UUID_TEXT_SIZE + 1U];
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_UUID_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 0U) {
        set_native_function_parameter_count_error(database, "UUID");
        return MYLITE_ERROR;
    }

    rc = mylite_uuid_generate(database, uuid_text);
    if (rc == MYLITE_OK) {
        rc = duplicate_text(database, uuid_text, &out_cell->owned_text);
    }
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->owned_text;
    }
    return rc;
}

static int is_uuid_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *argument = NULL;
    const unsigned char *bytes = NULL;
    char *owned_bytes = NULL;
    size_t byte_count = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_IS_UUID_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "IS_UUID");
        return MYLITE_ERROR;
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    rc = uuid_argument_bytes(
        database,
        argument,
        out_cell,
        &bytes,
        &byte_count,
        &owned_bytes,
        &is_null
    );
    if (rc != MYLITE_OK || is_null) {
        free(owned_bytes);
        return rc;
    }

    out_cell->value = "0";
    if (mylite_uuid_string_is_valid(bytes, byte_count)) {
        out_cell->value = "1";
    }
    free(owned_bytes);
    return MYLITE_OK;
}

static int uuid_to_bin_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *argument = NULL;
    const struct mylite_sql_ast_node *swap_argument = NULL;
    const unsigned char *bytes = NULL;
    unsigned char uuid_bytes[MYLITE_UUID_BINARY_SIZE];
    char *owned_bytes = NULL;
    size_t byte_count = 0U;
    bool is_null = false;
    bool swap = false;
    bool valid = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION ||
        (mylite_sql_ast_node_child_count(expression) != 1U &&
         mylite_sql_ast_node_child_count(expression) != 2U)) {
        set_native_function_parameter_count_error(database, "UUID_TO_BIN");
        return MYLITE_ERROR;
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    swap_argument = unwrap_parenthesized_expression(child_at(expression, 1U));
    rc = uuid_argument_bytes(
        database,
        argument,
        out_cell,
        &bytes,
        &byte_count,
        &owned_bytes,
        &is_null
    );
    if (rc == MYLITE_OK && swap_argument != NULL) {
        rc = uuid_swap_flag_value(database, swap_argument, &swap);
    }
    if (rc != MYLITE_OK || is_null) {
        free(owned_bytes);
        return rc;
    }

    rc = mylite_uuid_string_to_binary(bytes, byte_count, swap, uuid_bytes, &valid);
    if (rc != MYLITE_OK) {
        free(owned_bytes);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
        return rc;
    }
    if (!valid) {
        rc = mylite_uuid_set_incorrect_string_error(database, bytes, byte_count, "uuid_to_bin");
        free(owned_bytes);
        return rc;
    }

    out_cell->owned_text = (char *)malloc(MYLITE_UUID_BINARY_SIZE + 1U);
    if (out_cell->owned_text == NULL) {
        free(owned_bytes);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(out_cell->owned_text, uuid_bytes, MYLITE_UUID_BINARY_SIZE);
    out_cell->owned_text[MYLITE_UUID_BINARY_SIZE] = '\0';
    out_cell->value = out_cell->owned_text;
    out_cell->value_size = MYLITE_UUID_BINARY_SIZE;
    out_cell->has_value_size = true;
    free(owned_bytes);
    return MYLITE_OK;
}

static int bin_to_uuid_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *argument = NULL;
    const struct mylite_sql_ast_node *swap_argument = NULL;
    const unsigned char *bytes = NULL;
    char uuid_text[MYLITE_UUID_TEXT_SIZE + 1U];
    char *owned_bytes = NULL;
    size_t byte_count = 0U;
    bool is_null = false;
    bool swap = false;
    bool valid = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION ||
        (mylite_sql_ast_node_child_count(expression) != 1U &&
         mylite_sql_ast_node_child_count(expression) != 2U)) {
        set_native_function_parameter_count_error(database, "BIN_TO_UUID");
        return MYLITE_ERROR;
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    swap_argument = unwrap_parenthesized_expression(child_at(expression, 1U));
    rc = uuid_argument_bytes(
        database,
        argument,
        out_cell,
        &bytes,
        &byte_count,
        &owned_bytes,
        &is_null
    );
    if (rc == MYLITE_OK && swap_argument != NULL) {
        rc = uuid_swap_flag_value(database, swap_argument, &swap);
    }
    if (rc != MYLITE_OK || is_null) {
        free(owned_bytes);
        return rc;
    }

    rc = mylite_uuid_binary_to_string(bytes, byte_count, swap, uuid_text, &valid);
    if (rc != MYLITE_OK) {
        free(owned_bytes);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
        return rc;
    }
    if (!valid) {
        rc = mylite_uuid_set_incorrect_string_error(database, bytes, byte_count, "bin_to_uuid");
        free(owned_bytes);
        return rc;
    }

    rc = duplicate_text(database, uuid_text, &out_cell->owned_text);
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->owned_text;
    }
    free(owned_bytes);
    return rc;
}

static int char_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    struct mylite_string_char_buffer buffer = {0};
    char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CHAR_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(
            database,
            "CHAR() supports one or more integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }

    arguments = child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST ||
        mylite_sql_ast_node_child_count(arguments) == 0U) {
        set_unsupported_error(
            database,
            "CHAR() supports one or more integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }

    for (const struct mylite_sql_ast_node *argument = child_at(arguments, 0U);
         rc == MYLITE_OK && argument != NULL;
         argument = argument->next_sibling) {
        struct scalar_bitwise_value value = {.is_null = false, .integer = 0U};

        rc = char_function_argument_value(database, argument, &value);
        if (rc == MYLITE_OK && !value.is_null) {
            rc = mylite_string_char_buffer_append_uint64(&buffer, value.integer);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_string_char_buffer_finish(&buffer, &bytes, &byte_count);
    }
    mylite_string_char_buffer_deinit(&buffer);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_cell->owned_text = bytes;
    out_cell->value = out_cell->owned_text;
    out_cell->value_size = byte_count;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

static int char_function_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
) {
    const struct mylite_sql_ast_node *literal = NULL;
    bool is_negative = false;
    bool has_sign = false;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_bitwise_value){.is_null = false, .integer = 0U};
    expression = unwrap_parenthesized_expression(expression);
    literal = expression;
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            literal = NULL;
        } else {
            has_sign = true;
            is_negative = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
            literal = unwrap_parenthesized_expression(child_at(expression, 0U));
        }
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        if (expression != NULL && (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
                                   expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
            return char_function_unknown_column_argument(database, expression);
        }
        set_unsupported_error(
            database,
            "CHAR() supports only integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }

    return char_function_literal_value(database, literal, has_sign, is_negative, out_value);
}

static int char_function_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool has_sign,
    bool is_negative,
    struct scalar_bitwise_value *out_value
) {
    if (out_value == NULL || literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_MISUSE;
    }

    switch (mylite_sql_ast_node_literal_kind(literal)) {
    case MYLITE_SQL_AST_LITERAL_NULL:
        if (has_sign) {
            break;
        }
        out_value->is_null = true;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_TRUE:
        if (has_sign) {
            break;
        }
        out_value->integer = 1U;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_FALSE:
        if (has_sign) {
            break;
        }
        out_value->integer = 0U;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return char_function_integer_literal_value(database, literal, is_negative, out_value);
    default:
        break;
    }

    set_unsupported_error(database, "CHAR() supports only integer, boolean, and NULL arguments");
    return MYLITE_ERROR;
}

static int char_function_integer_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    struct scalar_bitwise_value *out_value
) {
    static const uint64_t int64_min_magnitude = 9223372036854775808ULL;
    uint64_t magnitude = 0U;

    if (literal == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        set_unsupported_error(
            database,
            "CHAR() integer literals must fit the unsigned 64-bit range"
        );
        return MYLITE_ERROR;
    }
    if (!is_negative) {
        out_value->integer = magnitude;
        return MYLITE_OK;
    }
    if (magnitude > int64_min_magnitude) {
        set_unsupported_error(
            database,
            "CHAR() signed integer literals must fit the signed 64-bit range"
        );
        return MYLITE_ERROR;
    }
    if (magnitude == int64_min_magnitude) {
        out_value->integer = (uint64_t)INT64_MIN;
    } else if (magnitude != 0U) {
        out_value->integer = (uint64_t)(-(int64_t)magnitude);
    }
    return MYLITE_OK;
}

static int char_function_unknown_column_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = collect_column_reference_parts(database, expression, parts, &part_count);

    if (rc == MYLITE_OK) {
        rc = format_column_reference_name(
            database,
            parts,
            part_count,
            column_name,
            sizeof(column_name)
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    set_unknown_column_error(database, column_name);
    return MYLITE_ERROR;
}

static int hex_numeric_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value,
    bool *out_handled
) {
    if (out_value == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_bitwise_value){.is_null = false, .integer = 0U};
    *out_handled = true;
    if (expression == NULL) {
        *out_handled = false;
        return MYLITE_OK;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
        out_value->integer = database->session.connection_id;
        return MYLITE_OK;
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
        out_value->integer = (uint64_t)database->session.previous_row_count;
        return MYLITE_OK;
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION:
        out_value->integer = database->session.found_rows;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
        out_value->integer = database->session.last_insert_id;
        return MYLITE_OK;
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        return hex_numeric_system_variable_value(database, expression, out_value, out_handled);
    default:
        break;
    }

    *out_handled = false;
    return MYLITE_OK;
}

static int hex_numeric_system_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value,
    bool *out_handled
) {
    enum mylite_execution_system_variable_kind variable = MYLITE_EXECUTION_SYSTEM_VARIABLE_NONE;
    int64_t timestamp = 0;
    int rc = MYLITE_OK;

    if (out_value == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_bitwise_value){.is_null = false, .integer = 0U};
    *out_handled = true;

    rc = resolve_session_system_variable(database, expression, &variable);
    if (rc != MYLITE_OK) {
        return rc;
    }

    switch (variable) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_INCREMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_OFFSET:
        out_value->integer = auto_increment_step_system_variable_value(
            database,
            variable,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WAIT_TIMEOUT:
        out_value->integer = timeout_system_variable_value(
            database,
            variable,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN:
        if (system_variable_expression_has_global_scope(expression)) {
            out_value->integer = group_concat_max_len_default_value;
        } else {
            out_value->integer = database->session.group_concat_max_len;
        }
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INFORMATION_SCHEMA_STATS_EXPIRY:
        if (system_variable_expression_has_global_scope(expression)) {
            out_value->integer = information_schema_stats_expiry_default_value;
        } else {
            out_value->integer = database->session.information_schema_stats_expiry;
        }
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS:
        out_value->integer = foreign_key_checks_system_variable_uint64_value(
            database,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BIG_TABLES:
        out_value->integer = big_tables_system_variable_uint64_value(
            database,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOCOMMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_QUOTE_SHOW_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_UNIQUE_CHECKS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_NOTES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BIG_SELECTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLICIT_DEFAULTS_FOR_TIMESTAMP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_BIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN:
        out_value->integer = 1U;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_WARNINGS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BUFFER_RESULT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_GENERATE_INVISIBLE_PRIMARY_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_LOG_OFF:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REPLICA_SKIP_COUNTER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REQUIRE_PRIMARY_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER:
        out_value->integer = 0U;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID:
        out_value->integer = server_id_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID_BITS:
        out_value->integer = server_id_bits_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PORT:
        out_value->integer = port_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_VERSION:
        out_value->integer = protocol_version_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SELECT_LIMIT:
        out_value->integer = sql_select_limit_system_variable_value(
            database,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TIMESTAMP:
        timestamp = current_timestamp_epoch(database);
        out_value->integer = (uint64_t)timestamp;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WARNING_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ERROR_COUNT:
        return diagnostics_count_system_variable_value(
            system_variable_count_diagnostics(database),
            variable,
            &out_value->integer
        );
    default:
        break;
    }

    *out_handled = false;
    return MYLITE_OK;
}

static int hex_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes
) {
    bool handled_scalar = false;
    int rc = MYLITE_OK;

    if (cell == NULL || out_bytes == NULL || out_byte_count == NULL || out_owned_bytes == NULL) {
        return MYLITE_MISUSE;
    }
    *out_bytes = NULL;
    *out_byte_count = 0U;
    *out_owned_bytes = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_hex_unsupported_error(database);
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return hex_literal_argument_bytes(
            database,
            expression,
            out_bytes,
            out_byte_count,
            out_owned_bytes
        );
    }

    rc = hex_scalar_argument_value(database, expression, cell, &handled_scalar);
    if (rc != MYLITE_OK || handled_scalar) {
        if (rc == MYLITE_OK && cell->value != NULL) {
            *out_bytes = (const unsigned char *)cell->value;
            *out_byte_count = cell->has_value_size ? cell->value_size : strlen(cell->value);
        }
        return rc;
    }

    set_hex_unsupported_error(database);
    return MYLITE_ERROR;
}

static int hex_literal_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (literal == NULL || out_bytes == NULL || out_byte_count == NULL || out_owned_bytes == NULL) {
        return MYLITE_MISUSE;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = decode_sql_string_literal_with_policy(
            database,
            literal,
            "HEX() supports only string, hex, integer, boolean, NULL, supported session "
            "scalar, and supported system variable arguments",
            "HEX() string literal is invalid",
            true,
            out_owned_bytes,
            out_byte_count
        );
        if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)*out_owned_bytes;
        }
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_HEX) {
        rc = decode_binary_hex_literal(database, literal, out_owned_bytes, out_byte_count);
        if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)*out_owned_bytes;
        }
        return rc;
    }

    set_hex_unsupported_error(database);
    return MYLITE_ERROR;
}

static int hex_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    bool *out_handled
) {
    int rc =
        hex_non_weight_string_scalar_argument_value(database, expression, out_cell, out_handled);

    if (rc != MYLITE_OK || out_handled == NULL || *out_handled) {
        return rc;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION:
    case MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION:
        *out_handled = true;
        return weight_string_function_value(database, expression, out_cell);
    default:
        break;
    }
    return MYLITE_OK;
}

static int hex_non_weight_string_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    bool *out_handled
) {
    int written = 0;

    if (out_cell == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = true;
    switch (expression->kind) {
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
        if (database->session.has_selected_schema) {
            out_cell->value = database->session.selected_schema;
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_USER_FUNCTION:
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
        out_cell->value = database->session.client_user_identity;
        return MYLITE_OK;
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
        out_cell->value = database->session.current_user_identity;
        return MYLITE_OK;
    case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
        out_cell->value = "NONE";
        return MYLITE_OK;
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
        written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRIu64,
            database->session.connection_id
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            set_runtime_error(database, "failed to format CONNECTION_ID() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    case MYLITE_SQL_AST_VERSION_FUNCTION:
        out_cell->value = MYLITE_MYSQL_SERVER_VERSION_STRING;
        return MYLITE_OK;
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE:
        return current_timestamp_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_SYSDATE_FUNCTION:
        return sysdate_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_CURRENT_DATE_VALUE:
        return current_date_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_CURRENT_TIME_VALUE:
        return current_time_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_UTC_DATE_VALUE:
        return utc_date_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_UTC_TIME_VALUE:
        return utc_time_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE:
        return utc_timestamp_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
        written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRId64,
            database->session.previous_row_count
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            set_runtime_error(database, "failed to format ROW_COUNT() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION:
        written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRIu64,
            database->session.found_rows
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            set_runtime_error(database, "failed to format FOUND_ROWS() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
        written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRIu64,
            database->session.last_insert_id
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            set_runtime_error(database, "failed to format LAST_INSERT_ID() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        return system_variable_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
        return cast_binary_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
        return convert_binary_type_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        return convert_using_binary_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
        return convert_using_charset_value(database, expression, out_cell);
    case MYLITE_SQL_AST_COLLATE_EXPRESSION:
        return collate_expression_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION:
        return mylite_execution_scalar_json_extract_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_QUOTE_FUNCTION:
        return mylite_execution_scalar_json_quote_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION:
        return mylite_execution_scalar_json_unquote_function_value(database, expression, out_cell);
    default:
        break;
    }

    *out_handled = false;
    return MYLITE_OK;
}

static int base64_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
) {
    bool handled_scalar = false;
    int rc = MYLITE_OK;

    if (cell == NULL || function_name == NULL || out_bytes == NULL || out_byte_count == NULL ||
        out_owned_bytes == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_bytes = NULL;
    *out_byte_count = 0U;
    *out_owned_bytes = NULL;
    *out_is_null = false;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_base64_argument_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return base64_literal_argument_bytes(
            database,
            expression,
            function_name,
            out_bytes,
            out_byte_count,
            out_owned_bytes,
            out_is_null
        );
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return base64_unary_argument_bytes(
            database,
            expression,
            function_name,
            out_bytes,
            out_byte_count,
            out_owned_bytes,
            out_is_null
        );
    }

    rc = base64_scalar_argument_value(database, expression, cell, &handled_scalar);
    if (rc != MYLITE_OK || handled_scalar) {
        if (rc == MYLITE_OK && cell->value == NULL) {
            *out_is_null = true;
        } else if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)cell->value;
            *out_byte_count = strlen(cell->value);
            if (cell->has_value_size) {
                *out_byte_count = cell->value_size;
            }
        }
        return rc;
    }

    set_base64_argument_unsupported_error(database, function_name);
    return MYLITE_ERROR;
}

static int base64_literal_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const char *function_name,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (literal == NULL || function_name == NULL || out_bytes == NULL || out_byte_count == NULL ||
        out_owned_bytes == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_bytes =
            (const unsigned char *)(literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ? "1" : "0");
        *out_byte_count = 1U;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        rc = copy_source_span_text(database, &literal->span, out_owned_bytes);
        if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)*out_owned_bytes;
            *out_byte_count = strlen(*out_owned_bytes);
        }
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        return base64_string_literal_argument_bytes(
            database,
            literal,
            function_name,
            out_bytes,
            out_byte_count,
            out_owned_bytes
        );
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_HEX) {
        rc = decode_binary_hex_literal(database, literal, out_owned_bytes, out_byte_count);
        if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)*out_owned_bytes;
        }
        return rc;
    }

    set_base64_argument_unsupported_error(database, function_name);
    return MYLITE_ERROR;
}

static int base64_string_literal_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const char *function_name,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes
) {
    char unsupported_message[base64_unsupported_message_capacity];
    char invalid_message[base64_invalid_message_capacity];
    int written = 0;
    int rc = MYLITE_OK;

    written = snprintf(
        unsupported_message,
        sizeof(unsupported_message),
        "%s() supports only string, hex, integer, boolean, NULL, supported session scalar, "
        "supported system variable, binary cast/convert, and descriptor column arguments",
        function_name
    );
    if (written < 0 || (size_t)written >= sizeof(unsupported_message)) {
        set_runtime_error(database, "failed to format Base64 unsupported message");
        return MYLITE_ERROR;
    }
    written = snprintf(
        invalid_message,
        sizeof(invalid_message),
        "%s() string literal is invalid",
        function_name
    );
    if (written < 0 || (size_t)written >= sizeof(invalid_message)) {
        set_runtime_error(database, "failed to format Base64 invalid message");
        return MYLITE_ERROR;
    }
    rc = decode_sql_string_literal_with_policy(
        database,
        literal,
        unsupported_message,
        invalid_message,
        true,
        out_owned_bytes,
        out_byte_count
    );
    if (rc == MYLITE_OK) {
        *out_bytes = (const unsigned char *)*out_owned_bytes;
    }
    return rc;
}

static int base64_unary_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
) {
    const struct mylite_sql_ast_node *literal = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    char message[base64_signed_integer_message_capacity];
    int written = 0;
    int rc = MYLITE_OK;

    if (expression == NULL || function_name == NULL || out_bytes == NULL ||
        out_byte_count == NULL || out_owned_bytes == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }

    written = snprintf(
        message,
        sizeof(message),
        "%s() supports only signed integer literal arguments",
        function_name
    );
    if (written < 0 || (size_t)written >= sizeof(message)) {
        set_runtime_error(database, "failed to format Base64 signed-integer message");
        return MYLITE_ERROR;
    }

    operator_kind = mylite_sql_ast_node_operator(expression);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        set_unsupported_error(database, message);
        return MYLITE_ERROR;
    }

    literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(database, message);
        return MYLITE_ERROR;
    }

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
        rc = copy_source_span_text(database, &literal->span, out_owned_bytes);
    } else {
        if (literal->span.length > SIZE_MAX - 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        *out_owned_bytes = (char *)malloc(literal->span.length + 2U);
        if (*out_owned_bytes == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        (*out_owned_bytes)[0] = '-';
        memcpy(*out_owned_bytes + 1U, literal->span.text, literal->span.length);
        (*out_owned_bytes)[literal->span.length + 1U] = '\0';
        rc = MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        *out_bytes = (const unsigned char *)*out_owned_bytes;
        *out_byte_count = strlen(*out_owned_bytes);
        *out_is_null = false;
    }
    return rc;
}

static int base64_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    bool *out_handled
) {
    return hex_scalar_argument_value(database, expression, out_cell, out_handled);
}

static void set_base64_argument_unsupported_error(
    struct mylite_db *database,
    const char *function_name
) {
    char message[base64_unsupported_message_capacity];
    int written = 0;

    if (function_name == NULL) {
        set_runtime_error(database, "missing Base64 function name");
        return;
    }
    written = snprintf(
        message,
        sizeof(message),
        "%s() supports only string, hex, integer, boolean, NULL, supported session scalar, "
        "supported system variable, binary cast/convert, and descriptor column arguments",
        function_name
    );
    if (written < 0 || (size_t)written >= sizeof(message)) {
        set_runtime_error(database, "failed to format Base64 unsupported message");
        return;
    }
    set_unsupported_error(database, message);
}

static int unhex_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
) {
    bool handled_scalar = false;
    int rc = MYLITE_OK;

    if (cell == NULL || out_bytes == NULL || out_byte_count == NULL || out_owned_bytes == NULL ||
        out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_bytes = NULL;
    *out_byte_count = 0U;
    *out_owned_bytes = NULL;
    *out_is_null = false;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "UNHEX() supports only string, hex, integer, boolean, NULL, supported session "
            "scalar, and supported system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return unhex_literal_argument_bytes(
            database,
            expression,
            out_bytes,
            out_byte_count,
            out_owned_bytes,
            out_is_null
        );
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return unhex_unary_argument_bytes(
            database,
            expression,
            out_bytes,
            out_byte_count,
            out_owned_bytes,
            out_is_null
        );
    }

    rc = unhex_scalar_argument_value(database, expression, cell, &handled_scalar);
    if (rc != MYLITE_OK || handled_scalar) {
        if (rc == MYLITE_OK && cell->value == NULL) {
            *out_is_null = true;
        } else if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)cell->value;
            *out_byte_count = strlen(cell->value);
            if (cell->has_value_size) {
                *out_byte_count = cell->value_size;
            }
        }
        return rc;
    }

    set_unsupported_error(
        database,
        "UNHEX() supports only string, hex, integer, boolean, NULL, supported session scalar, "
        "and supported system variable arguments"
    );
    return MYLITE_ERROR;
}

static int unhex_literal_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (literal == NULL || out_bytes == NULL || out_byte_count == NULL || out_owned_bytes == NULL ||
        out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_bytes =
            (const unsigned char *)(literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ? "1" : "0");
        *out_byte_count = 1U;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        rc = copy_source_span_text(database, &literal->span, out_owned_bytes);
        if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)*out_owned_bytes;
            *out_byte_count = strlen(*out_owned_bytes);
        }
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = decode_sql_string_literal_with_policy(
            database,
            literal,
            "UNHEX() supports only string, hex, integer, boolean, NULL, supported session "
            "scalar, and supported system variable arguments",
            "UNHEX() string literal is invalid",
            true,
            out_owned_bytes,
            out_byte_count
        );
        if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)*out_owned_bytes;
        }
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_HEX) {
        rc = decode_binary_hex_literal(database, literal, out_owned_bytes, out_byte_count);
        if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)*out_owned_bytes;
        }
        return rc;
    }

    set_unsupported_error(
        database,
        "UNHEX() supports only string, hex, integer, boolean, NULL, supported session scalar, "
        "and supported system variable arguments"
    );
    return MYLITE_ERROR;
}

static int unhex_unary_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
) {
    const struct mylite_sql_ast_node *literal = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    int rc = MYLITE_OK;

    if (expression == NULL || out_bytes == NULL || out_byte_count == NULL ||
        out_owned_bytes == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    operator_kind = mylite_sql_ast_node_operator(expression);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        set_unsupported_error(database, "UNHEX() supports only signed integer literal arguments");
        return MYLITE_ERROR;
    }

    literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(database, "UNHEX() supports only signed integer literal arguments");
        return MYLITE_ERROR;
    }

    rc = copy_source_span_text(
        database,
        operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ? &literal->span : &expression->span,
        out_owned_bytes
    );
    if (rc == MYLITE_OK) {
        *out_bytes = (const unsigned char *)*out_owned_bytes;
        *out_byte_count = strlen(*out_owned_bytes);
        *out_is_null = false;
    }
    return rc;
}

static int unhex_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    bool *out_handled
) {
    return hex_scalar_argument_value(database, expression, out_cell, out_handled);
}

static int stage_unhex_incorrect_string_warning(
    struct mylite_db *database,
    struct session_scalar_cell *cell,
    const void *input,
    size_t input_size
) {
    int rc = MYLITE_OK;

    if (cell == NULL || (input == NULL && input_size != 0U)) {
        return MYLITE_MISUSE;
    }
    rc = mylite_string_unhex_format_warning_input(
        input,
        input_size,
        cell->staged_unhex_incorrect_string_text,
        sizeof(cell->staged_unhex_incorrect_string_text)
    );
    if (rc != MYLITE_OK) {
        set_runtime_error(database, "failed to format UNHEX() warning input");
        return MYLITE_ERROR;
    }
    cell->has_staged_unhex_incorrect_string_warning = true;
    return MYLITE_OK;
}

static int uuid_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION) {
        return uuid_to_bin_nested_argument_bytes(
            database,
            expression,
            out_bytes,
            out_byte_count,
            out_owned_bytes,
            out_is_null
        );
    }
    return uuid_direct_argument_bytes(
        database,
        expression,
        cell,
        out_bytes,
        out_byte_count,
        out_owned_bytes,
        out_is_null
    );
}

static int uuid_direct_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
) {
    struct session_scalar_cell scalar_cell = {0};
    bool handled_scalar = false;
    int rc = MYLITE_OK;

    if (cell == NULL || out_bytes == NULL || out_byte_count == NULL || out_owned_bytes == NULL ||
        out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_bytes = NULL;
    *out_byte_count = 0U;
    *out_owned_bytes = NULL;
    *out_is_null = false;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_uuid_unsupported_error(database, "UUID conversion");
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return uuid_literal_argument_bytes(
            database,
            expression,
            out_bytes,
            out_byte_count,
            out_owned_bytes,
            out_is_null
        );
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return uuid_unary_argument_bytes(
            database,
            expression,
            out_bytes,
            out_byte_count,
            out_owned_bytes,
            out_is_null
        );
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return uuid_unknown_column_argument(database, expression);
    }

    rc = uuid_scalar_argument_value(database, expression, &scalar_cell, &handled_scalar);
    if (rc != MYLITE_OK || handled_scalar) {
        if (rc == MYLITE_OK && scalar_cell.value == NULL) {
            *out_is_null = true;
        } else if (rc == MYLITE_OK) {
            char *copy = NULL;
            size_t byte_count = strlen(scalar_cell.value);

            if (scalar_cell.has_value_size) {
                byte_count = scalar_cell.value_size;
            }

            if (byte_count == SIZE_MAX) {
                set_nomem_error(database);
                session_scalar_cell_deinit(&scalar_cell);
                return MYLITE_NOMEM;
            }
            copy = (char *)malloc(byte_count + 1U);
            if (copy == NULL) {
                set_nomem_error(database);
                session_scalar_cell_deinit(&scalar_cell);
                return MYLITE_NOMEM;
            }
            if (byte_count != 0U) {
                memcpy(copy, scalar_cell.value, byte_count);
            }
            copy[byte_count] = '\0';
            *out_owned_bytes = copy;
            *out_bytes = (const unsigned char *)copy;
            *out_byte_count = byte_count;
        }
        session_scalar_cell_deinit(&scalar_cell);
        return rc;
    }
    session_scalar_cell_deinit(&scalar_cell);

    set_uuid_unsupported_error(database, "UUID conversion");
    return MYLITE_ERROR;
}

static int uuid_to_bin_nested_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
) {
    struct session_scalar_cell cell = {0};
    const struct mylite_sql_ast_node *argument = NULL;
    const struct mylite_sql_ast_node *swap_argument = NULL;
    const unsigned char *argument_bytes = NULL;
    unsigned char uuid_bytes[MYLITE_UUID_BINARY_SIZE];
    char *owned_argument_bytes = NULL;
    char *owned_result_bytes = NULL;
    size_t argument_byte_count = 0U;
    bool argument_is_null = false;
    bool swap = false;
    bool valid = false;
    int rc = MYLITE_OK;

    if (out_bytes == NULL || out_byte_count == NULL || out_owned_bytes == NULL ||
        out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_bytes = NULL;
    *out_byte_count = 0U;
    *out_owned_bytes = NULL;
    *out_is_null = false;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION ||
        (mylite_sql_ast_node_child_count(expression) != 1U &&
         mylite_sql_ast_node_child_count(expression) != 2U)) {
        set_native_function_parameter_count_error(database, "UUID_TO_BIN");
        return MYLITE_ERROR;
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    swap_argument = unwrap_parenthesized_expression(child_at(expression, 1U));
    rc = uuid_direct_argument_bytes(
        database,
        argument,
        &cell,
        &argument_bytes,
        &argument_byte_count,
        &owned_argument_bytes,
        &argument_is_null
    );
    if (rc == MYLITE_OK && swap_argument != NULL) {
        rc = uuid_swap_flag_value(database, swap_argument, &swap);
    }
    if (rc != MYLITE_OK || argument_is_null) {
        free(owned_argument_bytes);
        session_scalar_cell_deinit(&cell);
        *out_is_null = argument_is_null;
        return rc;
    }

    rc =
        mylite_uuid_string_to_binary(argument_bytes, argument_byte_count, swap, uuid_bytes, &valid);
    if (rc != MYLITE_OK) {
        free(owned_argument_bytes);
        session_scalar_cell_deinit(&cell);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
        return rc;
    }
    if (!valid) {
        rc = mylite_uuid_set_incorrect_string_error(
            database,
            argument_bytes,
            argument_byte_count,
            "uuid_to_bin"
        );
        free(owned_argument_bytes);
        session_scalar_cell_deinit(&cell);
        return rc;
    }
    free(owned_argument_bytes);
    session_scalar_cell_deinit(&cell);

    owned_result_bytes = (char *)malloc(MYLITE_UUID_BINARY_SIZE + 1U);
    if (owned_result_bytes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(owned_result_bytes, uuid_bytes, MYLITE_UUID_BINARY_SIZE);
    owned_result_bytes[MYLITE_UUID_BINARY_SIZE] = '\0';
    *out_owned_bytes = owned_result_bytes;
    *out_bytes = (const unsigned char *)owned_result_bytes;
    *out_byte_count = MYLITE_UUID_BINARY_SIZE;
    return MYLITE_OK;
}

static int uuid_literal_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (literal == NULL || out_bytes == NULL || out_byte_count == NULL || out_owned_bytes == NULL ||
        out_is_null == NULL) {
        return MYLITE_MISUSE;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_bytes =
            (const unsigned char *)(literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ? "1" : "0");
        *out_byte_count = 1U;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        rc = copy_source_span_text(database, &literal->span, out_owned_bytes);
        if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)*out_owned_bytes;
            *out_byte_count = strlen(*out_owned_bytes);
        }
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = decode_sql_string_literal_with_policy(
            database,
            literal,
            "UUID conversion functions support only string, hex, integer, boolean, NULL, and "
            "supported scalar arguments",
            "UUID conversion string literal is invalid",
            true,
            out_owned_bytes,
            out_byte_count
        );
        if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)*out_owned_bytes;
        }
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_HEX) {
        rc = decode_binary_hex_literal(database, literal, out_owned_bytes, out_byte_count);
        if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)*out_owned_bytes;
        }
        return rc;
    }

    set_uuid_unsupported_error(database, "UUID conversion");
    return MYLITE_ERROR;
}

static int uuid_unary_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
) {
    const struct mylite_sql_ast_node *literal = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    int rc = MYLITE_OK;

    if (expression == NULL || out_bytes == NULL || out_byte_count == NULL ||
        out_owned_bytes == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    operator_kind = mylite_sql_ast_node_operator(expression);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        set_uuid_unsupported_error(database, "UUID conversion");
        return MYLITE_ERROR;
    }

    literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_uuid_unsupported_error(database, "UUID conversion");
        return MYLITE_ERROR;
    }

    rc = copy_source_span_text(
        database,
        operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ? &literal->span : &expression->span,
        out_owned_bytes
    );
    if (rc == MYLITE_OK) {
        *out_bytes = (const unsigned char *)*out_owned_bytes;
        *out_byte_count = strlen(*out_owned_bytes);
        *out_is_null = false;
    }
    return rc;
}

static int uuid_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    bool *out_handled
) {
    if (out_cell == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        *out_handled = false;
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_UUID_FUNCTION) {
        *out_handled = true;
        return uuid_generate_function_value(database, expression, out_cell);
    }
    return hex_scalar_argument_value(database, expression, out_cell, out_handled);
}

static int uuid_swap_flag_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_swap
) {
    const struct mylite_sql_ast_node *literal = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t magnitude = 0U;

    if (out_swap == NULL) {
        return MYLITE_MISUSE;
    }
    *out_swap = false;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_uuid_unsupported_error(database, "UUID conversion swap flag");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return uuid_swap_flag_unary_value(database, expression, out_swap);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_uuid_unsupported_error(database, "UUID conversion swap flag");
        return MYLITE_ERROR;
    }

    literal = expression;
    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_NULL:
    case MYLITE_SQL_AST_LITERAL_FALSE:
        *out_swap = false;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_TRUE:
        *out_swap = true;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
            set_uuid_unsupported_error(database, "UUID conversion swap flag");
            return MYLITE_ERROR;
        }
        *out_swap = magnitude != 0U;
        return MYLITE_OK;
    default:
        break;
    }

    set_uuid_unsupported_error(database, "UUID conversion swap flag");
    return MYLITE_ERROR;
}

static int uuid_swap_flag_unary_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_swap
) {
    const struct mylite_sql_ast_node *literal = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    uint64_t magnitude = 0U;

    if (expression == NULL || out_swap == NULL) {
        return MYLITE_MISUSE;
    }
    *out_swap = false;

    operator_kind = mylite_sql_ast_node_operator(expression);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        set_uuid_unsupported_error(database, "UUID conversion swap flag");
        return MYLITE_ERROR;
    }
    literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER ||
        parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        set_uuid_unsupported_error(database, "UUID conversion swap flag");
        return MYLITE_ERROR;
    }

    *out_swap = magnitude != 0U;
    return MYLITE_OK;
}

static int uuid_unknown_column_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = collect_column_reference_parts(database, expression, parts, &part_count);

    if (rc == MYLITE_OK) {
        rc = format_column_reference_name(
            database,
            parts,
            part_count,
            column_name,
            sizeof(column_name)
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    set_unknown_column_error(database, column_name);
    return MYLITE_ERROR;
}

static void set_uuid_unsupported_error(struct mylite_db *database, const char *function_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "%s supports only supported UUID conversion scalar arguments",
        function_name == NULL ? "UUID conversion" : function_name
    );

    if (written < 0 || (size_t)written >= sizeof(message)) {
        set_unsupported_error(database, "UUID conversion argument shape is not supported");
        return;
    }
    set_unsupported_error(database, message);
}

static int format_hex_bytes(
    struct mylite_db *database,
    const unsigned char *bytes,
    size_t byte_count,
    char **out_text
) {
    static const char digits[] = "0123456789ABCDEF";
    char *text = NULL;

    if ((bytes == NULL && byte_count != 0U) || out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (byte_count > (SIZE_MAX - 1U) / 2U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    text = (char *)malloc((byte_count * 2U) + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < byte_count; ++index) {
        unsigned char byte = bytes[index];

        text[index * 2U] = digits[byte >> byte_high_nibble_shift];
        text[(index * 2U) + 1U] = digits[byte & byte_low_nibble_mask];
    }
    text[byte_count * 2U] = '\0';
    *out_text = text;
    return MYLITE_OK;
}

static int crc32_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes
) {
    const struct mylite_sql_ast_node *literal = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;

    if (cell == NULL || out_bytes == NULL || out_byte_count == NULL || out_owned_bytes == NULL) {
        return MYLITE_MISUSE;
    }
    *out_bytes = NULL;
    *out_byte_count = 0U;
    *out_owned_bytes = NULL;

    expression = unwrap_parenthesized_expression(expression);
    literal = expression;
    if (expression == NULL) {
        set_crc32_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_crc32_unsupported_error(database);
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_crc32_unsupported_error(database);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if ((is_negative || expression != literal) && literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_crc32_unsupported_error(database);
        return MYLITE_ERROR;
    }
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_NULL:
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_TRUE:
        *out_bytes = (const unsigned char *)"1";
        *out_byte_count = 1U;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_FALSE:
        *out_bytes = (const unsigned char *)"0";
        *out_byte_count = 1U;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_INTEGER: {
        int rc = normalize_decimal_integer_literal(
            database,
            &literal->span,
            is_negative,
            cell->literal_text,
            sizeof(cell->literal_text)
        );

        if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)cell->literal_text;
            *out_byte_count = strlen(cell->literal_text);
        }
        return rc;
    }
    case MYLITE_SQL_AST_LITERAL_STRING: {
        int rc = decode_sql_string_literal(
            database,
            literal,
            "CRC32() supports only string, hex, integer, boolean, and NULL literals",
            "CRC32() string literals do not support embedded NUL bytes",
            out_owned_bytes,
            out_byte_count
        );

        if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)*out_owned_bytes;
        }
        return rc;
    }
    case MYLITE_SQL_AST_LITERAL_HEX: {
        int rc = decode_binary_hex_literal(database, literal, out_owned_bytes, out_byte_count);

        if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)*out_owned_bytes;
        }
        return rc;
    }
    default:
        break;
    }

    set_crc32_unsupported_error(database);
    return MYLITE_ERROR;
}

static uint32_t crc32_checksum(const unsigned char *bytes, size_t byte_count) {
    uint32_t crc = UINT32_C(0xffffffff);

    for (size_t index = 0U; index < byte_count; ++index) {
        crc ^= (uint32_t)bytes[index];
        for (unsigned int bit = 0U; bit < crc32_bits_per_byte; ++bit) {
            if ((crc & UINT32_C(1)) != 0U) {
                crc = (crc >> 1U) ^ UINT32_C(0xedb88320);
            } else {
                crc >>= 1U;
            }
        }
    }
    return crc ^ UINT32_C(0xffffffff);
}

static int format_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct scalar_exact_decimal value = {
        .is_null = false,
        .is_negative = false,
        .integer_digits = "",
        .integer_length = 0U,
        .fraction_digits = "",
        .fraction_length = 0U,
    };
    struct scalar_decimal_places places = {
        .is_null = false,
        .is_negative = false,
        .overflowed = false,
        .magnitude = 0U,
    };
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_FORMAT_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_format_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = scalar_format_truncate_arguments(database, expression, "FORMAT", &value, &places);
    if (rc == MYLITE_OK) {
        rc = assign_format_function_text(database, &value, &places, out_cell);
    }
    return rc;
}

static int truncate_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct scalar_exact_decimal value = {
        .is_null = false,
        .is_negative = false,
        .integer_digits = "",
        .integer_length = 0U,
        .fraction_digits = "",
        .fraction_length = 0U,
    };
    struct scalar_decimal_places places = {
        .is_null = false,
        .is_negative = false,
        .overflowed = false,
        .magnitude = 0U,
    };
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_TRUNCATE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_truncate_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = scalar_format_truncate_arguments(database, expression, "TRUNCATE", &value, &places);
    if (rc == MYLITE_OK) {
        rc = assign_truncate_function_text(database, &value, &places, out_cell);
    }
    return rc;
}

static int scalar_format_truncate_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    struct scalar_exact_decimal *out_value,
    struct scalar_decimal_places *out_places
) {
    int rc = MYLITE_OK;

    if (expression == NULL || function_name == NULL || out_value == NULL || out_places == NULL) {
        return MYLITE_MISUSE;
    }
    rc =
        scalar_exact_decimal_argument(database, child_at(expression, 0U), function_name, out_value);
    if (rc == MYLITE_OK) {
        rc = scalar_decimal_places_argument(
            database,
            child_at(expression, 1U),
            function_name,
            out_places
        );
    }
    return rc;
}

static int scalar_exact_decimal_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    struct scalar_exact_decimal *out_decimal
) {
    const struct mylite_sql_ast_node *literal = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;

    if (function_name == NULL || out_decimal == NULL) {
        return MYLITE_MISUSE;
    }
    *out_decimal = (struct scalar_exact_decimal){
        .is_null = false,
        .is_negative = false,
        .integer_digits = "",
        .integer_length = 0U,
        .fraction_digits = "",
        .fraction_length = 0U,
    };
    expression = unwrap_parenthesized_expression(expression);
    literal = expression;
    if (expression == NULL) {
        set_format_truncate_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_format_truncate_unsupported_error(database, function_name);
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_format_truncate_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if ((is_negative || expression != literal) && literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER &&
        literal_kind != MYLITE_SQL_AST_LITERAL_DECIMAL) {
        set_format_truncate_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_decimal->is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_decimal->integer_digits[0] = literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ? '1' : '0';
        out_decimal->integer_digits[1] = '\0';
        out_decimal->integer_length = 1U;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER &&
        literal_kind != MYLITE_SQL_AST_LITERAL_DECIMAL) {
        set_format_truncate_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }

    return parse_scalar_exact_decimal_literal(
        database,
        &literal->span,
        is_negative,
        function_name,
        out_decimal
    );
}

static int scalar_decimal_places_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    struct scalar_decimal_places *out_places
) {
    const struct mylite_sql_ast_node *literal = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;

    if (function_name == NULL || out_places == NULL) {
        return MYLITE_MISUSE;
    }
    memset(out_places, 0, sizeof(*out_places));
    expression = unwrap_parenthesized_expression(expression);
    literal = expression;
    if (expression == NULL) {
        set_format_truncate_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_format_truncate_unsupported_error(database, function_name);
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_format_truncate_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if ((is_negative || expression != literal) && literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_format_truncate_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_places->is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_places->magnitude = literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ? 1U : 0U;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_format_truncate_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    return parse_scalar_decimal_places_literal(
        database,
        &literal->span,
        is_negative,
        function_name,
        out_places
    );
}

static int parse_scalar_exact_decimal_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    bool is_negative,
    const char *function_name,
    struct scalar_exact_decimal *out_decimal
) {
    size_t dot_index = SIZE_MAX;
    int rc = MYLITE_OK;

    if (span == NULL || span->text == NULL || span->length == 0U || function_name == NULL ||
        out_decimal == NULL) {
        return MYLITE_MISUSE;
    }
    *out_decimal = (struct scalar_exact_decimal){.is_negative = is_negative};
    rc = parse_scalar_exact_decimal_dot_index(database, span, &dot_index);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = assign_scalar_exact_decimal_integer(database, span, dot_index, function_name, out_decimal);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return assign_scalar_exact_decimal_fraction(
        database,
        span,
        dot_index,
        function_name,
        out_decimal
    );
}

static int parse_scalar_exact_decimal_dot_index(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    size_t *out_dot_index
) {
    size_t dot_index = SIZE_MAX;

    if (span == NULL || span->text == NULL || out_dot_index == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; index < span->length; ++index) {
        char byte = span->text[index];

        if (byte == '.') {
            if (dot_index != SIZE_MAX) {
                set_parse_error(database, NULL);
                return MYLITE_ERROR;
            }
            dot_index = index;
        } else if (byte < '0' || byte > '9') {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
    }
    *out_dot_index = dot_index;
    return MYLITE_OK;
}

static int assign_scalar_exact_decimal_integer(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    size_t dot_index,
    const char *function_name,
    struct scalar_exact_decimal *out_decimal
) {
    size_t integer_start = 0U;
    size_t integer_end = 0U;
    size_t integer_length = 0U;

    if (span == NULL || span->text == NULL || function_name == NULL || out_decimal == NULL) {
        return MYLITE_MISUSE;
    }

    integer_end = dot_index == SIZE_MAX ? span->length : dot_index;
    while (integer_start < integer_end && span->text[integer_start] == '0') {
        ++integer_start;
    }
    if (integer_start == integer_end) {
        out_decimal->integer_digits[0] = '0';
        out_decimal->integer_digits[1] = '\0';
        out_decimal->integer_length = 1U;
    } else {
        integer_length = integer_end - integer_start;
        if (integer_length >= scalar_exact_decimal_part_capacity) {
            set_format_truncate_unsupported_error(database, function_name);
            return MYLITE_ERROR;
        }
        memcpy(out_decimal->integer_digits, &span->text[integer_start], integer_length);
        out_decimal->integer_digits[integer_length] = '\0';
        out_decimal->integer_length = integer_length;
    }
    return MYLITE_OK;
}

static int assign_scalar_exact_decimal_fraction(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    size_t dot_index,
    const char *function_name,
    struct scalar_exact_decimal *out_decimal
) {
    size_t fraction_length = 0U;

    if (span == NULL || span->text == NULL || function_name == NULL || out_decimal == NULL) {
        return MYLITE_MISUSE;
    }

    if (dot_index != SIZE_MAX) {
        size_t fraction_start = dot_index + 1U;

        fraction_length = span->length - fraction_start;
        if (fraction_length >= scalar_exact_decimal_part_capacity) {
            set_format_truncate_unsupported_error(database, function_name);
            return MYLITE_ERROR;
        }
        if (fraction_length != 0U) {
            memcpy(out_decimal->fraction_digits, &span->text[fraction_start], fraction_length);
        }
        out_decimal->fraction_digits[fraction_length] = '\0';
        out_decimal->fraction_length = fraction_length;
    }
    return MYLITE_OK;
}

static int parse_scalar_decimal_places_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    bool is_negative,
    const char *function_name,
    struct scalar_decimal_places *out_places
) {
    uint64_t magnitude = 0U;

    if (span == NULL || span->text == NULL || span->length == 0U || function_name == NULL ||
        out_places == NULL) {
        return MYLITE_MISUSE;
    }
    *out_places = (struct scalar_decimal_places){.is_negative = is_negative};
    for (size_t index = 0U; index < span->length; ++index) {
        uint64_t digit = 0U;
        char byte = span->text[index];

        if (byte < '0' || byte > '9') {
            set_format_truncate_unsupported_error(database, function_name);
            return MYLITE_ERROR;
        }
        digit = (uint64_t)(byte - '0');
        if (!out_places->overflowed) {
            if (magnitude > (UINT64_MAX - digit) / decimal_base) {
                magnitude = UINT64_MAX;
                out_places->overflowed = true;
            } else {
                magnitude = (magnitude * decimal_base) + digit;
            }
        }
    }
    out_places->magnitude = magnitude;
    if (magnitude == 0U && !out_places->overflowed) {
        out_places->is_negative = false;
    }
    return MYLITE_OK;
}

static int assign_format_function_text(
    struct mylite_db *database,
    const struct scalar_exact_decimal *value,
    const struct scalar_decimal_places *places,
    struct session_scalar_cell *out_cell
) {
    char integer_digits[scalar_exact_decimal_part_capacity + 2U];
    char fraction_digits[scalar_format_max_decimals + 1U];
    size_t integer_length = 0U;
    size_t decimal_places = 0U;
    int rc = MYLITE_OK;

    if (value == NULL || places == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (value->is_null || places->is_null) {
        return MYLITE_OK;
    }

    decimal_places = format_decimal_place_count(places);
    integer_length = value->integer_length;
    memcpy(integer_digits, value->integer_digits, integer_length + 1U);
    copy_format_fraction_digits(value, decimal_places, fraction_digits);
    rc = round_format_decimal_digits(
        database,
        value,
        decimal_places,
        integer_digits,
        &integer_length,
        fraction_digits
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    return assign_format_output_text(
        database,
        value->is_negative,
        integer_digits,
        integer_length,
        fraction_digits,
        decimal_places,
        out_cell
    );
}

static size_t format_decimal_place_count(const struct scalar_decimal_places *places) {
    if (places == NULL || places->is_negative) {
        return 0U;
    }
    if (places->overflowed || places->magnitude > scalar_format_max_decimals) {
        return scalar_format_max_decimals;
    }
    return (size_t)places->magnitude;
}

static void copy_format_fraction_digits(
    const struct scalar_exact_decimal *value,
    size_t decimal_places,
    char *out_fraction_digits
) {
    if (value == NULL || out_fraction_digits == NULL) {
        return;
    }
    for (size_t index = 0U; index < decimal_places; ++index) {
        if (index < value->fraction_length) {
            out_fraction_digits[index] = value->fraction_digits[index];
        } else {
            out_fraction_digits[index] = '0';
        }
    }
    out_fraction_digits[decimal_places] = '\0';
}

static int round_format_decimal_digits(
    struct mylite_db *database,
    const struct scalar_exact_decimal *value,
    size_t decimal_places,
    char *integer_digits,
    size_t *in_out_integer_length,
    char *fraction_digits
) {
    size_t integer_length = 0U;
    bool carry = false;

    if (value == NULL || integer_digits == NULL || in_out_integer_length == NULL ||
        fraction_digits == NULL) {
        return MYLITE_MISUSE;
    }
    if (value->fraction_length <= decimal_places || value->fraction_digits[decimal_places] < '5') {
        return MYLITE_OK;
    }

    integer_length = *in_out_integer_length;
    carry = true;
    for (size_t index = decimal_places; carry && index > 0U; --index) {
        if (fraction_digits[index - 1U] == '9') {
            fraction_digits[index - 1U] = '0';
        } else {
            ++fraction_digits[index - 1U];
            carry = false;
        }
    }
    for (size_t index = integer_length; carry && index > 0U; --index) {
        if (integer_digits[index - 1U] == '9') {
            integer_digits[index - 1U] = '0';
        } else {
            ++integer_digits[index - 1U];
            carry = false;
        }
    }
    if (carry) {
        if (integer_length + 1U >= scalar_exact_decimal_part_capacity + 2U) {
            set_format_unsupported_error(database);
            return MYLITE_ERROR;
        }
        memmove(integer_digits + 1U, integer_digits, integer_length + 1U);
        integer_digits[0] = '1';
        ++integer_length;
    }
    *in_out_integer_length = integer_length;
    return MYLITE_OK;
}

static int assign_format_output_text(
    struct mylite_db *database,
    bool is_negative,
    const char *integer_digits,
    size_t integer_length,
    const char *fraction_digits,
    size_t decimal_places,
    struct session_scalar_cell *out_cell
) {
    size_t output_length = 0U;
    size_t output_index = 0U;
    char *output = NULL;
    bool is_zero = false;

    if (integer_digits == NULL || fraction_digits == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }

    is_zero = scalar_decimal_parts_are_zero(
        integer_digits,
        integer_length,
        fraction_digits,
        decimal_places
    );
    output_length = integer_length + ((integer_length - 1U) / 3U);
    if (is_negative && !is_zero) {
        ++output_length;
    }
    if (decimal_places != 0U) {
        output_length += 1U + decimal_places;
    }
    output = (char *)malloc(output_length + 1U);
    if (output == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (is_negative && !is_zero) {
        output[output_index] = '-';
        ++output_index;
    }
    for (size_t index = 0U; index < integer_length; ++index) {
        if (index != 0U && ((integer_length - index) % 3U) == 0U) {
            output[output_index] = ',';
            ++output_index;
        }
        output[output_index] = integer_digits[index];
        ++output_index;
    }
    if (decimal_places != 0U) {
        output[output_index] = '.';
        ++output_index;
        memcpy(output + output_index, fraction_digits, decimal_places);
        output_index += decimal_places;
    }
    output[output_index] = '\0';
    out_cell->owned_text = output;
    out_cell->value = out_cell->owned_text;
    return MYLITE_OK;
}

static int assign_truncate_function_text(
    struct mylite_db *database,
    const struct scalar_exact_decimal *value,
    const struct scalar_decimal_places *places,
    struct session_scalar_cell *out_cell
) {
    char integer_digits[scalar_exact_decimal_part_capacity + 1U];
    size_t integer_length = 0U;
    size_t fraction_length = 0U;
    size_t output_length = 0U;
    size_t output_index = 0U;
    char *output = NULL;
    bool is_zero = false;

    if (value == NULL || places == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (value->is_null || places->is_null) {
        return MYLITE_OK;
    }

    integer_length = value->integer_length;
    memcpy(integer_digits, value->integer_digits, integer_length + 1U);
    if (places->is_negative) {
        if (places->overflowed || places->magnitude >= integer_length) {
            integer_digits[0] = '0';
            integer_digits[1] = '\0';
            integer_length = 1U;
        } else if (places->magnitude != 0U) {
            size_t zero_count = (size_t)places->magnitude;

            memset(integer_digits + integer_length - zero_count, '0', zero_count);
            if (scalar_decimal_digits_are_zero(integer_digits, integer_length)) {
                integer_digits[0] = '0';
                integer_digits[1] = '\0';
                integer_length = 1U;
            }
        }
    } else if (places->magnitude != 0U) {
        if (places->overflowed || places->magnitude >= value->fraction_length) {
            fraction_length = value->fraction_length;
        } else {
            fraction_length = (size_t)places->magnitude;
        }
    }

    is_zero = scalar_decimal_parts_are_zero(
        integer_digits,
        integer_length,
        value->fraction_digits,
        fraction_length
    );
    output_length = integer_length;
    if (value->is_negative && !is_zero) {
        ++output_length;
    }
    if (fraction_length != 0U) {
        output_length += 1U + fraction_length;
    }
    output = (char *)malloc(output_length + 1U);
    if (output == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (value->is_negative && !is_zero) {
        output[output_index] = '-';
        ++output_index;
    }
    memcpy(output + output_index, integer_digits, integer_length);
    output_index += integer_length;
    if (fraction_length != 0U) {
        output[output_index] = '.';
        ++output_index;
        memcpy(output + output_index, value->fraction_digits, fraction_length);
        output_index += fraction_length;
    }
    output[output_index] = '\0';
    out_cell->owned_text = output;
    out_cell->value = out_cell->owned_text;
    return MYLITE_OK;
}

static void set_format_truncate_unsupported_error(
    struct mylite_db *database,
    const char *function_name
) {
    if (function_name != NULL && strcmp(function_name, "FORMAT") == 0) {
        set_format_unsupported_error(database);
        return;
    }
    set_truncate_unsupported_error(database);
}

static bool scalar_decimal_parts_are_zero(
    const char *integer_digits,
    size_t integer_digit_count,
    const char *fraction_digits,
    size_t fraction_digit_count
) {
    if (!scalar_decimal_digits_are_zero(integer_digits, integer_digit_count)) {
        return false;
    }
    return scalar_decimal_digits_are_zero(fraction_digits, fraction_digit_count);
}

static bool scalar_decimal_digits_are_zero(const char *digits, size_t digit_count) {
    if (digits == NULL) {
        return true;
    }
    for (size_t index = 0U; index < digit_count; ++index) {
        if (digits[index] != '0') {
            return false;
        }
    }
    return true;
}
