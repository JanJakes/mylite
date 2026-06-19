#include "mylite_execution_scalar_binary_internal.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_mysql_error_codes.h"
#include "mylite_string_char.h"

enum {
    char_invalid_utf8_warning_hex_capacity = 64,
    char_hex_bits_per_nibble = 4,
    char_hex_low_nibble_mask = 0x0f,
};

static int char_function_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value
);
static int char_function_charset_info(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_convert_charset_info *out_info
);
static int char_function_copy_charset_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char *charset_name,
    size_t charset_name_size
);
static int char_function_apply_charset(
    struct mylite_db *database,
    const struct scalar_convert_charset_info *info,
    char *bytes,
    size_t *inout_byte_count,
    bool *out_is_null
);
static int char_function_apply_utf8_charset(
    struct mylite_db *database,
    const struct scalar_convert_charset_info *info,
    char *bytes,
    size_t *inout_byte_count,
    bool *out_is_null
);
static int char_function_valid_utf8_prefix_length(
    const char *bytes,
    size_t byte_count,
    size_t *out_prefix_length
);
static int char_function_append_invalid_utf8_warning(
    struct mylite_db *database,
    const struct scalar_convert_charset_info *info,
    const char *bytes,
    size_t byte_count
);
static bool char_function_session_sql_mode_is_strict(const struct mylite_db *database);
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

int mylite_execution_scalar_char_charset_info_for_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_convert_charset_info *out_info
) {
    return char_function_charset_info(database, expression, out_info);
}

