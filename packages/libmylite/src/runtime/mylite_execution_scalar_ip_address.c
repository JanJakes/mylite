#include "mylite_execution_scalar_binary_internal.h"

#include "mylite_ip_address.h"
#include "mylite_numeric_locale.h"

enum {
    ip_address_scalar_signed_text_capacity = 96,
    ip_address_scalar_negative_value_text_extra = 1,
    ip_address_scalar_negative_value_allocation_extra = 2,
    ip_address_scalar_negative_display_value_index = 2,
    ip_address_scalar_negative_display_text_extra = 3,
    ip_address_scalar_negative_display_allocation_extra = 4,
};

static int inet_aton_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    struct session_scalar_cell *out_cell
);
static int inet_ntoa_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    struct session_scalar_cell *out_cell
);
static int inet_aton_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);
static int inet_aton_literal_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);
static int inet_aton_unary_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes
);
static int inet_ntoa_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct session_scalar_cell *out_cell
);
static int inet_ntoa_unary_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int inet_ntoa_text_value(
    struct mylite_db *database,
    const void *input,
    size_t input_size,
    const void *range_warning_input,
    size_t range_warning_input_size,
    struct session_scalar_cell *out_cell
);
static int inet_ntoa_real_value(
    struct mylite_db *database,
    const char *input_text,
    double value,
    struct session_scalar_cell *out_cell
);
static int set_inet_ntoa_output(
    struct mylite_db *database,
    uint32_t value,
    struct session_scalar_cell *out_cell
);
static int set_owned_text_cell(
    struct mylite_db *database,
    const char *text,
    struct session_scalar_cell *out_cell
);
static int copy_scalar_cell_bytes(
    struct mylite_db *database,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);
static int copy_signed_literal_text(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const struct mylite_sql_ast_node *literal,
    char **out_text
);
static int copy_signed_literal_value_text(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const struct mylite_sql_ast_node *literal,
    char **out_text
);
static bool literal_is_textual_numeric(enum mylite_sql_ast_literal_kind literal_kind);

int mylite_execution_scalar_ip_address_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *argument = NULL;
    const char *function_name = "INET_ATON";

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_INET_NTOA_FUNCTION) {
        function_name = "INET_NTOA";
    }
    if (expression == NULL || mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }

    argument = mylite_execution_child_at(expression, 0U);
    switch (expression->kind) {
    case MYLITE_SQL_AST_INET_ATON_FUNCTION:
        return inet_aton_value(database, argument, out_cell);
    case MYLITE_SQL_AST_INET_NTOA_FUNCTION:
        return inet_ntoa_value(database, argument, out_cell);
    default:
        break;
    }

    mylite_execution_set_unsupported_error(database, "unsupported IP address scalar function");
    return MYLITE_ERROR;
}

static int inet_aton_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell argument_cell = {0};
    const unsigned char *bytes = NULL;
    char *owned_bytes = NULL;
    size_t byte_count = 0U;
    uint32_t value = 0U;
    bool is_null = false;
    bool valid = false;
    int rc = MYLITE_OK;

    rc = inet_aton_argument_bytes(
        database,
        argument,
        &argument_cell,
        &bytes,
        &byte_count,
        &owned_bytes,
        &is_null
    );
    if (rc == MYLITE_OK && !is_null) {
        rc = mylite_ip_address_parse_inet_aton(bytes, byte_count, &value, &valid);
    }
    if (rc == MYLITE_OK && !is_null && !valid) {
        rc = mylite_ip_address_append_inet_aton_warning(database, bytes, byte_count);
        is_null = true;
    }
    if (rc == MYLITE_OK && !is_null) {
        rc = mylite_execution_format_session_scalar_uint64_value(database, value, out_cell);
    }

    free(owned_bytes);
    mylite_execution_session_scalar_cell_deinit(&argument_cell);
    return rc;
}

