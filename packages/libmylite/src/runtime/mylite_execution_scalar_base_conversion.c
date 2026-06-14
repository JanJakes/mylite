#include "mylite_execution_scalar_binary_internal.h"

struct conv_input_value {
    bool is_null;
    bool is_negative;
    uint64_t magnitude;
    size_t division_by_zero_warning_count;
};

struct conv_arguments {
    struct conv_input_value input;
    struct scalar_arithmetic_value from_base;
    struct scalar_arithmetic_value to_base;
};

struct conv_digit_parse {
    uint64_t value;
    bool saw_digit;
    bool overflowed;
};

struct conv_output_format {
    uint64_t value;
    unsigned int base;
    bool signed_output;
};

static int evaluate_conv_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    struct conv_arguments *out_arguments,
    bool *out_is_null
);
static int set_conv_staged_division_by_zero_count(
    struct mylite_db *database,
    const struct conv_arguments *arguments,
    bool include_from_base,
    bool include_to_base,
    struct session_scalar_cell *cell
);
static unsigned int absolute_conv_base(int64_t base);
static int evaluate_conv_value_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct conv_input_value *out_value
);
static int evaluate_conv_direct_value_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct conv_input_value *out_value,
    bool *out_handled
);
static int evaluate_conv_base_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
);
static int convert_conv_value(
    struct mylite_db *database,
    const struct conv_input_value *input,
    unsigned int from_base,
    bool signed_input,
    struct session_scalar_cell *cell,
    uint64_t *out_value
);
static int parse_conv_input_digits(
    const char *input_text,
    unsigned int from_base,
    uint64_t limit,
    struct conv_digit_parse *out_parse
);
static int format_conv_input_text(
    struct mylite_db *database,
    const struct conv_input_value *input,
    char *buffer,
    size_t buffer_size
);
static int stage_conv_truncated_decimal_warning(
    struct mylite_db *database,
    const char *input_text,
    struct session_scalar_cell *cell
);
static int format_conv_output_value(
    struct mylite_db *database,
    struct conv_output_format format,
    char *buffer,
    size_t buffer_size
);
static int evaluate_base_conversion_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
);
static uint64_t count_uint64_bits(uint64_t value);