int mylite_execution_scalar_char_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    struct scalar_convert_charset_info charset_info = {0};
    struct mylite_string_char_buffer buffer = {0};
    char *bytes = NULL;
    size_t byte_count = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CHAR_FUNCTION ||
        (mylite_sql_ast_node_child_count(expression) != 1U &&
         mylite_sql_ast_node_child_count(expression) != 2U)) {
        mylite_execution_set_unsupported_error(
            database,
            "CHAR() supports one or more integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }
    rc = char_function_charset_info(database, expression, &charset_info);
    if (rc != MYLITE_OK) {
        return rc;
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
    if (rc == MYLITE_OK) {
        rc = char_function_apply_charset(database, &charset_info, bytes, &byte_count, &is_null);
    }
    mylite_string_char_buffer_deinit(&buffer);
    if (rc == MYLITE_NOMEM) {
        free(bytes);
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        free(bytes);
        return rc;
    }
    if (is_null) {
        free(bytes);
        return MYLITE_OK;
    }

    out_cell->owned_text = bytes;
    out_cell->value = out_cell->owned_text;
    out_cell->value_size = byte_count;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

static int char_function_charset_info(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_convert_charset_info *out_info
) {
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    if (out_info == NULL) {
        return MYLITE_MISUSE;
    }
    *out_info = (struct scalar_convert_charset_info){
        .charset = "binary",
        .collation = "binary",
        .ascii_only_value = false,
        .warning = SCALAR_CONVERT_CHARSET_WARNING_NONE,
    };

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CHAR_FUNCTION) {
        mylite_execution_set_unsupported_error(
            database,
            "CHAR() supports one or more integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count == 1U) {
        return MYLITE_OK;
    }
    if (child_count != 2U) {
        mylite_execution_set_unsupported_error(
            database,
            "CHAR() supports one optional USING character set clause"
        );
        return MYLITE_ERROR;
    }

    rc = char_function_copy_charset_name(
        database,
        mylite_execution_child_at(expression, 1U),
        charset_name,
        sizeof(charset_name)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_execution_text_equals_ascii_case_insensitive(charset_name, "binary")) {
        return MYLITE_OK;
    }
    return mylite_execution_scalar_convert_charset_info_by_name(database, charset_name, out_info);
}

static int char_function_copy_charset_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char *charset_name,
    size_t charset_name_size
) {
    return mylite_execution_copy_identifier_name_text(
        database,
        expression,
        charset_name,
        charset_name_size,
        "character set",
        "CHAR() USING character set names do not support NUL bytes"
    );
}

static int char_function_apply_charset(
    struct mylite_db *database,
    const struct scalar_convert_charset_info *info,
    char *bytes,
    size_t *inout_byte_count,
    bool *out_is_null
) {
    if (info == NULL || bytes == NULL || inout_byte_count == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_is_null = false;

    if (mylite_execution_text_equals_ascii_case_insensitive(info->charset, "utf8mb4") ||
        mylite_execution_text_equals_ascii_case_insensitive(info->charset, "utf8mb3")) {
        return char_function_apply_utf8_charset(
            database,
            info,
            bytes,
            inout_byte_count,
            out_is_null
        );
    }
    return MYLITE_OK;
}

static int char_function_apply_utf8_charset(
    struct mylite_db *database,
    const struct scalar_convert_charset_info *info,
    char *bytes,
    size_t *inout_byte_count,
    bool *out_is_null
) {
    size_t prefix_length = 0U;
    int rc = char_function_valid_utf8_prefix_length(bytes, *inout_byte_count, &prefix_length);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (prefix_length == *inout_byte_count) {
        return MYLITE_OK;
    }

    rc = char_function_append_invalid_utf8_warning(
        database,
        info,
        bytes + prefix_length,
        *inout_byte_count - prefix_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (char_function_session_sql_mode_is_strict(database)) {
        *out_is_null = true;
        *inout_byte_count = 0U;
        return MYLITE_OK;
    }

    *inout_byte_count = prefix_length;
    bytes[prefix_length] = '\0';
    return MYLITE_OK;
}

static int char_function_valid_utf8_prefix_length(
    const char *bytes,
    size_t byte_count,
    size_t *out_prefix_length
) {
    size_t index = 0U;

    if (bytes == NULL || out_prefix_length == NULL) {
        return MYLITE_MISUSE;
    }
    while (index < byte_count) {
        size_t width = 0U;

        if (mylite_execution_utf8_sequence_width(bytes, byte_count, index, &width) != MYLITE_OK) {
            break;
        }
        index += width;
    }
    *out_prefix_length = index;
    return MYLITE_OK;
}

static int char_function_append_invalid_utf8_warning(
    struct mylite_db *database,
    const struct scalar_convert_charset_info *info,
    const char *bytes,
    size_t byte_count
) {
    static const char hex_digits[] = "0123456789ABCDEF";
    char hex[char_invalid_utf8_warning_hex_capacity + 1U];
    char message
        [sizeof("Invalid  character string: ''") + MYLITE_CATALOG_IDENTIFIER_CAPACITY +
         char_invalid_utf8_warning_hex_capacity];
    size_t hex_byte_count = byte_count;

    if (database == NULL || info == NULL || bytes == NULL) {
        return MYLITE_MISUSE;
    }
    if (hex_byte_count > char_invalid_utf8_warning_hex_capacity / 2U) {
        hex_byte_count = char_invalid_utf8_warning_hex_capacity / 2U;
    }
    for (size_t index = 0U; index < hex_byte_count; ++index) {
        unsigned char byte = (unsigned char)bytes[index];

        hex[index * 2U] = hex_digits[byte >> char_hex_bits_per_nibble];
        hex[(index * 2U) + 1U] = hex_digits[byte & char_hex_low_nibble_mask];
    }
    hex[hex_byte_count * 2U] = '\0';

    snprintf(message, sizeof(message), "Invalid %s character string: '%s'", info->charset, hex);
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_invalid_character_string,
        "HY000",
        message
    );
}

static bool char_function_session_sql_mode_is_strict(const struct mylite_db *database) {
    if (database == NULL) {
        return false;
    }
    return (database->session.sql_mode & (MYLITE_SESSION_SQL_MODE_STRICT_TRANS_TABLES |
                                          MYLITE_SESSION_SQL_MODE_STRICT_ALL_TABLES)) != 0U;
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