static int inet_ntoa_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell scalar_cell = {0};
    const struct mylite_sql_ast_node *expression =
        mylite_execution_unwrap_parenthesized_expression(argument);
    bool handled_scalar = false;
    int rc = MYLITE_OK;

    if (expression == NULL) {
        mylite_execution_set_unsupported_error(database, "INET_NTOA() argument is unsupported");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return inet_ntoa_literal_value(database, expression, out_cell);
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return inet_ntoa_unary_value(database, expression, out_cell);
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return mylite_execution_set_unknown_column_reference_error(database, expression);
    }

    rc = mylite_execution_scalar_binary_argument_scalar_value(
        database,
        expression,
        &scalar_cell,
        &handled_scalar
    );
    if (rc != MYLITE_OK || !handled_scalar) {
        if (rc == MYLITE_OK) {
            mylite_execution_set_unsupported_error(
                database,
                "INET_NTOA() supports only integer, decimal, string, binary, boolean, NULL, "
                "supported session scalar, and supported system variable arguments"
            );
            rc = MYLITE_ERROR;
        }
        mylite_execution_session_scalar_cell_deinit(&scalar_cell);
        return rc;
    }
    if (scalar_cell.value == NULL) {
        mylite_execution_session_scalar_cell_deinit(&scalar_cell);
        return MYLITE_OK;
    }

    rc = inet_ntoa_text_value(
        database,
        scalar_cell.value,
        scalar_cell.has_value_size ? scalar_cell.value_size : strlen(scalar_cell.value),
        scalar_cell.value,
        scalar_cell.has_value_size ? scalar_cell.value_size : strlen(scalar_cell.value),
        out_cell
    );
    mylite_execution_session_scalar_cell_deinit(&scalar_cell);
    return rc;
}

static int inet_aton_argument_bytes(
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
        mylite_execution_set_unsupported_error(database, "INET_ATON() argument is unsupported");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return inet_aton_literal_bytes(
            database,
            expression,
            out_bytes,
            out_byte_count,
            out_owned_bytes,
            out_is_null
        );
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return inet_aton_unary_bytes(
            database,
            expression,
            out_bytes,
            out_byte_count,
            out_owned_bytes
        );
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return mylite_execution_set_unknown_column_reference_error(database, expression);
    }

    rc = mylite_execution_scalar_binary_argument_scalar_value(
        database,
        expression,
        cell,
        &handled_scalar
    );
    if (rc != MYLITE_OK || !handled_scalar) {
        if (rc == MYLITE_OK) {
            mylite_execution_set_unsupported_error(
                database,
                "INET_ATON() supports only string, hex, integer, decimal, boolean, NULL, "
                "supported session scalar, and supported system variable arguments"
            );
            rc = MYLITE_ERROR;
        }
        return rc;
    }

    return copy_scalar_cell_bytes(
        database,
        cell,
        out_bytes,
        out_byte_count,
        out_owned_bytes,
        out_is_null
    );
}

static int inet_aton_literal_bytes(
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
    if (literal_is_textual_numeric(literal_kind)) {
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
            "INET_ATON() supports only string, hex, integer, decimal, boolean, NULL, supported "
            "session scalar, and supported system variable arguments",
            "INET_ATON() string literal is invalid",
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
        "INET_ATON() supports only string, hex, integer, decimal, boolean, and NULL literal "
        "arguments"
    );
    return MYLITE_ERROR;
}

static int inet_aton_unary_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes
) {
    const struct mylite_sql_ast_node *literal = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (expression == NULL || out_bytes == NULL || out_byte_count == NULL ||
        out_owned_bytes == NULL) {
        return MYLITE_MISUSE;
    }

    operator_kind = mylite_sql_ast_node_operator(expression);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        mylite_execution_set_unsupported_error(
            database,
            "INET_ATON() unary argument is unsupported"
        );
        return MYLITE_ERROR;
    }
    literal =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        mylite_execution_set_unsupported_error(
            database,
            "INET_ATON() unary argument is unsupported"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (!literal_is_textual_numeric(literal_kind)) {
        mylite_execution_set_unsupported_error(
            database,
            "INET_ATON() unary argument is unsupported"
        );
        return MYLITE_ERROR;
    }

    rc = copy_signed_literal_text(database, operator_kind, literal, out_owned_bytes);
    if (rc == MYLITE_OK) {
        *out_bytes = (const unsigned char *)*out_owned_bytes;
        *out_byte_count = strlen(*out_owned_bytes);
    }
    return rc;
}

