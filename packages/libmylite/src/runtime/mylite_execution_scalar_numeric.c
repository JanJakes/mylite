#include "mylite_execution_scalar_numeric.h"
#include "mylite_execution_scalar.h"

#include "mylite_ast.h"

#include <mylite/mylite.h>

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    decimal_base = 10,
    avg_fraction_digits = 4,
    avg_fraction_scale = 10000,
    avg_round_half_digit = 5,
    rounding_negative_places_zero_threshold = 20,
};

struct rounding_signed_value {
    bool is_null;
    bool is_negative;
    uint64_t magnitude;
    size_t staged_division_by_zero_warning_count;
};

struct uint128_parts {
    uint64_t high;
    uint64_t low;
};

static bool scalar_division_left_null_short_circuits(
    const struct mylite_sql_ast_node *expression,
    const struct scalar_arithmetic_value *left
);
static int format_scalar_division_value(
    struct mylite_db *database,
    int64_t numerator,
    int64_t denominator,
    char *buffer,
    size_t buffer_size
);
static int evaluate_abs_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
);
static int evaluate_abs_direct_literal_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value,
    bool *out_handled
);
static int evaluate_sign_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
);
static int evaluate_sign_direct_literal_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value,
    bool *out_handled
);
static int sign_of_decimal_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    bool is_negative,
    struct scalar_arithmetic_value *out_value
);
static int evaluate_rounding_places_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
);
static int evaluate_rounding_signed_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct rounding_signed_value *out_value
);
static int evaluate_rounding_direct_signed_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct rounding_signed_value *out_value,
    bool *out_handled
);
static int round_signed_value_to_negative_places(
    struct mylite_db *database,
    const struct rounding_signed_value *value,
    int64_t places,
    struct session_scalar_cell *out_cell
);
static int evaluate_rounding_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_rounding_direct_literal_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    bool *out_handled
);
static uint64_t absolute_int64_magnitude(int64_t value);
static int next_decimal_digit(uint64_t *remainder, uint64_t denominator);
static struct uint128_parts multiply_u64_by_decimal_radix(uint64_t value);
static bool uint128_ge_u64(const struct uint128_parts *left, uint64_t right);
static void uint128_subtract_u64(struct uint128_parts *left, uint64_t right);

