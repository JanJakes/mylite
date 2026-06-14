#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_numeric.h"

#include "mylite_ast.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    decimal_base = 10,
    literal_projection_max_significant_digits = 81,
    scalar_exact_decimal_part_capacity = literal_projection_max_significant_digits + 1,
    scalar_format_max_decimals = 30,
};

struct scalar_exact_decimal {
    bool is_null;
    bool is_negative;
    char integer_digits[scalar_exact_decimal_part_capacity + 1U];
    size_t integer_length;
    char fraction_digits[scalar_exact_decimal_part_capacity + 1U];
    size_t fraction_length;
};

struct scalar_decimal_places {
    bool is_null;
    bool is_negative;
    bool overflowed;
    uint64_t magnitude;
};

static int scalar_format_truncate_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    struct scalar_exact_decimal *out_value,
    struct scalar_decimal_places *out_places
);
static int scalar_exact_decimal_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    struct scalar_exact_decimal *out_decimal
);
static int scalar_decimal_places_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    struct scalar_decimal_places *out_places
);
static int parse_scalar_exact_decimal_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    bool is_negative,
    const char *function_name,
    struct scalar_exact_decimal *out_decimal
);
static int parse_scalar_exact_decimal_dot_index(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    size_t *out_dot_index
);
static int assign_scalar_exact_decimal_integer(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    size_t dot_index,
    const char *function_name,
    struct scalar_exact_decimal *out_decimal
);
static int assign_scalar_exact_decimal_fraction(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    size_t dot_index,
    const char *function_name,
    struct scalar_exact_decimal *out_decimal
);
static int parse_scalar_decimal_places_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    bool is_negative,
    const char *function_name,
    struct scalar_decimal_places *out_places
);
static int assign_format_function_text(
    struct mylite_db *database,
    const struct scalar_exact_decimal *value,
    const struct scalar_decimal_places *places,
    struct session_scalar_cell *out_cell
);
static size_t format_decimal_place_count(const struct scalar_decimal_places *places);
static void copy_format_fraction_digits(
    const struct scalar_exact_decimal *value,
    size_t decimal_places,
    char *out_fraction_digits
);
static int round_format_decimal_digits(
    struct mylite_db *database,
    const struct scalar_exact_decimal *value,
    size_t decimal_places,
    char *integer_digits,
    size_t *in_out_integer_length,
    char *fraction_digits
);
static int assign_format_output_text(
    struct mylite_db *database,
    bool is_negative,
    const char *integer_digits,
    size_t integer_length,
    const char *fraction_digits,
    size_t decimal_places,
    struct session_scalar_cell *out_cell
);
static int assign_truncate_function_text(
    struct mylite_db *database,
    const struct scalar_exact_decimal *value,
    const struct scalar_decimal_places *places,
    struct session_scalar_cell *out_cell
);
static void set_format_truncate_unsupported_error(
    struct mylite_db *database,
    const char *function_name
);
static bool scalar_decimal_parts_are_zero(
    const char *integer_digits,
    size_t integer_digit_count,
    const char *fraction_digits,
    size_t fraction_digit_count
);
static bool scalar_decimal_digits_are_zero(const char *digits, size_t digit_count);

int mylite_execution_scalar_format_function_value(
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
        mylite_execution_set_format_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = scalar_format_truncate_arguments(database, expression, "FORMAT", &value, &places);
    if (rc == MYLITE_OK) {
        rc = assign_format_function_text(database, &value, &places, out_cell);
    }
    return rc;
}

int mylite_execution_scalar_truncate_function_value(
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
        mylite_execution_set_truncate_unsupported_error(database);
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
    rc = scalar_exact_decimal_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        function_name,
        out_value
    );
    if (rc == MYLITE_OK) {
        rc = scalar_decimal_places_argument(
            database,
            mylite_execution_child_at(expression, 1U),
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
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
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
        literal = mylite_execution_unwrap_parenthesized_expression(
            mylite_execution_child_at(expression, 0U)
        );
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
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
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
        literal = mylite_execution_unwrap_parenthesized_expression(
            mylite_execution_child_at(expression, 0U)
        );
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
                mylite_execution_set_parse_error(database);
                return MYLITE_ERROR;
            }
            dot_index = index;
        } else if (byte < '0' || byte > '9') {
            mylite_execution_set_parse_error(database);
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
            mylite_execution_set_format_unsupported_error(database);
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
        mylite_execution_set_nomem_error(database);
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
        mylite_execution_set_nomem_error(database);
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
        mylite_execution_set_format_unsupported_error(database);
        return;
    }
    mylite_execution_set_truncate_unsupported_error(database);
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
