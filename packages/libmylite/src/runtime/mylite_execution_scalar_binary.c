#include "mylite_execution_scalar_binary_internal.h"

#include "mylite_diagnostics.h"
#include "mylite_digest.h"
#include "mylite_mysql_server_identity.h"
#include "mylite_random_bytes.h"
#include "mylite_string_base64.h"
#include "mylite_string_char.h"
#include "mylite_string_compression.h"
#include "mylite_string_unhex.h"
#include "mylite_uuid.h"
#include "mylite_weight_string.h"

enum {
    base64_function_chain_initial_capacity = 4,
    base64_function_chain_growth_factor = 2,
    base64_invalid_message_capacity = 64,
    base64_signed_integer_message_capacity = 96,
    base64_unsupported_message_capacity = 256,
    digest_sha2_incorrect_parameter_warning = 1583,
    crc32_bits_per_byte = 8,
    byte_high_nibble_shift = 4,
    byte_low_nibble_mask = 0x0f,
};

enum base64_function_operation {
    base64_function_operation_to,
    base64_function_operation_from,
};

struct base64_function_chain {
    enum base64_function_operation *operations;
    size_t count;
    size_t capacity;
};

static int weight_string_binary_length_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_length
);
static int weight_string_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);
static int uuid_generate_function_value(
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
static int hex_numeric_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value,
    bool *out_handled
);
static int hex_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes
);
static int hex_literal_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes
);
static int hex_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    bool *out_handled
);
static int hex_non_weight_string_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    bool *out_handled
);
static int base64_function_chain_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum base64_function_operation expected_operation,
    struct session_scalar_cell *out_cell
);
static int base64_collect_function_chain(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum base64_function_operation expected_operation,
    struct base64_function_chain *chain,
    const struct mylite_sql_ast_node **out_argument
);
static int base64_function_chain_append(
    struct mylite_db *database,
    struct base64_function_chain *chain,
    enum base64_function_operation operation
);
static int base64_apply_function_operation(
    struct mylite_db *database,
    enum base64_function_operation operation,
    const unsigned char *bytes,
    size_t byte_count,
    struct session_scalar_cell *out_cell
);
static const char *base64_function_operation_name(enum base64_function_operation operation);
static void base64_function_chain_deinit(struct base64_function_chain *chain);
static int base64_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);
static int base64_literal_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const char *function_name,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);
static int base64_string_literal_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const char *function_name,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes
);
static int base64_unary_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);
static int base64_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    bool *out_handled
);
static int base64_copy_scalar_argument_bytes(
    struct mylite_db *database,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes,
    bool *out_is_null
);
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
static int crc32_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes
);
static int digest_function_algorithm(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_digest_algorithm *out_algorithm,
    bool *out_hash_length_is_null
);
static int sha2_hash_length_algorithm(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_digest_algorithm *out_algorithm,
    bool *out_is_null
);
static int sha2_hash_length_literal_algorithm(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    enum mylite_digest_algorithm *out_algorithm,
    bool *out_is_null
);
static int sha2_hash_length_from_magnitude(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    enum mylite_digest_algorithm *out_algorithm,
    bool *out_is_null
);
static int append_sha2_incorrect_parameters_warning(struct mylite_db *database);
static const char *digest_function_name(enum mylite_sql_ast_node_kind kind);
static uint32_t crc32_checksum(const unsigned char *bytes, size_t byte_count);

int mylite_execution_scalar_crc32_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const unsigned char *bytes = NULL;
    char *owned_bytes = NULL;
    size_t byte_count = 0U;
    uint32_t checksum = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CRC32_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_crc32_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = crc32_argument_bytes(
        database,
        mylite_execution_child_at(expression, 0U),
        out_cell,
        &bytes,
        &byte_count,
        &owned_bytes
    );
    if (rc != MYLITE_OK || bytes == NULL) {
        free(owned_bytes);
        return rc;
    }

    checksum = crc32_checksum(bytes, byte_count);
    free(owned_bytes);
    rc = mylite_execution_format_uint64(
        database,
        (uint64_t)checksum,
        out_cell->integer_text,
        sizeof(out_cell->integer_text)
    );
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->integer_text;
    }
    return rc;
}