int mylite_execution_scalar_division_value(
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
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!mylite_execution_is_scalar_division_projection_expression(expression)) {
        mylite_execution_set_scalar_division_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = mylite_execution_evaluate_scalar_arithmetic_expression(
        database,
        mylite_execution_child_at(expression, 0U),
        &left
    );
    if (rc == MYLITE_OK && scalar_division_left_null_short_circuits(
                               mylite_execution_child_at(expression, 0U),
                               &left
                           )) {
        out_cell->staged_division_by_zero_warning_count = left.division_by_zero_warning_count;
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_evaluate_scalar_arithmetic_expression(
            database,
            mylite_execution_child_at(expression, 1U),
            &right
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_execution_accumulate_staged_division_by_zero_warnings(
        database,
        left.division_by_zero_warning_count,
        &warning_count
    );
    if (rc == MYLITE_OK) {
        rc = mylite_execution_accumulate_staged_division_by_zero_warnings(
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
        rc = mylite_execution_accumulate_staged_division_by_zero_warnings(
            database,
            1U,
            &warning_count
        );
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

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
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
            mylite_execution_set_runtime_error(database, "failed to format scalar division value");
            return MYLITE_ERROR;
        }
        fraction = (fraction * decimal_base) + (unsigned int)digit;
    }
    round_digit = next_decimal_digit(&remainder, denominator_magnitude);
    if (round_digit < 0) {
        mylite_execution_set_runtime_error(database, "failed to format scalar division value");
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
        mylite_execution_set_runtime_error(database, "failed to format scalar division value");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

int mylite_execution_scalar_bitwise_value(
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
    rc = mylite_execution_evaluate_scalar_bitwise_expression(database, expression, &value);
    if (rc != MYLITE_OK || value.is_null) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    rc = mylite_execution_format_uint64(
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

int mylite_execution_scalar_abs_function_value(
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
        mylite_execution_set_abs_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_abs_operand(database, mylite_execution_child_at(expression, 0U), &value);
    if (rc != MYLITE_OK || value.is_null) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    rc = mylite_execution_format_uint64(
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
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_abs_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_abs_direct_literal_operand(database, expression, out_value, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (mylite_execution_is_scalar_bitwise_projection_expression(expression)) {
        return mylite_execution_evaluate_scalar_bitwise_expression(database, expression, out_value);
    }
    if (!mylite_execution_is_scalar_arithmetic_projection_expression(expression)) {
        mylite_execution_set_abs_unsupported_error(database);
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
    if (arithmetic.integer == INT64_MIN) {
        mylite_execution_set_abs_signed_minimum_overflow_error(database);
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

    if (mylite_execution_parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        mylite_execution_set_abs_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (is_negative && magnitude == int64_min_magnitude) {
        mylite_execution_set_abs_signed_minimum_overflow_error(database);
        return MYLITE_ERROR;
    }
    out_value->integer = magnitude;
    return MYLITE_OK;
}

int mylite_execution_scalar_sign_function_value(
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
        mylite_execution_set_sign_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_sign_operand(database, mylite_execution_child_at(expression, 0U), &value);
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
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_sign_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_sign_direct_literal_operand(database, expression, out_value, &handled);
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
        out_value->division_by_zero_warning_count = bitwise.division_by_zero_warning_count;
        if (!bitwise.is_null) {
            out_value->integer = bitwise.integer == 0U ? 0 : 1;
        }
        return MYLITE_OK;
    }
    if (!mylite_execution_is_scalar_arithmetic_projection_expression(expression)) {
        mylite_execution_set_sign_unsupported_error(database);
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
        mylite_execution_set_sign_unsupported_error(database);
        return MYLITE_ERROR;
    }

    for (size_t index = 0U; index < span->length; ++index) {
        unsigned char byte = (unsigned char)span->text[index];

        if (byte < '0' || byte > '9') {
            mylite_execution_set_sign_unsupported_error(database);
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

int mylite_execution_scalar_rounding_function_value(
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
        mylite_execution_set_rounding_unsupported_error(database);
        return MYLITE_ERROR;
    }

    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count == 1U) {
        return evaluate_rounding_operand(
            database,
            mylite_execution_child_at(expression, 0U),
            out_cell
        );
    }
    if (child_count != 2U) {
        mylite_execution_set_rounding_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_rounding_places_operand(
        database,
        mylite_execution_child_at(expression, 1U),
        &places
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (places.is_null || places.integer >= 0) {
        rc = evaluate_rounding_operand(
            database,
            mylite_execution_child_at(expression, 0U),
            out_cell
        );
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

    rc = evaluate_rounding_signed_operand(
        database,
        mylite_execution_child_at(expression, 0U),
        &value
    );
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
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){.is_null = false, .integer = 0};
    if (expression == NULL ||
        !mylite_execution_is_scalar_arithmetic_projection_expression(expression)) {
        mylite_execution_set_rounding_unsupported_error(database);
        return MYLITE_ERROR;
    }
    return mylite_execution_evaluate_scalar_arithmetic_expression(database, expression, out_value);
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
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_rounding_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_rounding_direct_signed_operand(database, expression, out_value, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (mylite_execution_is_scalar_arithmetic_projection_expression(expression)) {
        struct scalar_arithmetic_value arithmetic = {.is_null = false, .integer = 0};

        rc = mylite_execution_evaluate_scalar_arithmetic_expression(
            database,
            expression,
            &arithmetic
        );
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
    if (mylite_execution_is_scalar_bitwise_projection_expression(expression)) {
        struct scalar_bitwise_value bitwise = {.is_null = false, .integer = 0U};

        rc = mylite_execution_evaluate_scalar_bitwise_expression(database, expression, &bitwise);
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_value->is_null = bitwise.is_null;
        out_value->staged_division_by_zero_warning_count = bitwise.division_by_zero_warning_count;
        if (!bitwise.is_null) {
            if (bitwise.integer > (uint64_t)INT64_MAX) {
                mylite_execution_set_rounding_unsupported_error(database);
                return MYLITE_ERROR;
            }
            out_value->magnitude = bitwise.integer;
        }
        return MYLITE_OK;
    }

    mylite_execution_set_rounding_unsupported_error(database);
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
    if (mylite_execution_parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        mylite_execution_set_rounding_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (is_negative) {
        if (magnitude > (uint64_t)INT64_MAX + 1U) {
            mylite_execution_set_rounding_unsupported_error(database);
            return MYLITE_ERROR;
        }
        out_value->is_negative = magnitude != 0U;
    } else if (magnitude > (uint64_t)INT64_MAX) {
        mylite_execution_set_rounding_unsupported_error(database);
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
        return mylite_execution_set_rounding_signed_overflow_error(database);
    }
    rounded = quotient * divisor;
    if (rounded == 0U) {
        out_cell->value = "0";
        return MYLITE_OK;
    }
    if (rounded > (uint64_t)INT64_MAX) {
        return mylite_execution_set_rounding_signed_overflow_error(database);
    }

    if (value->is_negative) {
        written =
            snprintf(out_cell->integer_text, sizeof(out_cell->integer_text), "-%" PRIu64, rounded);
    } else {
        written =
            snprintf(out_cell->integer_text, sizeof(out_cell->integer_text), "%" PRIu64, rounded);
    }
    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        mylite_execution_set_runtime_error(database, "failed to format scalar rounding value");
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
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_rounding_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_rounding_direct_literal_operand(database, expression, out_cell, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (mylite_execution_is_scalar_bitwise_projection_expression(expression)) {
        struct scalar_bitwise_value bitwise = {.is_null = false, .integer = 0U};

        rc = mylite_execution_evaluate_scalar_bitwise_expression(database, expression, &bitwise);
        if (rc != MYLITE_OK || bitwise.is_null) {
            if (rc == MYLITE_OK) {
                out_cell->staged_division_by_zero_warning_count =
                    bitwise.division_by_zero_warning_count;
            }
            return rc;
        }
        rc = mylite_execution_format_uint64(
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
    if (mylite_execution_is_scalar_arithmetic_projection_expression(expression)) {
        struct scalar_arithmetic_value arithmetic = {.is_null = false, .integer = 0};
        int written = 0;

        rc = mylite_execution_evaluate_scalar_arithmetic_expression(
            database,
            expression,
            &arithmetic
        );
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
            mylite_execution_set_runtime_error(database, "failed to format scalar rounding value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        out_cell->staged_division_by_zero_warning_count = arithmetic.division_by_zero_warning_count;
        return MYLITE_OK;
    }

    mylite_execution_set_rounding_unsupported_error(database);
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

    rc = mylite_execution_normalize_decimal_integer_literal(
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

static uint64_t absolute_int64_magnitude(int64_t value) {
    const uint64_t int64_negative_abs_max = 9223372036854775808ULL;

    if (value >= 0) {
        return (uint64_t)value;
    }
    if (value == INT64_MIN) {
        return int64_negative_abs_max;
    }

    return (uint64_t)-value;
}

static int next_decimal_digit(uint64_t *remainder, uint64_t denominator) {
    struct uint128_parts product = {0};
    int digit = 0;

    if (remainder == NULL || denominator == 0U || *remainder >= denominator) {
        return -1;
    }

    product = multiply_u64_by_decimal_radix(*remainder);
    while (uint128_ge_u64(&product, denominator)) {
        uint128_subtract_u64(&product, denominator);
        ++digit;
    }

    *remainder = product.low;
    return digit;
}

static struct uint128_parts multiply_u64_by_decimal_radix(uint64_t value) {
    struct uint128_parts product = {0};

    for (unsigned int index = 0U; index < decimal_base; ++index) {
        uint64_t previous_low = product.low;

        product.low += value;
        if (product.low < previous_low) {
            ++product.high;
        }
    }

    return product;
}

static bool uint128_ge_u64(const struct uint128_parts *left, uint64_t right) {
    if (left->high != 0U) {
        return true;
    }

    return left->low >= right;
}

static void uint128_subtract_u64(struct uint128_parts *left, uint64_t right) {
    uint64_t previous_low = left->low;

    left->low -= right;
    if (previous_low < right) {
        --left->high;
    }
}
