#include "mylite_execution_scalar_binary_internal.h"

enum {
    binary_argument_invalid_message_capacity = 64,
    binary_argument_signed_integer_message_capacity = 96,
    binary_argument_unsupported_message_capacity = 256,
};

static int binary_argument_literal_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const char *function_name,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);
static int binary_argument_string_literal_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const char *function_name,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes
);
static int binary_argument_unary_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);
static int binary_argument_copy_scalar_bytes(
    struct mylite_db *database,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);

int mylite_execution_scalar_binary_format_base_conversion_value(
    struct mylite_db *database,
    uint64_t value,
    unsigned int base,
    char *buffer,
    size_t buffer_size
) {
    static const char base_conversion_digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char reversed[binary_base_conversion_text_capacity];
    size_t digit_count = 0U;

    if (buffer == NULL || buffer_size == 0U || base < binary_base_conversion_binary_base ||
        base > binary_base_conversion_max_base) {
        mylite_execution_set_runtime_error(database, "failed to format base conversion value");
        return MYLITE_ERROR;
    }
    if (value == 0U) {
        if (buffer_size < 2U) {
            mylite_execution_set_runtime_error(database, "failed to format base conversion value");
            return MYLITE_ERROR;
        }
        buffer[0] = '0';
        buffer[1] = '\0';
        return MYLITE_OK;
    }

    while (value != 0U) {
        unsigned int digit = (unsigned int)(value % base);

        if (digit_count == sizeof(reversed)) {
            mylite_execution_set_runtime_error(database, "failed to format base conversion value");
            return MYLITE_ERROR;
        }
        reversed[digit_count] = base_conversion_digits[digit];
        ++digit_count;
        value /= base;
    }
    if (digit_count >= buffer_size) {
        mylite_execution_set_runtime_error(database, "failed to format base conversion value");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; index < digit_count; ++index) {
        buffer[index] = reversed[digit_count - index - 1U];
    }
    buffer[digit_count] = '\0';
    return MYLITE_OK;
}

int mylite_execution_scalar_binary_evaluate_base_conversion_direct_literal_operand(
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
        mylite_execution_set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (!is_negative) {
        out_value->integer = magnitude;
        return MYLITE_OK;
    }
    if (magnitude > int64_min_magnitude) {
        mylite_execution_set_base_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (magnitude == int64_min_magnitude) {
        out_value->integer = (uint64_t)INT64_MIN;
    } else if (magnitude != 0U) {
        out_value->integer = (uint64_t)(-(int64_t)magnitude);
    }
    return MYLITE_OK;
}

int mylite_execution_scalar_binary_argument_bytes(
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

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_scalar_set_base64_argument_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return binary_argument_literal_bytes(
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
        return binary_argument_unary_bytes(
            database,
            expression,
            function_name,
            out_bytes,
            out_byte_count,
            out_owned_bytes,
            out_is_null
        );
    }

    rc = mylite_execution_scalar_binary_argument_scalar_value(
        database,
        expression,
        cell,
        &handled_scalar
    );
    if (rc != MYLITE_OK || handled_scalar) {
        if (rc == MYLITE_OK) {
            rc = binary_argument_copy_scalar_bytes(
                database,
                cell,
                out_bytes,
                out_byte_count,
                out_owned_bytes,
                out_is_null
            );
        }
        return rc;
    }

    mylite_execution_scalar_set_base64_argument_unsupported_error(database, function_name);
    return MYLITE_ERROR;
}

static int binary_argument_literal_bytes(
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
        rc = mylite_execution_copy_source_span_text(database, &literal->span, out_owned_bytes);
        if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)*out_owned_bytes;
            *out_byte_count = strlen(*out_owned_bytes);
        }
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        return binary_argument_string_literal_bytes(
            database,
            literal,
            function_name,
            out_bytes,
            out_byte_count,
            out_owned_bytes
        );
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

    mylite_execution_scalar_set_base64_argument_unsupported_error(database, function_name);
    return MYLITE_ERROR;
}