static int inet_ntoa_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct session_scalar_cell *out_cell
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *text = NULL;
    char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = MYLITE_OK;

    if (literal == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        return set_inet_ntoa_output(
            database,
            literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ? 1U : 0U,
            out_cell
        );
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        rc = mylite_execution_copy_source_span_text(database, &literal->span, &text);
        if (rc == MYLITE_OK) {
            rc = inet_ntoa_text_value(database, text, strlen(text), text, strlen(text), out_cell);
        }
        free(text);
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT) {
        rc = mylite_execution_copy_source_span_text(database, &literal->span, &text);
        if (rc == MYLITE_OK) {
            rc = inet_ntoa_real_value(
                database,
                text,
                mylite_numeric_parse_double(text, NULL),
                out_cell
            );
        }
        free(text);
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = mylite_execution_decode_sql_string_literal_with_policy(
            database,
            literal,
            "INET_NTOA() supports only integer, decimal, string, binary, boolean, NULL, "
            "supported session scalar, and supported system variable arguments",
            "INET_NTOA() string literal is invalid",
            true,
            &text,
            &byte_count
        );
        if (rc == MYLITE_OK) {
            rc = inet_ntoa_text_value(
                database,
                text,
                byte_count,
                literal->span.text,
                literal->span.length,
                out_cell
            );
        }
        free(text);
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_HEX) {
        rc = mylite_execution_decode_binary_hex_literal(database, literal, &bytes, &byte_count);
        if (rc == MYLITE_OK) {
            rc = mylite_ip_address_append_inet_ntoa_binary_warning(database, bytes, byte_count);
        }
        if (rc == MYLITE_OK) {
            rc = set_inet_ntoa_output(database, 0U, out_cell);
        }
        free(bytes);
        return rc;
    }

    mylite_execution_set_unsupported_error(
        database,
        "INET_NTOA() supports only integer, decimal, string, binary, boolean, and NULL literal "
        "arguments"
    );
    return MYLITE_ERROR;
}

static int inet_ntoa_unary_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *literal = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *display_text = NULL;
    char *value_text = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }

    operator_kind = mylite_sql_ast_node_operator(expression);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        mylite_execution_set_unsupported_error(
            database,
            "INET_NTOA() unary argument is unsupported"
        );
        return MYLITE_ERROR;
    }
    literal =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        mylite_execution_set_unsupported_error(
            database,
            "INET_NTOA() unary argument is unsupported"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (!literal_is_textual_numeric(literal_kind)) {
        mylite_execution_set_unsupported_error(
            database,
            "INET_NTOA() unary argument is unsupported"
        );
        return MYLITE_ERROR;
    }

    rc = copy_signed_literal_value_text(database, operator_kind, literal, &value_text);
    if (rc == MYLITE_OK) {
        rc = copy_signed_literal_text(database, operator_kind, literal, &display_text);
    }
    if (rc == MYLITE_OK && literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        rc = inet_ntoa_text_value(
            database,
            value_text,
            strlen(value_text),
            display_text,
            strlen(display_text),
            out_cell
        );
    } else if (rc == MYLITE_OK) {
        rc = inet_ntoa_real_value(
            database,
            display_text,
            mylite_numeric_parse_double(value_text, NULL),
            out_cell
        );
    }
    free(display_text);
    free(value_text);
    return rc;
}

