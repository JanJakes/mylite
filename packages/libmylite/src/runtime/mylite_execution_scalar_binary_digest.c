#include "mylite_execution_scalar_binary_internal.h"

#include "mylite_diagnostics.h"
#include "mylite_digest.h"

enum {
    digest_sha2_incorrect_parameter_warning = 1583,
    crc32_bits_per_byte = 8,
};

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

    rc = mylite_execution_scalar_binary_argument_bytes(
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