static int binary_argument_string_literal_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const char *function_name,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes
) {
    char unsupported_message[binary_argument_unsupported_message_capacity];
    char invalid_message[binary_argument_invalid_message_capacity];
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
        mylite_execution_set_runtime_error(
            database,
            "failed to format binary scalar unsupported message"
        );
        return MYLITE_ERROR;
    }
    written = snprintf(
        invalid_message,
        sizeof(invalid_message),
        "%s() string literal is invalid",
        function_name
    );
    if (written < 0 || (size_t)written >= sizeof(invalid_message)) {
        mylite_execution_set_runtime_error(
            database,
            "failed to format binary scalar invalid message"
        );
        return MYLITE_ERROR;
    }
    rc = mylite_execution_decode_sql_string_literal_with_policy(
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

static int binary_argument_unary_bytes(
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
    char message[binary_argument_signed_integer_message_capacity];
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
        mylite_execution_set_runtime_error(
            database,
            "failed to format binary scalar signed-integer message"
        );
        return MYLITE_ERROR;
    }

    operator_kind = mylite_sql_ast_node_operator(expression);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        mylite_execution_set_unsupported_error(database, message);
        return MYLITE_ERROR;
    }

    literal =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        mylite_execution_set_unsupported_error(database, message);
        return MYLITE_ERROR;
    }

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
        rc = mylite_execution_copy_source_span_text(database, &literal->span, out_owned_bytes);
    } else {
        if (literal->span.length > SIZE_MAX - 2U) {
            mylite_execution_set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        *out_owned_bytes = (char *)malloc(literal->span.length + 2U);
        if (*out_owned_bytes == NULL) {
            mylite_execution_set_nomem_error(database);
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

int mylite_execution_scalar_binary_argument_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    bool *out_handled
) {
    int rc = mylite_execution_scalar_binary_scalar_argument_value(
        database,
        expression,
        out_cell,
        out_handled
    );

    if (rc != MYLITE_OK || out_handled == NULL || *out_handled) {
        return rc;
    }

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return MYLITE_OK;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_COMPRESS_FUNCTION:
        *out_handled = true;
        return mylite_execution_scalar_compress_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_UNCOMPRESS_FUNCTION:
        *out_handled = true;
        return mylite_execution_scalar_uncompress_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_FUNCTION:
        *out_handled = true;
        return mylite_execution_scalar_uncompressed_length_function_value(
            database,
            expression,
            out_cell
        );
    case MYLITE_SQL_AST_RANDOM_BYTES_FUNCTION:
        *out_handled = true;
        return mylite_execution_scalar_random_bytes_function_value(database, expression, out_cell);
    default:
        break;
    }
    return MYLITE_OK;
}

static int binary_argument_copy_scalar_bytes(
    struct mylite_db *database,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
) {
    size_t byte_count = 0U;
    char *bytes = NULL;

    if (cell == NULL || out_bytes == NULL || out_byte_count == NULL || out_owned_bytes == NULL ||
        out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    if (cell->value == NULL) {
        mylite_execution_session_scalar_cell_deinit(cell);
        *out_is_null = true;
        return MYLITE_OK;
    }

    byte_count = cell->has_value_size ? cell->value_size : strlen(cell->value);
    if (byte_count == SIZE_MAX) {
        mylite_execution_session_scalar_cell_deinit(cell);
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    bytes = (char *)malloc(byte_count + 1U);
    if (bytes == NULL) {
        mylite_execution_session_scalar_cell_deinit(cell);
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(bytes, cell->value, byte_count);
    bytes[byte_count] = '\0';
    mylite_execution_session_scalar_cell_deinit(cell);

    *out_owned_bytes = bytes;
    *out_bytes = (const unsigned char *)bytes;
    *out_byte_count = byte_count;
    return MYLITE_OK;
}

void mylite_execution_scalar_set_base64_argument_unsupported_error(
    struct mylite_db *database,
    const char *function_name
) {
    char message[binary_argument_unsupported_message_capacity];
    int written = 0;

    if (function_name == NULL) {
        mylite_execution_set_runtime_error(database, "missing Base64 function name");
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
        mylite_execution_set_runtime_error(database, "failed to format Base64 unsupported message");
        return;
    }
    mylite_execution_set_unsupported_error(database, message);
}