int mylite_execution_scalar_digest_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const unsigned char *bytes = NULL;
    char *owned_bytes = NULL;
    size_t byte_count = 0U;
    struct session_scalar_cell argument_cell = {0};
    enum mylite_digest_algorithm algorithm = MYLITE_DIGEST_ALGORITHM_MD5;
    const char *function_name = NULL;
    bool input_is_null = false;
    bool hash_length_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(database, "digest function is not supported");
        return MYLITE_ERROR;
    }
    function_name = digest_function_name(expression->kind);
    if (function_name == NULL) {
        mylite_execution_set_unsupported_error(database, "digest function is not supported");
        return MYLITE_ERROR;
    }

    rc = digest_function_algorithm(database, expression, &algorithm, &hash_length_is_null);
    if (rc != MYLITE_OK || hash_length_is_null) {
        return rc;
    }

    rc = base64_argument_bytes(
        database,
        mylite_execution_child_at(expression, 0U),
        function_name,
        &argument_cell,
        &bytes,
        &byte_count,
        &owned_bytes,
        &input_is_null
    );
    if (rc != MYLITE_OK || input_is_null) {
        free(owned_bytes);
        mylite_execution_session_scalar_cell_deinit(&argument_cell);
        return rc;
    }

    rc = mylite_digest_hex_value(
        algorithm,
        bytes,
        byte_count,
        &out_cell->owned_text,
        &out_cell->value_size
    );
    free(owned_bytes);
    mylite_execution_session_scalar_cell_deinit(&argument_cell);
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(database, "failed to calculate digest value");
        return rc;
    }
    out_cell->value = out_cell->owned_text;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

int mylite_execution_scalar_hex_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *argument = NULL;
    struct session_scalar_cell argument_cell = {0};
    const unsigned char *bytes = NULL;
    char *owned_bytes = NULL;
    size_t byte_count = 0U;
    struct scalar_bitwise_value numeric_value = {.is_null = false, .integer = 0U};
    bool handled_numeric = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_HEX_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_hex_unsupported_error(database);
        return MYLITE_ERROR;
    }

    argument =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
    rc = mylite_execution_scalar_binary_evaluate_base_conversion_direct_literal_operand(
        database,
        argument,
        &numeric_value,
        &handled_numeric
    );
    if (rc == MYLITE_OK && !handled_numeric) {
        rc =
            hex_numeric_scalar_argument_value(database, argument, &numeric_value, &handled_numeric);
    }
    if (rc != MYLITE_OK || handled_numeric) {
        if (rc == MYLITE_OK && !numeric_value.is_null) {
            rc = mylite_execution_scalar_binary_format_base_conversion_value(
                database,
                numeric_value.integer,
                binary_base_conversion_hexadecimal_base,
                out_cell->base_conversion_text,
                sizeof(out_cell->base_conversion_text)
            );
            if (rc == MYLITE_OK) {
                out_cell->value = out_cell->base_conversion_text;
            }
        }
        return rc;
    }

    rc = hex_argument_bytes(database, argument, &argument_cell, &bytes, &byte_count, &owned_bytes);
    if (rc != MYLITE_OK || bytes == NULL) {
        free(owned_bytes);
        mylite_execution_session_scalar_cell_deinit(&argument_cell);
        return rc;
    }

    rc = mylite_execution_format_hex_bytes(database, bytes, byte_count, &out_cell->owned_text);
    free(owned_bytes);
    mylite_execution_session_scalar_cell_deinit(&argument_cell);
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->owned_text;
    }
    return rc;
}

int mylite_execution_scalar_weight_string_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *argument = NULL;
    struct session_scalar_cell argument_cell = {0};
    const unsigned char *bytes = NULL;
    unsigned char *result = NULL;
    char *owned_bytes = NULL;
    size_t byte_count = 0U;
    size_t result_size = 0U;
    bool is_null = false;
    int64_t binary_length = 0;
    bool has_binary_length = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || (expression->kind != MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION)) {
        mylite_execution_set_unsupported_error(
            database,
            "WEIGHT_STRING() expression is unsupported"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION &&
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_unsupported_error(
            database,
            "WEIGHT_STRING() expression is unsupported"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION &&
        mylite_sql_ast_node_child_count(expression) != 2U) {
        mylite_execution_set_unsupported_error(
            database,
            "WEIGHT_STRING() AS BINARY expression is unsupported"
        );
        return MYLITE_ERROR;
    }

    argument =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
    rc = weight_string_argument_bytes(
        database,
        argument,
        &argument_cell,
        &bytes,
        &byte_count,
        &owned_bytes,
        &is_null
    );
    if (rc == MYLITE_OK && expression->kind == MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION) {
        has_binary_length = true;
        rc = weight_string_binary_length_value(
            database,
            mylite_execution_child_at(expression, 1U),
            &binary_length
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_weight_string_value(
            database,
            bytes,
            byte_count,
            is_null,
            has_binary_length,
            binary_length,
            &result,
            &result_size,
            &is_null
        );
    }
    free(owned_bytes);
    mylite_execution_session_scalar_cell_deinit(&argument_cell);
    if (rc != MYLITE_OK || is_null) {
        free(result);
        return rc;
    }

    out_cell->owned_text = (char *)result;
    out_cell->value = out_cell->owned_text;
    out_cell->value_size = result_size;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

static int weight_string_binary_length_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_length
) {
    uint64_t length = 0U;

    if (out_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_length = 0;
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_INTEGER ||
        mylite_execution_parse_unsigned_integer_literal(&expression->span, &length) != MYLITE_OK ||
        length > (uint64_t)INT64_MAX) {
        mylite_execution_set_unsupported_error(
            database,
            "WEIGHT_STRING() AS BINARY length must fit signed 64-bit"
        );
        return MYLITE_ERROR;
    }
    *out_length = (int64_t)length;
    return MYLITE_OK;
}

static int weight_string_argument_bytes(
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
        mylite_execution_scalar_set_base64_argument_unsupported_error(database, "WEIGHT_STRING");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return base64_literal_argument_bytes(
            database,
            expression,
            "WEIGHT_STRING",
            out_bytes,
            out_byte_count,
            out_owned_bytes,
            out_is_null
        );
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return base64_unary_argument_bytes(
            database,
            expression,
            "WEIGHT_STRING",
            out_bytes,
            out_byte_count,
            out_owned_bytes,
            out_is_null
        );
    }

    rc = hex_non_weight_string_scalar_argument_value(database, expression, cell, &handled_scalar);
    if (rc != MYLITE_OK || handled_scalar) {
        if (rc == MYLITE_OK && cell->value == NULL) {
            *out_is_null = true;
        } else if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)cell->value;
            *out_byte_count = cell->has_value_size ? cell->value_size : strlen(cell->value);
        }
        return rc;
    }

    mylite_execution_scalar_set_base64_argument_unsupported_error(database, "WEIGHT_STRING");
    return MYLITE_ERROR;
}

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

