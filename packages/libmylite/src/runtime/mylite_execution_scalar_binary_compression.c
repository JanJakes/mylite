#include "mylite_execution_scalar_binary_internal.h"

#include "mylite_string_compression.h"

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

    rc = mylite_execution_scalar_binary_argument_bytes(
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

    rc = mylite_execution_scalar_binary_argument_bytes(
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

    rc = mylite_execution_scalar_binary_argument_bytes(
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
