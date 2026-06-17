#include "mylite_execution_scalar_binary_internal.h"

#include "mylite_string_unhex.h"

static int unhex_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);
static int unhex_literal_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);
static int unhex_unary_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);
static int unhex_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    bool *out_handled
);
static int stage_unhex_incorrect_string_warning(
    struct mylite_db *database,
    struct session_scalar_cell *cell,
    const void *input,
    size_t input_size
);

int mylite_execution_scalar_unhex_function_value(
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
        mylite_execution_set_unsupported_error(
            database,
            "UNHEX() supports only one string, hex, integer, boolean, NULL, supported session "
            "scalar, or supported system variable argument"
        );
        return MYLITE_ERROR;
    }

    argument =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
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
            mylite_execution_set_nomem_error(database);
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

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
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
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return mylite_execution_set_unknown_column_reference_error(database, expression);
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

    mylite_execution_set_unsupported_error(
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
        rc = mylite_execution_copy_source_span_text(database, &literal->span, out_owned_bytes);
        if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)*out_owned_bytes;
            *out_byte_count = strlen(*out_owned_bytes);
        }
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = mylite_execution_decode_sql_string_literal_with_policy(
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
        rc = mylite_execution_decode_binary_hex_literal(
            database,
            literal,
            out_owned_bytes,
            out_byte_count
        );
        if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)*out_owned_bytes;
        }
        return rc;
    }

    mylite_execution_set_unsupported_error(
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
        mylite_execution_set_unsupported_error(
            database,
            "UNHEX() supports only signed integer literal arguments"
        );
        return MYLITE_ERROR;
    }

    literal =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        mylite_execution_set_unsupported_error(
            database,
            "UNHEX() supports only signed integer literal arguments"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_execution_copy_source_span_text(
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
    return mylite_execution_scalar_binary_scalar_argument_value(
        database,
        expression,
        out_cell,
        out_handled
    );
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
        mylite_execution_set_runtime_error(database, "failed to format UNHEX() warning input");
        return MYLITE_ERROR;
    }
    cell->has_staged_unhex_incorrect_string_warning = true;
    return MYLITE_OK;
}
