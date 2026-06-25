#include "mylite_execution_scalar_binary_internal.h"

#include "mylite_uuid.h"

static int uuid_generate_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int uuid_short_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int is_uuid_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int uuid_to_bin_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int bin_to_uuid_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int uuid_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);
static int uuid_direct_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);
static int uuid_to_bin_nested_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);
static int uuid_literal_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);
static int uuid_unary_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);
static int uuid_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    bool *out_handled
);
static int uuid_swap_flag_unary_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_swap
);
static int uuid_unknown_column_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);

int mylite_execution_scalar_uuid_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_scalar_set_uuid_unsupported_error(database, "UUID conversion");
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_UUID_FUNCTION:
        return uuid_generate_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_UUID_SHORT_FUNCTION:
        return uuid_short_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_IS_UUID_FUNCTION:
        return is_uuid_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION:
        return uuid_to_bin_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION:
        return bin_to_uuid_function_value(database, expression, out_cell);
    default:
        break;
    }

    mylite_execution_scalar_set_uuid_unsupported_error(database, "UUID conversion");
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
        mylite_execution_set_native_function_parameter_count_error(database, "UUID");
        return MYLITE_ERROR;
    }

    rc = mylite_uuid_generate(database, uuid_text);
    if (rc == MYLITE_OK) {
        rc = mylite_execution_duplicate_text(database, uuid_text, &out_cell->owned_text);
    }
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->owned_text;
    }
    return rc;
}

static int uuid_short_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    uint64_t value = 0U;
    int written = 0;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_UUID_SHORT_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 0U) {
        mylite_execution_set_native_function_parameter_count_error(database, "UUID_SHORT");
        return MYLITE_ERROR;
    }

    value = mylite_uuid_short_generate(database);
    written = snprintf(out_cell->integer_text, sizeof(out_cell->integer_text), "%" PRIu64, value);
    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        mylite_execution_set_runtime_error(database, "failed to format UUID_SHORT() value");
        return MYLITE_ERROR;
    }
    out_cell->value = out_cell->integer_text;
    return MYLITE_OK;
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
        mylite_execution_set_native_function_parameter_count_error(database, "IS_UUID");
        return MYLITE_ERROR;
    }

    argument =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
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
        mylite_execution_set_native_function_parameter_count_error(database, "UUID_TO_BIN");
        return MYLITE_ERROR;
    }

    argument =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
    swap_argument =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 1U));
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
        rc = mylite_execution_scalar_uuid_swap_flag_value(database, swap_argument, &swap);
    }
    if (rc != MYLITE_OK || is_null) {
        free(owned_bytes);
        return rc;
    }

    rc = mylite_uuid_string_to_binary(bytes, byte_count, swap, uuid_bytes, &valid);
    if (rc != MYLITE_OK) {
        free(owned_bytes);
        if (rc == MYLITE_NOMEM) {
            mylite_execution_set_nomem_error(database);
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
        mylite_execution_set_nomem_error(database);
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
        mylite_execution_set_native_function_parameter_count_error(database, "BIN_TO_UUID");
        return MYLITE_ERROR;
    }

    argument =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
    swap_argument =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 1U));
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
        rc = mylite_execution_scalar_uuid_swap_flag_value(database, swap_argument, &swap);
    }
    if (rc != MYLITE_OK || is_null) {
        free(owned_bytes);
        return rc;
    }

    rc = mylite_uuid_binary_to_string(bytes, byte_count, swap, uuid_text, &valid);
    if (rc != MYLITE_OK) {
        free(owned_bytes);
        if (rc == MYLITE_NOMEM) {
            mylite_execution_set_nomem_error(database);
        }
        return rc;
    }
    if (!valid) {
        rc = mylite_uuid_set_incorrect_string_error(database, bytes, byte_count, "bin_to_uuid");
        free(owned_bytes);
        return rc;
    }

    rc = mylite_execution_duplicate_text(database, uuid_text, &out_cell->owned_text);
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->owned_text;
    }
    free(owned_bytes);
    return rc;
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
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
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

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_scalar_set_uuid_unsupported_error(database, "UUID conversion");
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
                mylite_execution_set_nomem_error(database);
                mylite_execution_session_scalar_cell_deinit(&scalar_cell);
                return MYLITE_NOMEM;
            }
            copy = (char *)malloc(byte_count + 1U);
            if (copy == NULL) {
                mylite_execution_set_nomem_error(database);
                mylite_execution_session_scalar_cell_deinit(&scalar_cell);
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
        mylite_execution_session_scalar_cell_deinit(&scalar_cell);
        return rc;
    }
    mylite_execution_session_scalar_cell_deinit(&scalar_cell);

    mylite_execution_scalar_set_uuid_unsupported_error(database, "UUID conversion");
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
        mylite_execution_set_native_function_parameter_count_error(database, "UUID_TO_BIN");
        return MYLITE_ERROR;
    }

    argument =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
    swap_argument =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 1U));
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
        rc = mylite_execution_scalar_uuid_swap_flag_value(database, swap_argument, &swap);
    }
    if (rc != MYLITE_OK || argument_is_null) {
        free(owned_argument_bytes);
        mylite_execution_session_scalar_cell_deinit(&cell);
        *out_is_null = argument_is_null;
        return rc;
    }

    rc =
        mylite_uuid_string_to_binary(argument_bytes, argument_byte_count, swap, uuid_bytes, &valid);
    if (rc != MYLITE_OK) {
        free(owned_argument_bytes);
        mylite_execution_session_scalar_cell_deinit(&cell);
        if (rc == MYLITE_NOMEM) {
            mylite_execution_set_nomem_error(database);
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
        mylite_execution_session_scalar_cell_deinit(&cell);
        return rc;
    }
    free(owned_argument_bytes);
    mylite_execution_session_scalar_cell_deinit(&cell);

    owned_result_bytes = (char *)malloc(MYLITE_UUID_BINARY_SIZE + 1U);
    if (owned_result_bytes == NULL) {
        mylite_execution_set_nomem_error(database);
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

    mylite_execution_scalar_set_uuid_unsupported_error(database, "UUID conversion");
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
        mylite_execution_scalar_set_uuid_unsupported_error(database, "UUID conversion");
        return MYLITE_ERROR;
    }

    literal =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        mylite_execution_scalar_set_uuid_unsupported_error(database, "UUID conversion");
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

static int uuid_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    bool *out_handled
) {
    if (out_cell == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        *out_handled = false;
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_UUID_FUNCTION) {
        *out_handled = true;
        return uuid_generate_function_value(database, expression, out_cell);
    }
    if (expression->kind == MYLITE_SQL_AST_UUID_SHORT_FUNCTION) {
        *out_handled = true;
        return uuid_short_function_value(database, expression, out_cell);
    }
    return mylite_execution_scalar_binary_scalar_argument_value(
        database,
        expression,
        out_cell,
        out_handled
    );
}

