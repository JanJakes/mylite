#include "mylite_execution_scalar_binary_internal.h"

#include "mylite_random_bytes.h"

static int random_bytes_length_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    size_t *out_length,
    bool *out_is_null
);
static int random_bytes_length_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    size_t *out_length,
    bool *out_is_null
);
static int random_bytes_length_unary(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    size_t *out_length,
    bool *out_is_null
);
static int random_bytes_length_scalar(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    size_t *out_length,
    bool *out_is_null,
    bool *out_handled
);

int mylite_execution_scalar_random_bytes_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    unsigned char *bytes = NULL;
    size_t length = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_RANDOM_BYTES_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_unsupported_error(
            database,
            "RANDOM_BYTES() supports only one length argument"
        );
        return MYLITE_ERROR;
    }

    rc = random_bytes_length_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        out_cell,
        &length,
        &is_null
    );
    if (rc != MYLITE_OK || is_null) {
        return rc;
    }

    rc = mylite_random_bytes_generate(length, &bytes);
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(database, "failed to calculate RANDOM_BYTES() value");
        return rc;
    }

    out_cell->owned_text = (char *)bytes;
    out_cell->value = out_cell->owned_text;
    out_cell->value_size = length;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

static int random_bytes_length_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    size_t *out_length,
    bool *out_is_null
) {
    bool handled_scalar = false;
    int rc = MYLITE_OK;

    if (cell == NULL || out_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_random_bytes_set_length_out_of_range_error(database);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return random_bytes_length_literal(database, expression, out_length, out_is_null);
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return random_bytes_length_unary(database, expression, out_length, out_is_null);
    }

    rc = random_bytes_length_scalar(
        database,
        expression,
        cell,
        out_length,
        out_is_null,
        &handled_scalar
    );
    if (rc != MYLITE_OK || handled_scalar) {
        return rc;
    }

    mylite_execution_set_unsupported_error(
        database,
        "RANDOM_BYTES() supports only integer, decimal, string, binary, boolean, NULL, "
        "supported session scalar, and supported system variable length arguments"
    );
    return MYLITE_ERROR;
}

static int random_bytes_length_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    size_t *out_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *text = NULL;
    char *bytes = NULL;
    size_t byte_count = 0U;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (literal == NULL || out_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_length = 0U;
    *out_is_null = false;

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        return mylite_random_bytes_length_from_int64(
            database,
            literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ? 1 : 0,
            out_length
        );
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        if (mylite_execution_parse_unsigned_integer_literal(&literal->span, &magnitude) !=
                MYLITE_OK ||
            magnitude > (uint64_t)INT64_MAX) {
            mylite_random_bytes_set_length_out_of_range_error(database);
            return MYLITE_ERROR;
        }
        return mylite_random_bytes_length_from_int64(database, (int64_t)magnitude, out_length);
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT) {
        rc = mylite_execution_copy_source_span_text(database, &literal->span, &text);
        if (rc == MYLITE_OK) {
            rc = mylite_random_bytes_length_from_text(database, text, strlen(text), out_length);
        }
        free(text);
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = mylite_execution_decode_sql_string_literal_with_policy(
            database,
            literal,
            "RANDOM_BYTES() supports only integer, decimal, string, binary, boolean, NULL, "
            "supported session scalar, and supported system variable length arguments",
            "RANDOM_BYTES() string literal is invalid",
            true,
            &text,
            &byte_count
        );
        if (rc == MYLITE_OK) {
            rc = mylite_random_bytes_length_from_text(database, text, byte_count, out_length);
        }
        free(text);
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_HEX) {
        rc = mylite_execution_decode_binary_hex_literal(database, literal, &bytes, &byte_count);
        if (rc == MYLITE_OK) {
            rc = mylite_random_bytes_length_from_text(database, bytes, byte_count, out_length);
        }
        free(bytes);
        return rc;
    }

    mylite_execution_set_unsupported_error(
        database,
        "RANDOM_BYTES() supports only integer, decimal, string, binary, boolean, and NULL "
        "literal length arguments"
    );
    return MYLITE_ERROR;
}

static int random_bytes_length_unary(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    size_t *out_length,
    bool *out_is_null
) {
    const struct mylite_sql_ast_node *literal = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *text = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || out_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_length = 0U;
    *out_is_null = false;

    operator_kind = mylite_sql_ast_node_operator(expression);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        mylite_random_bytes_set_length_out_of_range_error(database);
        return MYLITE_ERROR;
    }

    literal =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        mylite_execution_set_unsupported_error(
            database,
            "RANDOM_BYTES() supports only signed numeric literal length arguments"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER &&
        literal_kind != MYLITE_SQL_AST_LITERAL_DECIMAL &&
        literal_kind != MYLITE_SQL_AST_LITERAL_FLOAT) {
        mylite_execution_set_unsupported_error(
            database,
            "RANDOM_BYTES() supports only signed numeric literal length arguments"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_execution_copy_source_span_text(database, &expression->span, &text);
    if (rc == MYLITE_OK) {
        rc = mylite_random_bytes_length_from_text(database, text, strlen(text), out_length);
    }
    free(text);
    return rc;
}

static int random_bytes_length_scalar(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    size_t *out_length,
    bool *out_is_null,
    bool *out_handled
) {
    bool handled_scalar = false;
    int rc = MYLITE_OK;

    if (cell == NULL || out_length == NULL || out_is_null == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_length = 0U;
    *out_is_null = false;
    *out_handled = false;

    rc = mylite_execution_scalar_binary_argument_scalar_value(
        database,
        expression,
        cell,
        &handled_scalar
    );
    if (rc != MYLITE_OK || !handled_scalar) {
        return rc;
    }
    *out_handled = true;
    if (cell->value == NULL) {
        *out_is_null = true;
        mylite_execution_session_scalar_cell_deinit(cell);
        return MYLITE_OK;
    }

    rc = mylite_random_bytes_length_from_text(
        database,
        cell->value,
        cell->has_value_size ? cell->value_size : strlen(cell->value),
        out_length
    );
    mylite_execution_session_scalar_cell_deinit(cell);
    return rc;
}
