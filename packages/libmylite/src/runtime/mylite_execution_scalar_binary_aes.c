#include "mylite_execution_scalar_binary_internal.h"

#include "mylite_aes.h"
#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_execution_ast_internal.h"

enum {
    aes_long_key_warning = 3237,
    aes_legacy_key_size = 16,
};

static const char aes_long_key_warning_message[] =
    "AES key size should be 16 bytes length or secure KDF methods hkdf or pbkdf2_hmac should "
    "be used, please provide exact AES key size or use KDF methods for better security.";

static const struct mylite_sql_ast_node *aes_function_arguments(
    const struct mylite_sql_ast_node *expression
);
static const char *aes_function_name(const struct mylite_sql_ast_node *expression);
static bool aes_function_name_matches(
    const struct mylite_sql_ast_node *expression,
    const char *name
);
static bool source_span_equals_ascii_case_insensitive(
    const struct mylite_sql_source_span *span,
    const char *text
);
static int aes_append_long_key_warning(struct mylite_db *database);

bool mylite_execution_scalar_aes_function_match(const struct mylite_sql_ast_node *expression) {
    return aes_function_name(expression) != NULL;
}

int mylite_execution_scalar_aes_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const char *function_name = aes_function_name(expression);
    const struct mylite_sql_ast_node *arguments = aes_function_arguments(expression);
    struct session_scalar_cell input_cell = {0};
    struct session_scalar_cell key_cell = {0};
    const unsigned char *input = NULL;
    const unsigned char *key = NULL;
    char *owned_input = NULL;
    char *owned_key = NULL;
    size_t input_size = 0U;
    size_t key_size = 0U;
    bool input_is_null = false;
    bool key_is_null = false;
    bool long_key_warning = false;
    bool invalid = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (function_name == NULL || arguments == NULL ||
        mylite_sql_ast_node_child_count(arguments) != 2U) {
        mylite_execution_set_native_function_parameter_count_error(
            database,
            function_name == NULL ? "AES_ENCRYPT" : function_name
        );
        return MYLITE_ERROR;
    }

    rc = mylite_execution_scalar_binary_argument_bytes(
        database,
        child_at(arguments, 0U),
        function_name,
        &input_cell,
        &input,
        &input_size,
        &owned_input,
        &input_is_null
    );
    if (rc == MYLITE_OK) {
        rc = mylite_execution_scalar_binary_argument_bytes(
            database,
            child_at(arguments, 1U),
            function_name,
            &key_cell,
            &key,
            &key_size,
            &owned_key,
            &key_is_null
        );
    }
    if (rc != MYLITE_OK || input_is_null || key_is_null) {
        if (rc == MYLITE_OK && input_is_null && !key_is_null && key_size > aes_legacy_key_size) {
            rc = aes_append_long_key_warning(database);
        }
        free(owned_input);
        free(owned_key);
        mylite_execution_session_scalar_cell_deinit(&input_cell);
        mylite_execution_session_scalar_cell_deinit(&key_cell);
        return rc;
    }

    if (strcmp(function_name, "AES_ENCRYPT") == 0) {
        rc = mylite_aes_encrypt_default(
            input,
            input_size,
            key,
            key_size,
            (unsigned char **)&out_cell->owned_text,
            &out_cell->value_size,
            &long_key_warning
        );
    } else {
        rc = mylite_aes_decrypt_default(
            input,
            input_size,
            key,
            key_size,
            (unsigned char **)&out_cell->owned_text,
            &out_cell->value_size,
            &invalid,
            &long_key_warning
        );
    }
    free(owned_input);
    free(owned_key);
    mylite_execution_session_scalar_cell_deinit(&input_cell);
    mylite_execution_session_scalar_cell_deinit(&key_cell);
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(database, "failed to calculate AES value");
        return rc;
    }
    if (long_key_warning) {
        rc = aes_append_long_key_warning(database);
        if (rc != MYLITE_OK) {
            free(out_cell->owned_text);
            *out_cell = (struct session_scalar_cell){0};
            return rc;
        }
    }
    if (invalid) {
        return MYLITE_OK;
    }
    out_cell->value = out_cell->owned_text;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

static const struct mylite_sql_ast_node *aes_function_arguments(
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *child = expression == NULL ? NULL : expression->first_child;

    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
            return child;
        }
        child = child->next_sibling;
    }
    return NULL;
}

static const char *aes_function_name(const struct mylite_sql_ast_node *expression) {
    if (aes_function_name_matches(expression, "AES_ENCRYPT")) {
        return "AES_ENCRYPT";
    }
    if (aes_function_name_matches(expression, "AES_DECRYPT")) {
        return "AES_DECRYPT";
    }
    return NULL;
}

static bool aes_function_name_matches(
    const struct mylite_sql_ast_node *expression,
    const char *name
) {
    const struct mylite_sql_ast_node *child = expression == NULL ? NULL : expression->first_child;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_GENERIC_FUNCTION) {
        return false;
    }
    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_IDENTIFIER &&
            source_span_equals_ascii_case_insensitive(&child->span, name)) {
            return true;
        }
        child = child->next_sibling;
    }
    return false;
}

static bool source_span_equals_ascii_case_insensitive(
    const struct mylite_sql_source_span *span,
    const char *text
) {
    size_t length = text == NULL ? 0U : strlen(text);

    if (span == NULL || span->text == NULL || text == NULL || span->length != length) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        char left = span->text[index];
        char right = text[index];

        if (left >= 'a' && left <= 'z') {
            left = (char)(left - ('a' - 'A'));
        }
        if (right >= 'a' && right <= 'z') {
            right = (char)(right - ('a' - 'A'));
        }
        if (left != right) {
            return false;
        }
    }
    return true;
}

static int aes_append_long_key_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        aes_long_key_warning,
        "HY000",
        aes_long_key_warning_message
    );
}