int mylite_execution_scalar_to_base64_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return base64_function_chain_value(
        database,
        expression,
        base64_function_operation_to,
        out_cell
    );
}

int mylite_execution_scalar_from_base64_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return base64_function_chain_value(
        database,
        expression,
        base64_function_operation_from,
        out_cell
    );
}

int mylite_execution_scalar_compress_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const unsigned char *bytes = NULL;
    unsigned char *compressed = NULL;
    char *owned_bytes = NULL;
    size_t byte_count = 0U;
    size_t compressed_size = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_COMPRESS_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_scalar_set_base64_argument_unsupported_error(database, "COMPRESS");
        return MYLITE_ERROR;
    }

    rc = base64_argument_bytes(
        database,
        mylite_execution_child_at(expression, 0U),
        "COMPRESS",
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

    rc = mylite_string_compress(bytes, byte_count, &compressed, &compressed_size);
    free(owned_bytes);
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(database, "failed to calculate COMPRESS() value");
        return rc;
    }

    out_cell->owned_text = (char *)compressed;
    out_cell->value = out_cell->owned_text;
    out_cell->value_size = compressed_size;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

int mylite_execution_scalar_uncompress_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
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
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_UNCOMPRESS_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_scalar_set_base64_argument_unsupported_error(database, "UNCOMPRESS");
        return MYLITE_ERROR;
    }

    rc = base64_argument_bytes(
        database,
        mylite_execution_child_at(expression, 0U),
        "UNCOMPRESS",
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

    rc = mylite_string_uncompress(bytes, byte_count, &decoded, &decoded_size, &valid);
    if (rc == MYLITE_NOMEM) {
        free(owned_bytes);
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        free(owned_bytes);
        mylite_execution_set_runtime_error(database, "failed to calculate UNCOMPRESS() value");
        return rc;
    }
    if (!valid) {
        rc = mylite_string_compression_append_uncompress_warning(database, bytes, byte_count);
        free(owned_bytes);
        return rc;
    }
    free(owned_bytes);

    out_cell->owned_text = (char *)decoded;
    out_cell->value = out_cell->owned_text;
    out_cell->value_size = decoded_size;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

int mylite_execution_scalar_uncompressed_length_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const unsigned char *bytes = NULL;
    char *owned_bytes = NULL;
    size_t byte_count = 0U;
    uint32_t original_size = 0U;
    bool is_null = false;
    bool valid = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_scalar_set_base64_argument_unsupported_error(
            database,
            "UNCOMPRESSED_LENGTH"
        );
        return MYLITE_ERROR;
    }

    rc = base64_argument_bytes(
        database,
        mylite_execution_child_at(expression, 0U),
        "UNCOMPRESSED_LENGTH",
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

    rc = mylite_string_uncompressed_length(bytes, byte_count, &original_size, &valid);
    if (rc == MYLITE_NOMEM) {
        free(owned_bytes);
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        free(owned_bytes);
        mylite_execution_set_runtime_error(
            database,
            "failed to calculate UNCOMPRESSED_LENGTH() value"
        );
        return rc;
    }
    if (!valid) {
        rc = mylite_string_compression_append_uncompress_warning(database, bytes, byte_count);
        original_size = 0U;
    }
    free(owned_bytes);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_execution_format_uint64(
        database,
        (uint64_t)original_size,
        out_cell->integer_text,
        sizeof(out_cell->integer_text)
    );
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->integer_text;
    }
    return rc;
}

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