int mylite_execution_scalar_base_conversion_function_value(
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
        mylite_execution_set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_BIN_FUNCTION) {
        base = binary_base_conversion_binary_base;
    } else if (expression->kind == MYLITE_SQL_AST_OCT_FUNCTION) {
        base = binary_base_conversion_octal_base;
    } else {
        mylite_execution_set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_base_conversion_operand(
        database,
        mylite_execution_child_at(expression, 0U),
        &value
    );
    if (rc != MYLITE_OK || value.is_null) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    rc = mylite_execution_scalar_binary_format_base_conversion_value(
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

int mylite_execution_scalar_conv_function_value(
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
        mylite_execution_set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_conv_arguments(database, expression, out_cell, &arguments, &is_null);
    if (rc != MYLITE_OK || is_null) {
        return rc;
    }

    input_base = absolute_conv_base(arguments.from_base.integer);
    output_base = absolute_conv_base(arguments.to_base.integer);
    if (input_base < binary_base_conversion_binary_base ||
        input_base > binary_base_conversion_max_base ||
        output_base < binary_base_conversion_binary_base ||
        output_base > binary_base_conversion_max_base) {
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

    rc = evaluate_conv_value_operand(
        database,
        mylite_execution_child_at(expression, 0U),
        &out_arguments->input
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_arguments->input.is_null) {
        rc = set_conv_staged_division_by_zero_count(database, out_arguments, false, false, cell);
        *out_is_null = true;
        return rc;
    }

    rc = evaluate_conv_base_operand(
        database,
        mylite_execution_child_at(expression, 1U),
        &out_arguments->from_base
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_arguments->from_base.is_null) {
        rc = set_conv_staged_division_by_zero_count(database, out_arguments, true, false, cell);
        *out_is_null = true;
        return rc;
    }

    rc = evaluate_conv_base_operand(
        database,
        mylite_execution_child_at(expression, 2U),
        &out_arguments->to_base
    );
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

    rc = mylite_execution_accumulate_staged_division_by_zero_warnings(
        database,
        arguments->input.division_by_zero_warning_count,
        &warning_count
    );
    if (rc == MYLITE_OK && include_from_base) {
        rc = mylite_execution_accumulate_staged_division_by_zero_warnings(
            database,
            arguments->from_base.division_by_zero_warning_count,
            &warning_count
        );
    }
    if (rc == MYLITE_OK && include_to_base) {
        rc = mylite_execution_accumulate_staged_division_by_zero_warnings(
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
        return binary_base_conversion_max_base + 1U;
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
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_conv_direct_value_operand(database, expression, out_value, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (mylite_execution_is_scalar_bitwise_projection_expression(expression)) {
        struct scalar_bitwise_value bitwise = {.is_null = false, .integer = 0U};

        rc = mylite_execution_evaluate_scalar_bitwise_expression(database, expression, &bitwise);
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_value->is_null = bitwise.is_null;
        out_value->magnitude = bitwise.integer;
        out_value->division_by_zero_warning_count = bitwise.division_by_zero_warning_count;
        return MYLITE_OK;
    }
    if (!mylite_execution_is_scalar_arithmetic_projection_expression(expression)) {
        mylite_execution_set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = mylite_execution_evaluate_scalar_arithmetic_expression(database, expression, &arithmetic);
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
        literal = mylite_execution_unwrap_parenthesized_expression(
            mylite_execution_child_at(expression, 0U)
        );
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

    if (mylite_execution_parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        mylite_execution_set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (is_negative && magnitude > int64_min_magnitude) {
        mylite_execution_set_base_conversion_unsupported_error(database);
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
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){.is_null = false, .integer = 0};
    if (expression == NULL ||
        !mylite_execution_is_scalar_arithmetic_projection_expression(expression)) {
        mylite_execution_set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }
    return mylite_execution_evaluate_scalar_arithmetic_expression(database, expression, out_value);
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
    char input_text[binary_integer_text_capacity];
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
        return mylite_execution_format_uint64(database, input->magnitude, buffer, buffer_size);
    }
    written = snprintf(buffer, buffer_size, "-%" PRIu64, input->magnitude);
    if (written < 0 || (size_t)written >= buffer_size) {
        mylite_execution_set_runtime_error(database, "failed to format CONV() input value");
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
        mylite_execution_set_runtime_error(database, "failed to stage CONV() warning");
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

    if (buffer == NULL || buffer_size == 0U || format.base < binary_base_conversion_binary_base ||
        format.base > binary_base_conversion_max_base) {
        mylite_execution_set_runtime_error(database, "failed to format CONV() value");
        return MYLITE_ERROR;
    }
    if (format.signed_output && format.value > (uint64_t)INT64_MAX) {
        is_negative = true;
        magnitude = 0U - format.value;
    }
    if (is_negative) {
        if (buffer_size < 2U) {
            mylite_execution_set_runtime_error(database, "failed to format CONV() value");
            return MYLITE_ERROR;
        }
        buffer[0] = '-';
        rc = mylite_execution_scalar_binary_format_base_conversion_value(
            database,
            magnitude,
            format.base,
            buffer + 1U,
            buffer_size - 1U
        );
    } else {
        rc = mylite_execution_scalar_binary_format_base_conversion_value(
            database,
            magnitude,
            format.base,
            buffer,
            buffer_size
        );
    }
    return rc;
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
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = mylite_execution_scalar_binary_evaluate_base_conversion_direct_literal_operand(
        database,
        expression,
        out_value,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (mylite_execution_is_scalar_bitwise_projection_expression(expression)) {
        return mylite_execution_evaluate_scalar_bitwise_expression(database, expression, out_value);
    }
    if (!mylite_execution_is_scalar_arithmetic_projection_expression(expression)) {
        mylite_execution_set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = mylite_execution_evaluate_scalar_arithmetic_expression(database, expression, &arithmetic);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_value->is_null = arithmetic.is_null;
    out_value->integer = (uint64_t)arithmetic.integer;
    out_value->division_by_zero_warning_count = arithmetic.division_by_zero_warning_count;
    return MYLITE_OK;
}

static uint64_t count_uint64_bits(uint64_t value) {
    uint64_t count = 0U;

    while (value != 0U) {
        count += value & 1U;
        value >>= 1U;
    }
    return count;
}

int mylite_execution_scalar_bit_count_function_value(
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
        mylite_execution_set_bit_count_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = mylite_execution_evaluate_bit_count_operand(
        database,
        mylite_execution_child_at(expression, 0U),
        &value
    );
    if (rc != MYLITE_OK || value.is_null) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    bit_count = count_uint64_bits(value.integer);
    rc = mylite_execution_format_uint64(
        database,
        bit_count,
        out_cell->integer_text,
        sizeof(out_cell->integer_text)
    );
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->integer_text;
        out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
    }
    return rc;
}