static int inet_ntoa_text_value(
    struct mylite_db *database,
    const void *input,
    size_t input_size,
    const void *range_warning_input,
    size_t range_warning_input_size,
    struct session_scalar_cell *out_cell
) {
    uint32_t value = 0U;
    bool out_of_range = false;
    bool truncated = false;
    int rc = MYLITE_OK;

    rc = mylite_ip_address_parse_inet_ntoa_integer_text(
        input,
        input_size,
        &value,
        &out_of_range,
        &truncated
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (truncated) {
        rc = mylite_ip_address_append_inet_ntoa_truncated_integer_warning(
            database,
            input,
            input_size
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    if (out_of_range) {
        return mylite_ip_address_append_inet_ntoa_range_warning(
            database,
            range_warning_input,
            range_warning_input_size
        );
    }
    return set_inet_ntoa_output(database, value, out_cell);
}

static int inet_ntoa_real_value(
    struct mylite_db *database,
    const char *input_text,
    double value,
    struct session_scalar_cell *out_cell
) {
    uint32_t rounded = 0U;
    bool out_of_range = false;
    int rc = MYLITE_OK;

    rc = mylite_ip_address_round_inet_ntoa_real(value, &rounded, &out_of_range);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_of_range) {
        return mylite_ip_address_append_inet_ntoa_range_warning(
            database,
            input_text,
            input_text == NULL ? 0U : strlen(input_text)
        );
    }
    return set_inet_ntoa_output(database, rounded, out_cell);
}

static int set_inet_ntoa_output(
    struct mylite_db *database,
    uint32_t value,
    struct session_scalar_cell *out_cell
) {
    char text[mylite_ip_address_text_capacity];

    mylite_ip_address_format_inet_ntoa(value, text);
    return set_owned_text_cell(database, text, out_cell);
}

static int set_owned_text_cell(
    struct mylite_db *database,
    const char *text,
    struct session_scalar_cell *out_cell
) {
    size_t text_size = 0U;

    if (text == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }

    text_size = strlen(text);
    out_cell->owned_text = (char *)malloc(text_size + 1U);
    if (out_cell->owned_text == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(out_cell->owned_text, text, text_size + 1U);
    out_cell->value = out_cell->owned_text;
    out_cell->value_size = text_size;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

static int copy_scalar_cell_bytes(
    struct mylite_db *database,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
) {
    size_t byte_count = 0U;

    if (cell == NULL || out_bytes == NULL || out_byte_count == NULL || out_owned_bytes == NULL ||
        out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    if (cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    byte_count = cell->has_value_size ? cell->value_size : strlen(cell->value);
    if (byte_count == SIZE_MAX) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    *out_owned_bytes = (char *)malloc(byte_count + 1U);
    if (*out_owned_bytes == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(*out_owned_bytes, cell->value, byte_count);
    (*out_owned_bytes)[byte_count] = '\0';
    *out_bytes = (const unsigned char *)*out_owned_bytes;
    *out_byte_count = byte_count;
    return MYLITE_OK;
}

static int copy_signed_literal_text(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const struct mylite_sql_ast_node *literal,
    char **out_text
) {
    if (literal == NULL || out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
        return mylite_execution_copy_source_span_text(database, &literal->span, out_text);
    }
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        return MYLITE_MISUSE;
    }
    if (literal->span.length > ip_address_scalar_signed_text_capacity -
                                   ip_address_scalar_negative_display_allocation_extra) {
        mylite_execution_set_runtime_error(database, "signed IP address literal is too long");
        return MYLITE_ERROR;
    }

    *out_text =
        (char *)malloc(literal->span.length + ip_address_scalar_negative_display_allocation_extra);
    if (*out_text == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    (*out_text)[0] = '-';
    (*out_text)[1] = '(';
    memcpy(
        *out_text + ip_address_scalar_negative_display_value_index,
        literal->span.text,
        literal->span.length
    );
    (*out_text)[literal->span.length + ip_address_scalar_negative_display_text_extra - 1U] = ')';
    (*out_text)[literal->span.length + ip_address_scalar_negative_display_text_extra] = '\0';

    return MYLITE_OK;
}

static int copy_signed_literal_value_text(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const struct mylite_sql_ast_node *literal,
    char **out_text
) {
    if (literal == NULL || out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
        return mylite_execution_copy_source_span_text(database, &literal->span, out_text);
    }
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        return MYLITE_MISUSE;
    }
    if (literal->span.length > ip_address_scalar_signed_text_capacity -
                                   ip_address_scalar_negative_value_allocation_extra) {
        mylite_execution_set_runtime_error(database, "signed IP address literal is too long");
        return MYLITE_ERROR;
    }

    *out_text =
        (char *)malloc(literal->span.length + ip_address_scalar_negative_value_allocation_extra);
    if (*out_text == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    (*out_text)[0] = '-';
    memcpy(
        *out_text + ip_address_scalar_negative_value_text_extra,
        literal->span.text,
        literal->span.length
    );
    (*out_text)[literal->span.length + ip_address_scalar_negative_value_text_extra] = '\0';

    return MYLITE_OK;
}

static bool literal_is_textual_numeric(enum mylite_sql_ast_literal_kind literal_kind) {
    return literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
           literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL ||
           literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT;
}