static int hex_numeric_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value,
    bool *out_handled
) {
    return mylite_execution_scalar_hex_numeric_runtime_value(
        database,
        expression,
        out_value,
        out_handled
    );
}

static int hex_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes
) {
    bool handled_scalar = false;
    int rc = MYLITE_OK;

    if (cell == NULL || out_bytes == NULL || out_byte_count == NULL || out_owned_bytes == NULL) {
        return MYLITE_MISUSE;
    }
    *out_bytes = NULL;
    *out_byte_count = 0U;
    *out_owned_bytes = NULL;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_hex_unsupported_error(database);
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return hex_literal_argument_bytes(
            database,
            expression,
            out_bytes,
            out_byte_count,
            out_owned_bytes
        );
    }

    rc = hex_scalar_argument_value(database, expression, cell, &handled_scalar);
    if (rc != MYLITE_OK || handled_scalar) {
        if (rc == MYLITE_OK && cell->value != NULL) {
            *out_bytes = (const unsigned char *)cell->value;
            *out_byte_count = cell->has_value_size ? cell->value_size : strlen(cell->value);
        }
        return rc;
    }

    mylite_execution_set_hex_unsupported_error(database);
    return MYLITE_ERROR;
}

static int hex_literal_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (literal == NULL || out_bytes == NULL || out_byte_count == NULL || out_owned_bytes == NULL) {
        return MYLITE_MISUSE;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = mylite_execution_decode_sql_string_literal_with_policy(
            database,
            literal,
            "HEX() supports only string, hex, integer, boolean, NULL, supported session "
            "scalar, and supported system variable arguments",
            "HEX() string literal is invalid",
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

    mylite_execution_set_hex_unsupported_error(database);
    return MYLITE_ERROR;
}

static int hex_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    bool *out_handled
) {
    int rc =
        hex_non_weight_string_scalar_argument_value(database, expression, out_cell, out_handled);

    if (rc != MYLITE_OK || out_handled == NULL || *out_handled) {
        return rc;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION:
    case MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION:
        *out_handled = true;
        return mylite_execution_scalar_weight_string_function_value(database, expression, out_cell);
    default:
        break;
    }
    return MYLITE_OK;
}