int mylite_execution_scalar_uuid_swap_flag_value(
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

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_scalar_set_uuid_unsupported_error(database, "UUID conversion swap flag");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return uuid_swap_flag_unary_value(database, expression, out_swap);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        mylite_execution_scalar_set_uuid_unsupported_error(database, "UUID conversion swap flag");
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
        if (mylite_execution_parse_unsigned_integer_literal(&literal->span, &magnitude) !=
            MYLITE_OK) {
            mylite_execution_scalar_set_uuid_unsupported_error(
                database,
                "UUID conversion swap flag"
            );
            return MYLITE_ERROR;
        }
        *out_swap = magnitude != 0U;
        return MYLITE_OK;
    default:
        break;
    }

    mylite_execution_scalar_set_uuid_unsupported_error(database, "UUID conversion swap flag");
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
        mylite_execution_scalar_set_uuid_unsupported_error(database, "UUID conversion swap flag");
        return MYLITE_ERROR;
    }
    literal =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER ||
        mylite_execution_parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        mylite_execution_scalar_set_uuid_unsupported_error(database, "UUID conversion swap flag");
        return MYLITE_ERROR;
    }

    *out_swap = magnitude != 0U;
    return MYLITE_OK;
}

static int uuid_unknown_column_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    return mylite_execution_set_unknown_column_reference_error(database, expression);
}

void mylite_execution_scalar_set_uuid_unsupported_error(
    struct mylite_db *database,
    const char *function_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "%s supports only supported UUID conversion scalar arguments",
        function_name == NULL ? "UUID conversion" : function_name
    );

    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_execution_set_unsupported_error(
            database,
            "UUID conversion argument shape is not supported"
        );
        return;
    }
    mylite_execution_set_unsupported_error(database, message);
}
