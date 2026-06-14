#include "mylite_execution_scalar_binary_internal.h"

#include "mylite_string_char.h"

static int char_function_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
);
static int char_function_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool has_sign,
    bool is_negative,
    struct scalar_bitwise_value *out_value
);
static int char_function_integer_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    struct scalar_bitwise_value *out_value
);
static int char_function_unknown_column_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);

int mylite_execution_scalar_char_function_value(
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
        mylite_execution_set_unsupported_error(
            database,
            "CHAR() supports one or more integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }

    arguments = mylite_execution_child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST ||
        mylite_sql_ast_node_child_count(arguments) == 0U) {
        mylite_execution_set_unsupported_error(
            database,
            "CHAR() supports one or more integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }

    for (const struct mylite_sql_ast_node *argument = mylite_execution_child_at(arguments, 0U);
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
        mylite_execution_set_nomem_error(database);
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
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    literal = expression;
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            literal = NULL;
        } else {
            has_sign = true;
            is_negative = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
            literal = mylite_execution_unwrap_parenthesized_expression(
                mylite_execution_child_at(expression, 0U)
            );
        }
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        if (expression != NULL && (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
                                   expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
            return char_function_unknown_column_argument(database, expression);
        }
        mylite_execution_set_unsupported_error(
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

    mylite_execution_set_unsupported_error(
        database,
        "CHAR() supports only integer, boolean, and NULL arguments"
    );
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
    if (mylite_execution_parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        mylite_execution_set_unsupported_error(
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
        mylite_execution_set_unsupported_error(
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
    return mylite_execution_set_unknown_column_reference_error(database, expression);
}