static int hex_non_weight_string_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    bool *out_handled
) {
    int written = 0;

    if (out_cell == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = true;
    switch (expression->kind) {
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
        if (database->session.has_selected_schema) {
            out_cell->value = database->session.selected_schema;
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_USER_FUNCTION:
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
        out_cell->value = database->session.client_user_identity;
        return MYLITE_OK;
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
        out_cell->value = database->session.current_user_identity;
        return MYLITE_OK;
    case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
        out_cell->value = "NONE";
        return MYLITE_OK;
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
        written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRIu64,
            database->session.connection_id
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            mylite_execution_set_runtime_error(database, "failed to format CONNECTION_ID() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    case MYLITE_SQL_AST_VERSION_FUNCTION:
        out_cell->value = MYLITE_MYSQL_SERVER_VERSION_STRING;
        return MYLITE_OK;
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE:
        return mylite_execution_current_timestamp_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_SYSDATE_FUNCTION:
        return mylite_execution_sysdate_scalar_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CURRENT_DATE_VALUE:
        return mylite_execution_current_date_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_CURRENT_TIME_VALUE:
        if (mylite_execution_validate_temporal_fractional_precision(
                database,
                expression,
                (struct mylite_execution_temporal_fractional_precision_context){
                    .subject_name = "curtime",
                    .unsupported_message = "fractional CURRENT_TIME precision is not supported",
                }
            ) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        return mylite_execution_current_time_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_UTC_DATE_VALUE:
        return mylite_execution_utc_date_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_UTC_TIME_VALUE:
        if (mylite_execution_validate_temporal_fractional_precision(
                database,
                expression,
                (struct mylite_execution_temporal_fractional_precision_context){
                    .subject_name = "utc_time",
                    .unsupported_message = "fractional UTC_TIME precision is not supported",
                }
            ) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        return mylite_execution_utc_time_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE:
        if (mylite_execution_validate_temporal_fractional_precision(
                database,
                expression,
                (struct mylite_execution_temporal_fractional_precision_context){
                    .subject_name = "utc_timestamp",
                    .unsupported_message = "fractional UTC_TIMESTAMP precision is not supported",
                }
            ) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        return mylite_execution_utc_timestamp_scalar_value(database, out_cell);
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
        written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRId64,
            database->session.previous_row_count
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            mylite_execution_set_runtime_error(database, "failed to format ROW_COUNT() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION:
        written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRIu64,
            database->session.found_rows
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            mylite_execution_set_runtime_error(database, "failed to format FOUND_ROWS() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
        written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRIu64,
            database->session.last_insert_id
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            mylite_execution_set_runtime_error(database, "failed to format LAST_INSERT_ID() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        return mylite_execution_system_variable_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
        return mylite_execution_cast_binary_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
        return mylite_execution_convert_binary_type_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        return mylite_execution_convert_using_binary_value(database, expression, out_cell);
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
        return mylite_execution_convert_using_charset_value(database, expression, out_cell);
    case MYLITE_SQL_AST_COLLATE_EXPRESSION:
        return mylite_execution_collate_expression_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION:
        return mylite_execution_scalar_json_extract_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_QUOTE_FUNCTION:
        return mylite_execution_scalar_json_quote_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION:
        return mylite_execution_scalar_json_unquote_function_value(database, expression, out_cell);
    default:
        break;
    }

    *out_handled = false;
    return MYLITE_OK;
}

static int base64_function_chain_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum base64_function_operation expected_operation,
    struct session_scalar_cell *out_cell
) {
    struct base64_function_chain chain = {0};
    struct session_scalar_cell argument_cell = {0};
    struct session_scalar_cell current_cell = {0};
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

    rc = base64_collect_function_chain(database, expression, expected_operation, &chain, &argument);
    if (rc == MYLITE_OK) {
        rc = base64_argument_bytes(
            database,
            argument,
            base64_function_operation_name(chain.operations[chain.count - 1U]),
            &argument_cell,
            &bytes,
            &byte_count,
            &owned_bytes,
            &is_null
        );
        mylite_execution_session_scalar_cell_deinit(&argument_cell);
    }

    for (size_t index = chain.count; rc == MYLITE_OK && !is_null && index > 0U; --index) {
        struct session_scalar_cell next_cell = {0};

        rc = base64_apply_function_operation(
            database,
            chain.operations[index - 1U],
            bytes,
            byte_count,
            &next_cell
        );
        free(owned_bytes);
        owned_bytes = NULL;
        if (rc != MYLITE_OK) {
            mylite_execution_session_scalar_cell_deinit(&next_cell);
            break;
        }

        mylite_execution_session_scalar_cell_deinit(&current_cell);
        current_cell = next_cell;
        if (current_cell.value == NULL) {
            is_null = true;
            bytes = NULL;
            byte_count = 0U;
        } else {
            bytes = (const unsigned char *)current_cell.value;
            byte_count =
                current_cell.has_value_size ? current_cell.value_size : strlen(current_cell.value);
        }
    }

    if (rc == MYLITE_OK && !is_null) {
        *out_cell = current_cell;
        current_cell = (struct session_scalar_cell){0};
    }

    free(owned_bytes);
    mylite_execution_session_scalar_cell_deinit(&current_cell);
    base64_function_chain_deinit(&chain);
    return rc;
}

static int base64_collect_function_chain(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum base64_function_operation expected_operation,
    struct base64_function_chain *chain,
    const struct mylite_sql_ast_node **out_argument
) {
    const struct mylite_sql_ast_node *current =
        mylite_execution_unwrap_parenthesized_expression(expression);
    enum base64_function_operation operation = base64_function_operation_to;
    bool first = true;
    int rc = MYLITE_OK;

    if (chain == NULL || out_argument == NULL) {
        return MYLITE_MISUSE;
    }
    *out_argument = NULL;

    while (current != NULL) {
        switch (current->kind) {
        case MYLITE_SQL_AST_TO_BASE64_FUNCTION:
            operation = base64_function_operation_to;
            break;
        case MYLITE_SQL_AST_FROM_BASE64_FUNCTION:
            operation = base64_function_operation_from;
            break;
        default:
            if (first) {
                mylite_execution_set_native_function_parameter_count_error(
                    database,
                    base64_function_operation_name(expected_operation)
                );
                return MYLITE_ERROR;
            }
            *out_argument = current;
            return MYLITE_OK;
        }

        if (first && operation != expected_operation) {
            break;
        }
        first = false;

        if (mylite_sql_ast_node_child_count(current) != 1U) {
            mylite_execution_set_native_function_parameter_count_error(
                database,
                base64_function_operation_name(operation)
            );
            return MYLITE_ERROR;
        }
        rc = base64_function_chain_append(database, chain, operation);
        if (rc != MYLITE_OK) {
            return rc;
        }

        current =
            mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(current, 0U)
            );
    }

    mylite_execution_set_native_function_parameter_count_error(
        database,
        base64_function_operation_name(expected_operation)
    );
    return MYLITE_ERROR;
}

static int base64_function_chain_append(
    struct mylite_db *database,
    struct base64_function_chain *chain,
    enum base64_function_operation operation
) {
    enum base64_function_operation *operations = NULL;
    size_t new_capacity = 0U;

    if (chain == NULL) {
        return MYLITE_MISUSE;
    }
    if (chain->count < chain->capacity) {
        chain->operations[chain->count++] = operation;
        return MYLITE_OK;
    }

    if (chain->capacity == 0U) {
        new_capacity = base64_function_chain_initial_capacity;
    } else if (chain->capacity > SIZE_MAX / base64_function_chain_growth_factor) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    } else {
        new_capacity = chain->capacity * base64_function_chain_growth_factor;
    }
    if (new_capacity > SIZE_MAX / sizeof(*chain->operations)) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    operations = (enum base64_function_operation *)
        realloc(chain->operations, new_capacity * sizeof(*chain->operations));
    if (operations == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    chain->operations = operations;
    chain->capacity = new_capacity;
    chain->operations[chain->count++] = operation;
    return MYLITE_OK;
}

static int base64_apply_function_operation(
    struct mylite_db *database,
    enum base64_function_operation operation,
    const unsigned char *bytes,
    size_t byte_count,
    struct session_scalar_cell *out_cell
) {
    unsigned char *decoded = NULL;
    char *encoded = NULL;
    size_t result_size = 0U;
    bool valid = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL || (bytes == NULL && byte_count != 0U)) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    if (operation == base64_function_operation_to) {
        rc = mylite_string_base64_encode(bytes, byte_count, &encoded, &result_size);
        if (rc != MYLITE_OK) {
            if (rc == MYLITE_NOMEM) {
                mylite_execution_set_nomem_error(database);
            }
            return rc;
        }
        out_cell->owned_text = encoded;
    } else {
        rc = mylite_string_base64_decode(bytes, byte_count, &decoded, &result_size, &valid);
        if (rc != MYLITE_OK) {
            if (rc == MYLITE_NOMEM) {
                mylite_execution_set_nomem_error(database);
            }
            return rc;
        }
        if (!valid) {
            free(decoded);
            return MYLITE_OK;
        }
        out_cell->owned_text = (char *)decoded;
    }

    out_cell->value = out_cell->owned_text;
    out_cell->value_size = result_size;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

static const char *base64_function_operation_name(enum base64_function_operation operation) {
    return operation == base64_function_operation_to ? "TO_BASE64" : "FROM_BASE64";
}

static void base64_function_chain_deinit(struct base64_function_chain *chain) {
    if (chain == NULL) {
        return;
    }

    free(chain->operations);
    *chain = (struct base64_function_chain){0};
}

static int base64_argument_bytes(
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
        return base64_literal_argument_bytes(
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
        return base64_unary_argument_bytes(
            database,
            expression,
            function_name,
            out_bytes,
            out_byte_count,
            out_owned_bytes,
            out_is_null
        );
    }

    rc = base64_scalar_argument_value(database, expression, cell, &handled_scalar);
    if (rc != MYLITE_OK || handled_scalar) {
        if (rc == MYLITE_OK) {
            rc = base64_copy_scalar_argument_bytes(
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

static int base64_literal_argument_bytes(
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
        return base64_string_literal_argument_bytes(
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

static int base64_string_literal_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const char *function_name,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes
) {
    char unsupported_message[base64_unsupported_message_capacity];
    char invalid_message[base64_invalid_message_capacity];
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
        mylite_execution_set_runtime_error(database, "failed to format Base64 unsupported message");
        return MYLITE_ERROR;
    }
    written = snprintf(
        invalid_message,
        sizeof(invalid_message),
        "%s() string literal is invalid",
        function_name
    );
    if (written < 0 || (size_t)written >= sizeof(invalid_message)) {
        mylite_execution_set_runtime_error(database, "failed to format Base64 invalid message");
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

static int base64_unary_argument_bytes(
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
    char message[base64_signed_integer_message_capacity];
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
            "failed to format Base64 signed-integer message"
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

static int base64_scalar_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell,
    bool *out_handled
) {
    int rc = hex_scalar_argument_value(database, expression, out_cell, out_handled);

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

static int base64_copy_scalar_argument_bytes(
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

    rc = base64_scalar_argument_value(database, expression, cell, &handled_scalar);
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

void mylite_execution_scalar_set_base64_argument_unsupported_error(
    struct mylite_db *database,
    const char *function_name
) {
    char message[base64_unsupported_message_capacity];
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
    return hex_scalar_argument_value(database, expression, out_cell, out_handled);
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
    return hex_scalar_argument_value(database, expression, out_cell, out_handled);
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

int mylite_execution_format_hex_bytes(
    struct mylite_db *database,
    const unsigned char *bytes,
    size_t byte_count,
    char **out_text
) {
    static const char digits[] = "0123456789ABCDEF";
    char *text = NULL;

    if ((bytes == NULL && byte_count != 0U) || out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (byte_count > (SIZE_MAX - 1U) / 2U) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    text = (char *)malloc((byte_count * 2U) + 1U);
    if (text == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < byte_count; ++index) {
        unsigned char byte = bytes[index];

        text[index * 2U] = digits[byte >> byte_high_nibble_shift];
        text[(index * 2U) + 1U] = digits[byte & byte_low_nibble_mask];
    }
    text[byte_count * 2U] = '\0';
    *out_text = text;
    return MYLITE_OK;
}

static int crc32_argument_bytes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cell,
    const unsigned char **out_bytes,
    size_t *out_byte_count,
    char **out_owned_bytes
) {
    const struct mylite_sql_ast_node *literal = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;

    if (cell == NULL || out_bytes == NULL || out_byte_count == NULL || out_owned_bytes == NULL) {
        return MYLITE_MISUSE;
    }
    *out_bytes = NULL;
    *out_byte_count = 0U;
    *out_owned_bytes = NULL;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    literal = expression;
    if (expression == NULL) {
        mylite_execution_set_crc32_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            mylite_execution_set_crc32_unsupported_error(database);
            return MYLITE_ERROR;
        }
        literal = mylite_execution_unwrap_parenthesized_expression(
            mylite_execution_child_at(expression, 0U)
        );
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        mylite_execution_set_crc32_unsupported_error(database);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if ((is_negative || expression != literal) && literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        mylite_execution_set_crc32_unsupported_error(database);
        return MYLITE_ERROR;
    }
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_NULL:
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_TRUE:
        *out_bytes = (const unsigned char *)"1";
        *out_byte_count = 1U;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_FALSE:
        *out_bytes = (const unsigned char *)"0";
        *out_byte_count = 1U;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_INTEGER: {
        int rc = mylite_execution_normalize_decimal_integer_literal(
            database,
            &literal->span,
            is_negative,
            cell->literal_text,
            sizeof(cell->literal_text)
        );

        if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)cell->literal_text;
            *out_byte_count = strlen(cell->literal_text);
        }
        return rc;
    }
    case MYLITE_SQL_AST_LITERAL_STRING: {
        int rc = mylite_execution_decode_sql_string_literal(
            database,
            literal,
            "CRC32() supports only string, hex, integer, boolean, and NULL literals",
            "CRC32() string literals do not support embedded NUL bytes",
            out_owned_bytes,
            out_byte_count
        );

        if (rc == MYLITE_OK) {
            *out_bytes = (const unsigned char *)*out_owned_bytes;
        }
        return rc;
    }
    case MYLITE_SQL_AST_LITERAL_HEX: {
        int rc = mylite_execution_decode_binary_hex_literal(
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
    default:
        break;
    }

    mylite_execution_set_crc32_unsupported_error(database);
    return MYLITE_ERROR;
}

static int digest_function_algorithm(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_digest_algorithm *out_algorithm,
    bool *out_hash_length_is_null
) {
    size_t child_count = 0U;

    if (expression == NULL || out_algorithm == NULL || out_hash_length_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_algorithm = MYLITE_DIGEST_ALGORITHM_MD5;
    *out_hash_length_is_null = false;
    child_count = mylite_sql_ast_node_child_count(expression);

    switch (expression->kind) {
    case MYLITE_SQL_AST_MD5_FUNCTION:
        if (child_count == 1U) {
            *out_algorithm = MYLITE_DIGEST_ALGORITHM_MD5;
            return MYLITE_OK;
        }
        break;
    case MYLITE_SQL_AST_SHA_FUNCTION:
    case MYLITE_SQL_AST_SHA1_FUNCTION:
        if (child_count == 1U) {
            *out_algorithm = MYLITE_DIGEST_ALGORITHM_SHA1;
            return MYLITE_OK;
        }
        break;
    case MYLITE_SQL_AST_SHA2_FUNCTION:
        if (child_count == 2U) {
            return sha2_hash_length_algorithm(
                database,
                mylite_execution_child_at(expression, 1U),
                out_algorithm,
                out_hash_length_is_null
            );
        }
        break;
    default:
        break;
    }

    mylite_execution_set_unsupported_error(
        database,
        "digest functions support only MD5(arg), SHA(arg), SHA1(arg), and SHA2(arg, length)"
    );
    return MYLITE_ERROR;
}

static int sha2_hash_length_algorithm(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_digest_algorithm *out_algorithm,
    bool *out_is_null
) {
    const struct mylite_sql_ast_node *literal = NULL;
    bool is_negative = false;

    if (out_algorithm == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_algorithm = MYLITE_DIGEST_ALGORITHM_SHA256;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    literal = expression;
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "SHA2() supports only integer, boolean, and NULL hash-length arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            mylite_execution_set_unsupported_error(
                database,
                "SHA2() supports only signed integer hash-length arguments"
            );
            return MYLITE_ERROR;
        }
        literal = mylite_execution_unwrap_parenthesized_expression(
            mylite_execution_child_at(expression, 0U)
        );
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        mylite_execution_set_unsupported_error(
            database,
            "SHA2() supports only integer, boolean, and NULL hash-length arguments"
        );
        return MYLITE_ERROR;
    }
    return sha2_hash_length_literal_algorithm(
        database,
        literal,
        is_negative,
        out_algorithm,
        out_is_null
    );
}

static int sha2_hash_length_literal_algorithm(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    enum mylite_digest_algorithm *out_algorithm,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t magnitude = 0U;

    if (literal == NULL || out_algorithm == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        return sha2_hash_length_from_magnitude(database, 1U, false, out_algorithm, out_is_null);
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        return sha2_hash_length_from_magnitude(database, 0U, false, out_algorithm, out_is_null);
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        mylite_execution_set_unsupported_error(
            database,
            "SHA2() supports only integer, boolean, and NULL hash-length arguments"
        );
        return MYLITE_ERROR;
    }
    if (mylite_execution_parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        return sha2_hash_length_from_magnitude(
            database,
            UINT64_MAX,
            is_negative,
            out_algorithm,
            out_is_null
        );
    }
    return sha2_hash_length_from_magnitude(
        database,
        magnitude,
        is_negative,
        out_algorithm,
        out_is_null
    );
}

static int sha2_hash_length_from_magnitude(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    enum mylite_digest_algorithm *out_algorithm,
    bool *out_is_null
) {
    if (out_algorithm == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    if (!is_negative) {
        switch (magnitude) {
        case MYLITE_DIGEST_SHA2_LENGTH_DEFAULT:
        case MYLITE_DIGEST_SHA2_LENGTH_SHA256:
            *out_algorithm = MYLITE_DIGEST_ALGORITHM_SHA256;
            *out_is_null = false;
            return MYLITE_OK;
        case MYLITE_DIGEST_SHA2_LENGTH_SHA224:
            *out_algorithm = MYLITE_DIGEST_ALGORITHM_SHA224;
            *out_is_null = false;
            return MYLITE_OK;
        case MYLITE_DIGEST_SHA2_LENGTH_SHA384:
            *out_algorithm = MYLITE_DIGEST_ALGORITHM_SHA384;
            *out_is_null = false;
            return MYLITE_OK;
        case MYLITE_DIGEST_SHA2_LENGTH_SHA512:
            *out_algorithm = MYLITE_DIGEST_ALGORITHM_SHA512;
            *out_is_null = false;
            return MYLITE_OK;
        default:
            break;
        }
    }

    *out_is_null = true;
    return append_sha2_incorrect_parameters_warning(database);
}

static int append_sha2_incorrect_parameters_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        digest_sha2_incorrect_parameter_warning,
        "HY000",
        "Incorrect parameters in the call to native function 'sha2'"
    );
}

static const char *digest_function_name(enum mylite_sql_ast_node_kind kind) {
    switch (kind) {
    case MYLITE_SQL_AST_MD5_FUNCTION:
    case MYLITE_SQL_AST_MD5_ARGUMENT_COUNT_ERROR:
        return "MD5";
    case MYLITE_SQL_AST_SHA_FUNCTION:
    case MYLITE_SQL_AST_SHA_ARGUMENT_COUNT_ERROR:
        return "SHA";
    case MYLITE_SQL_AST_SHA1_FUNCTION:
    case MYLITE_SQL_AST_SHA1_ARGUMENT_COUNT_ERROR:
        return "SHA1";
    case MYLITE_SQL_AST_SHA2_FUNCTION:
    case MYLITE_SQL_AST_SHA2_ARGUMENT_COUNT_ERROR:
        return "SHA2";
    default:
        break;
    }
    return NULL;
}

static uint32_t crc32_checksum(const unsigned char *bytes, size_t byte_count) {
    uint32_t crc = UINT32_C(0xffffffff);

    for (size_t index = 0U; index < byte_count; ++index) {
        crc ^= (uint32_t)bytes[index];
        for (unsigned int bit = 0U; bit < crc32_bits_per_byte; ++bit) {
            if ((crc & UINT32_C(1)) != 0U) {
                crc = (crc >> 1U) ^ UINT32_C(0xedb88320);
            } else {
                crc >>= 1U;
            }
        }
    }
    return crc ^ UINT32_C(0xffffffff);
}
