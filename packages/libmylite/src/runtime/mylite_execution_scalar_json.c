#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_json_internal.h"

#include "mylite_ast.h"
#include "mylite_json.h"

#include <mylite/mylite.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct json_object_function_buffers {
    struct mylite_json_sql_value *keys;
    struct mylite_json_sql_value *values;
    char **owned_texts;
    size_t owned_text_count;
};

struct json_contains_path_scalar_buffers {
    char **owned_paths;
    const char **paths;
    size_t *path_lengths;
    size_t path_count;
    size_t admitted_path_count;
    bool force_null;
};

static int evaluate_json_valid_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null,
    bool *out_is_binary
);
static int allocate_json_contains_path_scalar_buffers(
    struct mylite_db *database,
    size_t path_count,
    struct json_contains_path_scalar_buffers *out_buffers
);
static int decode_json_contains_path_scalar_paths(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *first_path,
    struct json_contains_path_scalar_buffers *buffers
);
static int format_json_search_scalar_result(
    struct mylite_db *database,
    int64_t contains,
    const char *failure_message,
    struct session_scalar_cell *out_cell
);
static void json_contains_path_scalar_buffers_deinit(
    struct json_contains_path_scalar_buffers *buffers
);
static bool is_json_mutation_function_expression(const struct mylite_sql_ast_node *expression);
static int json_mutation_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int allocate_json_object_function_buffers(
    struct mylite_db *database,
    size_t argument_count,
    size_t pair_count,
    struct json_object_function_buffers *out_buffers
);
static void free_json_object_function_buffers(struct json_object_function_buffers *buffers);
static int evaluate_json_object_scalar_pairs(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    size_t pair_count,
    struct json_object_function_buffers *buffers
);
static int json_constructor_integer_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value
);
static int json_extract_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int json_introspection_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    const char *function_name,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int json_value_path_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length
);
static int json_contains_path_one_or_all_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_require_all,
    bool *out_is_null
);
static int json_length_path_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int json_keys_path_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int json_unquote_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int json_quote_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int finish_json_value_scalar_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result,
    bool allow_invalid_document_warning
);
static int finish_json_length_scalar_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
);
static int finish_json_keys_scalar_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
);
static int finish_json_type_scalar_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
);
static int finish_json_contains_scalar_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
);
static int finish_json_contains_path_scalar_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
);
static int finish_json_unquote_scalar_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
);
static int finish_json_construction_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
);

int mylite_execution_scalar_json_valid_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell argument_cell = {0};
    char *owned_text = NULL;
    const char *text = NULL;
    size_t text_length = 0U;
    bool is_null = false;
    bool is_binary = false;
    bool is_valid = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_VALID_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, "JSON_VALID");
        return MYLITE_ERROR;
    }

    rc = evaluate_json_valid_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &argument_cell,
        &owned_text,
        &text,
        &text_length,
        &is_null,
        &is_binary
    );
    if (rc != MYLITE_OK || is_null) {
        free(owned_text);
        mylite_execution_session_scalar_cell_deinit(&argument_cell);
        return rc;
    }
    if (is_binary) {
        out_cell->value = "0";
        free(owned_text);
        mylite_execution_session_scalar_cell_deinit(&argument_cell);
        return MYLITE_OK;
    }

    rc = mylite_json_validate(text, text_length, &is_valid);
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
    } else if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(database, "failed to validate JSON text");
        rc = MYLITE_ERROR;
    } else if (is_valid) {
        out_cell->value = "1";
    } else {
        out_cell->value = "0";
    }

    free(owned_text);
    mylite_execution_session_scalar_cell_deinit(&argument_cell);
    return rc;
}

static int evaluate_json_valid_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null,
    bool *out_is_binary
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL || out_is_binary == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;
    *out_is_binary = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!mylite_execution_scalar_json_valid_scalar_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_VALID() supports only string, integer, boolean, NULL, and limited binary cast "
            "arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "JSON_VALID() supports only string literals",
                "JSON_VALID() string literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            *out_is_null = true;
            return MYLITE_OK;
        }
        *out_text = "0";
        *out_text_length = 1U;
        *out_is_binary = true;
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        *out_text = "0";
        *out_text_length = 1U;
        *out_is_binary = true;
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_CAST_BINARY_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION) {
        if (expression->kind == MYLITE_SQL_AST_CAST_BINARY_EXPRESSION) {
            rc = mylite_execution_cast_binary_value(database, expression, inout_cell);
        } else if (expression->kind == MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION) {
            rc = mylite_execution_convert_binary_type_value(database, expression, inout_cell);
        } else {
            rc = mylite_execution_convert_using_binary_value(database, expression, inout_cell);
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_is_null = inout_cell->value == NULL;
        *out_is_binary = true;
        return MYLITE_OK;
    }

    mylite_execution_set_unsupported_error(
        database,
        "JSON_VALID() supports only string, integer, boolean, NULL, and limited binary cast "
        "arguments"
    );
    return MYLITE_ERROR;
}

bool mylite_execution_scalar_json_valid_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        return (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
                literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
                literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
                literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
                literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            mylite_execution_unwrap_parenthesized_expression(
                mylite_execution_child_at(expression, 0U)
            );

        return ((operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
                 operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) &&
                literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
                mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) != 0;
    }
    if (expression->kind == MYLITE_SQL_AST_CAST_BINARY_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION) {
        return true;
    }
    return false;
}

int mylite_execution_scalar_json_contains_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell target_cell = {0};
    struct session_scalar_cell candidate_cell = {0};
    char *owned_target = NULL;
    char *owned_candidate = NULL;
    char *owned_path = NULL;
    const char *target = NULL;
    const char *candidate = NULL;
    const char *path = NULL;
    size_t target_length = 0U;
    size_t candidate_length = 0U;
    size_t path_length = 0U;
    size_t child_count = 0U;
    int64_t contains = 0;
    bool target_is_null = false;
    bool candidate_is_null = false;
    bool path_is_null = false;
    bool result_is_null = false;
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_CONTAINS_FUNCTION) {
        mylite_execution_set_native_function_parameter_count_error(database, "JSON_CONTAINS");
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count != 2U && child_count != 3U) {
        mylite_execution_set_native_function_parameter_count_error(database, "JSON_CONTAINS");
        return MYLITE_ERROR;
    }

    rc = json_introspection_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &target_cell,
        "JSON_CONTAINS",
        &owned_target,
        &target,
        &target_length,
        &target_is_null
    );
    if (rc != MYLITE_OK || target_is_null) {
        goto done;
    }

    rc = json_introspection_scalar_argument(
        database,
        mylite_execution_child_at(expression, 1U),
        &candidate_cell,
        "JSON_CONTAINS",
        &owned_candidate,
        &candidate,
        &candidate_length,
        &candidate_is_null
    );
    if (rc != MYLITE_OK || candidate_is_null) {
        goto done;
    }

    if (child_count == 3U) {
        rc = mylite_execution_scalar_json_path_argument(
            database,
            mylite_execution_child_at(expression, 2U),
            &owned_path,
            &path,
            &path_length,
            &path_is_null
        );
        if (rc != MYLITE_OK || path_is_null) {
            goto done;
        }
    }

    rc = mylite_json_contains(
        target,
        target_length,
        candidate,
        candidate_length,
        path,
        path_length,
        child_count == 3U,
        &contains,
        &result_is_null,
        &result
    );
    rc = finish_json_contains_scalar_result(database, rc, &result);
    if (rc == MYLITE_OK && !result_is_null) {
        rc = format_json_search_scalar_result(
            database,
            contains,
            "failed to format JSON_CONTAINS() value",
            out_cell
        );
    }

done:
    free(owned_target);
    free(owned_candidate);
    free(owned_path);
    mylite_execution_session_scalar_cell_deinit(&target_cell);
    mylite_execution_session_scalar_cell_deinit(&candidate_cell);
    return rc;
}

int mylite_execution_scalar_json_contains_path_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *argument = NULL;
    struct session_scalar_cell document_cell = {0};
    struct json_contains_path_scalar_buffers path_buffers = {0};
    char *owned_document = NULL;
    const char *document = NULL;
    size_t argument_count = 0U;
    size_t document_length = 0U;
    int64_t contains = 0;
    bool document_is_null = false;
    bool require_all = false;
    bool mode_is_null = false;
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_CONTAINS_PATH_FUNCTION) {
        mylite_execution_set_native_function_parameter_count_error(database, "JSON_CONTAINS_PATH");
        return MYLITE_ERROR;
    }
    rc = mylite_execution_scalar_json_collect_function_arguments(
        database,
        expression,
        "JSON_CONTAINS_PATH",
        &arguments,
        &argument_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument_count < 3U) {
        mylite_execution_set_native_function_parameter_count_error(database, "JSON_CONTAINS_PATH");
        return MYLITE_ERROR;
    }

    argument = mylite_execution_child_at(arguments, 0U);
    rc = json_introspection_scalar_argument(
        database,
        argument,
        &document_cell,
        "JSON_CONTAINS_PATH",
        &owned_document,
        &document,
        &document_length,
        &document_is_null
    );
    if (rc != MYLITE_OK || document_is_null) {
        goto done;
    }

    rc = mylite_json_contains_path(
        document,
        document_length,
        NULL,
        NULL,
        0U,
        false,
        &contains,
        &result
    );
    rc = finish_json_contains_path_scalar_result(database, rc, &result);
    if (rc != MYLITE_OK) {
        goto done;
    }

    argument = argument == NULL ? NULL : argument->next_sibling;
    rc = json_contains_path_one_or_all_value(database, argument, &require_all, &mode_is_null);
    if (rc != MYLITE_OK || mode_is_null) {
        goto done;
    }

    rc = allocate_json_contains_path_scalar_buffers(database, argument_count - 2U, &path_buffers);
    if (rc != MYLITE_OK) {
        goto done;
    }
    argument = argument == NULL ? NULL : argument->next_sibling;
    rc = decode_json_contains_path_scalar_paths(database, argument, &path_buffers);
    if (rc != MYLITE_OK) {
        goto done;
    }

    rc = mylite_json_contains_path(
        document,
        document_length,
        path_buffers.paths,
        path_buffers.path_lengths,
        path_buffers.admitted_path_count,
        require_all,
        &contains,
        &result
    );
    rc = finish_json_contains_path_scalar_result(database, rc, &result);
    if (rc == MYLITE_OK && !path_buffers.force_null) {
        rc = format_json_search_scalar_result(
            database,
            contains,
            "failed to format JSON_CONTAINS_PATH() value",
            out_cell
        );
    }

done:
    json_contains_path_scalar_buffers_deinit(&path_buffers);
    free(owned_document);
    mylite_execution_session_scalar_cell_deinit(&document_cell);
    return rc;
}

static int allocate_json_contains_path_scalar_buffers(
    struct mylite_db *database,
    size_t path_count,
    struct json_contains_path_scalar_buffers *out_buffers
) {
    if (out_buffers == NULL) {
        return MYLITE_MISUSE;
    }
    *out_buffers = (struct json_contains_path_scalar_buffers){0};
    if (path_count > SIZE_MAX / sizeof(*out_buffers->paths) ||
        path_count > SIZE_MAX / sizeof(*out_buffers->path_lengths) ||
        path_count > SIZE_MAX / sizeof(*out_buffers->owned_paths)) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_buffers->path_count = path_count;
    out_buffers->paths = (const char **)calloc(path_count, sizeof(*out_buffers->paths));
    out_buffers->path_lengths = (size_t *)calloc(path_count, sizeof(*out_buffers->path_lengths));
    out_buffers->owned_paths = (char **)calloc(path_count, sizeof(*out_buffers->owned_paths));
    if (out_buffers->paths == NULL || out_buffers->path_lengths == NULL ||
        out_buffers->owned_paths == NULL) {
        json_contains_path_scalar_buffers_deinit(out_buffers);
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int decode_json_contains_path_scalar_paths(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *first_path,
    struct json_contains_path_scalar_buffers *buffers
) {
    const struct mylite_sql_ast_node *argument = first_path;
    int rc = MYLITE_OK;

    if (buffers == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t path_index = 0U; rc == MYLITE_OK && !buffers->force_null &&
                                 path_index < buffers->path_count && argument != NULL;
         ++path_index) {
        bool path_is_null = false;

        rc = mylite_execution_scalar_json_path_argument(
            database,
            argument,
            &buffers->owned_paths[path_index],
            &buffers->paths[path_index],
            &buffers->path_lengths[path_index],
            &path_is_null
        );
        if (rc == MYLITE_OK && path_is_null) {
            buffers->force_null = true;
        } else if (rc == MYLITE_OK) {
            buffers->admitted_path_count = path_index + 1U;
        }
        argument = argument->next_sibling;
    }
    return rc;
}

static int format_json_search_scalar_result(
    struct mylite_db *database,
    int64_t contains,
    const char *failure_message,
    struct session_scalar_cell *out_cell
) {
    int written = 0;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    written =
        snprintf(out_cell->integer_text, sizeof(out_cell->integer_text), "%" PRId64, contains);
    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        mylite_execution_set_runtime_error(database, failure_message);
        return MYLITE_ERROR;
    }
    out_cell->value = out_cell->integer_text;
    return MYLITE_OK;
}

static void json_contains_path_scalar_buffers_deinit(
    struct json_contains_path_scalar_buffers *buffers
) {
    if (buffers == NULL) {
        return;
    }
    if (buffers->owned_paths != NULL) {
        for (size_t path_index = 0U; path_index < buffers->path_count; ++path_index) {
            free(buffers->owned_paths[path_index]);
        }
    }
    free((void *)buffers->owned_paths);
    free((void *)buffers->path_lengths);
    free((void *)buffers->paths);
    *buffers = (struct json_contains_path_scalar_buffers){0};
}

int mylite_execution_scalar_json_extract_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell document_cell = {0};
    char *owned_document = NULL;
    char *owned_path = NULL;
    char *result_text = NULL;
    const char *document = NULL;
    const char *path = NULL;
    size_t document_length = 0U;
    size_t path_length = 0U;
    size_t result_length = 0U;
    bool document_is_null = false;
    bool path_is_null = false;
    bool result_is_null = false;
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION) {
        mylite_execution_set_native_function_parameter_count_error(database, "JSON_EXTRACT");
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(expression) != 2U) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_EXTRACT() multiple path arguments are not supported"
        );
        return MYLITE_ERROR;
    }

    rc = json_extract_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &document_cell,
        &owned_document,
        &document,
        &document_length,
        &document_is_null
    );
    if (rc == MYLITE_OK) {
        rc = mylite_execution_scalar_json_path_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &owned_path,
            &path,
            &path_length,
            &path_is_null
        );
    }
    if (rc == MYLITE_OK && (document_is_null || path_is_null)) {
        out_cell->value = NULL;
    } else if (rc == MYLITE_OK) {
        rc = mylite_json_extract(
            document,
            document_length,
            path,
            path_length,
            &result_text,
            &result_length,
            &result_is_null,
            &result
        );
        rc = mylite_execution_scalar_json_finish_extract_result(database, rc, &result);
        if (rc == MYLITE_OK && result_is_null) {
            out_cell->value = NULL;
        } else if (rc == MYLITE_OK) {
            (void)result_length;
            out_cell->owned_text = result_text;
            out_cell->value = out_cell->owned_text;
            result_text = NULL;
        }
    }

    free(result_text);
    free(owned_document);
    free(owned_path);
    mylite_execution_session_scalar_cell_deinit(&document_cell);
    return rc;
}

int mylite_execution_scalar_json_value_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell document_cell = {0};
    char *owned_document = NULL;
    char *owned_path = NULL;
    char *result_text = NULL;
    const char *document = NULL;
    const char *path = NULL;
    size_t document_length = 0U;
    size_t path_length = 0U;
    size_t result_length = 0U;
    bool document_is_null = false;
    bool result_is_null = false;
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_VALUE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        mylite_execution_set_parse_error(database);
        return MYLITE_ERROR;
    }

    rc = json_extract_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &document_cell,
        &owned_document,
        &document,
        &document_length,
        &document_is_null
    );
    if (rc == MYLITE_OK) {
        rc = json_value_path_scalar_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &owned_path,
            &path,
            &path_length
        );
    }
    if (rc == MYLITE_OK && document_is_null) {
        out_cell->value = NULL;
    } else if (rc == MYLITE_OK) {
        rc = mylite_json_value(
            document,
            document_length,
            path,
            path_length,
            &result_text,
            &result_length,
            &result_is_null,
            &result
        );
        rc = finish_json_value_scalar_result(database, rc, &result, true);
        if (rc == MYLITE_OK && (result_text == NULL || result_is_null)) {
            out_cell->value = NULL;
        } else if (rc == MYLITE_OK) {
            (void)result_length;
            out_cell->owned_text = result_text;
            out_cell->value = out_cell->owned_text;
            result_text = NULL;
        }
    }

    free(result_text);
    free(owned_document);
    free(owned_path);
    mylite_execution_session_scalar_cell_deinit(&document_cell);
    return rc;
}

int mylite_execution_scalar_json_length_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell document_cell = {0};
    char *owned_document = NULL;
    char *owned_path = NULL;
    const char *document = NULL;
    const char *path = NULL;
    size_t document_length = 0U;
    size_t path_length = 0U;
    size_t child_count = 0U;
    int64_t length = 0;
    bool document_is_null = false;
    bool path_is_null = false;
    bool result_is_null = false;
    bool has_path = false;
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_LENGTH_FUNCTION) {
        mylite_execution_set_native_function_parameter_count_error(database, "JSON_LENGTH");
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count != 1U && child_count != 2U) {
        mylite_execution_set_native_function_parameter_count_error(database, "JSON_LENGTH");
        return MYLITE_ERROR;
    }

    rc = json_introspection_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &document_cell,
        "JSON_LENGTH",
        &owned_document,
        &document,
        &document_length,
        &document_is_null
    );
    if (rc == MYLITE_OK && document_is_null) {
        out_cell->value = NULL;
    }
    if (rc == MYLITE_OK && !document_is_null && child_count == 2U) {
        rc = json_length_path_scalar_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &owned_path,
            &path,
            &path_length,
            &path_is_null
        );
    }
    if (rc == MYLITE_OK && !document_is_null) {
        has_path = (child_count == 2U && !path_is_null) != 0;
        rc = mylite_json_length(
            document,
            document_length,
            path,
            path_length,
            has_path,
            &length,
            &result_is_null,
            &result
        );
        rc = finish_json_length_scalar_result(database, rc, &result);
        if (rc == MYLITE_OK && (path_is_null || result_is_null)) {
            out_cell->value = NULL;
        } else if (rc == MYLITE_OK) {
            int written = snprintf(
                out_cell->integer_text,
                sizeof(out_cell->integer_text),
                "%" PRId64,
                length
            );

            if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
                mylite_execution_set_runtime_error(
                    database,
                    "failed to format JSON_LENGTH() value"
                );
                rc = MYLITE_ERROR;
            } else {
                out_cell->value = out_cell->integer_text;
            }
        }
    }

    free(owned_document);
    free(owned_path);
    mylite_execution_session_scalar_cell_deinit(&document_cell);
    return rc;
}

int mylite_execution_scalar_json_keys_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell document_cell = {0};
    char *owned_document = NULL;
    char *owned_path = NULL;
    char *result_text = NULL;
    const char *document = NULL;
    const char *path = NULL;
    size_t document_length = 0U;
    size_t path_length = 0U;
    size_t result_text_length = 0U;
    size_t child_count = 0U;
    bool document_is_null = false;
    bool path_is_null = false;
    bool result_is_null = false;
    bool has_path = false;
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_KEYS_FUNCTION) {
        mylite_execution_set_native_function_parameter_count_error(database, "JSON_KEYS");
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count != 1U && child_count != 2U) {
        mylite_execution_set_native_function_parameter_count_error(database, "JSON_KEYS");
        return MYLITE_ERROR;
    }

    rc = json_introspection_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &document_cell,
        "JSON_KEYS",
        &owned_document,
        &document,
        &document_length,
        &document_is_null
    );
    if (rc == MYLITE_OK && document_is_null) {
        out_cell->value = NULL;
    }
    if (rc == MYLITE_OK && !document_is_null && child_count == 2U) {
        rc = json_keys_path_scalar_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &owned_path,
            &path,
            &path_length,
            &path_is_null
        );
    }
    if (rc == MYLITE_OK && !document_is_null) {
        has_path = (child_count == 2U && !path_is_null) != 0;
        rc = mylite_json_keys(
            document,
            document_length,
            path,
            path_length,
            has_path,
            &result_text,
            &result_text_length,
            &result_is_null,
            &result
        );
        rc = finish_json_keys_scalar_result(database, rc, &result);
        if (rc == MYLITE_OK && (path_is_null || result_is_null)) {
            out_cell->value = NULL;
        } else if (rc == MYLITE_OK) {
            (void)result_text_length;
            out_cell->owned_text = result_text;
            out_cell->value = out_cell->owned_text;
            result_text = NULL;
        }
    }

    free(result_text);
    free(owned_document);
    free(owned_path);
    mylite_execution_session_scalar_cell_deinit(&document_cell);
    return rc;
}

int mylite_execution_scalar_json_type_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell document_cell = {0};
    char *owned_document = NULL;
    const char *document = NULL;
    const char *type = NULL;
    size_t document_length = 0U;
    bool document_is_null = false;
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_TYPE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, "JSON_TYPE");
        return MYLITE_ERROR;
    }

    rc = json_introspection_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &document_cell,
        "JSON_TYPE",
        &owned_document,
        &document,
        &document_length,
        &document_is_null
    );
    if (rc == MYLITE_OK && document_is_null) {
        out_cell->value = NULL;
    } else if (rc == MYLITE_OK) {
        rc = mylite_json_type(document, document_length, &type, &result);
        rc = finish_json_type_scalar_result(database, rc, &result);
        if (rc == MYLITE_OK) {
            out_cell->value = type;
        }
    }

    free(owned_document);
    mylite_execution_session_scalar_cell_deinit(&document_cell);
    return rc;
}

int mylite_execution_scalar_json_quote_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    char *owned_argument = NULL;
    char *result_text = NULL;
    const char *argument = NULL;
    size_t argument_length = 0U;
    size_t result_length = 0U;
    bool argument_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_QUOTE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, "JSON_QUOTE");
        return MYLITE_ERROR;
    }

    rc = json_quote_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &owned_argument,
        &argument,
        &argument_length,
        &argument_is_null
    );
    if (rc == MYLITE_OK && argument_is_null) {
        out_cell->value = NULL;
    } else if (rc == MYLITE_OK) {
        rc = mylite_json_quote_string(argument, argument_length, &result_text, &result_length);
        if (rc == MYLITE_NOMEM) {
            mylite_execution_set_nomem_error(database);
        } else if (rc == MYLITE_OK) {
            (void)result_length;
            out_cell->owned_text = result_text;
            out_cell->value = out_cell->owned_text;
            result_text = NULL;
        }
    }

    free(result_text);
    free(owned_argument);
    return rc;
}

int mylite_execution_scalar_json_unquote_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell argument_cell = {0};
    char *owned_argument = NULL;
    char *result_text = NULL;
    const char *argument = NULL;
    size_t argument_length = 0U;
    size_t result_length = 0U;
    bool argument_is_null = false;
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, "JSON_UNQUOTE");
        return MYLITE_ERROR;
    }

    rc = json_unquote_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &argument_cell,
        &owned_argument,
        &argument,
        &argument_length,
        &argument_is_null
    );
    if (rc == MYLITE_OK && argument_is_null) {
        out_cell->value = NULL;
    } else if (rc == MYLITE_OK) {
        rc = mylite_json_unquote(argument, argument_length, &result_text, &result_length, &result);
        rc = finish_json_unquote_scalar_result(database, rc, &result);
        if (rc == MYLITE_OK) {
            (void)result_length;
            out_cell->owned_text = result_text;
            out_cell->value = out_cell->owned_text;
            result_text = NULL;
        }
    }

    free(result_text);
    free(owned_argument);
    mylite_execution_session_scalar_cell_deinit(&argument_cell);
    return rc;
}

int mylite_execution_scalar_json_array_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *argument = NULL;
    struct mylite_json_sql_value *values = NULL;
    char **owned_texts = NULL;
    char *result_text = NULL;
    size_t argument_count = 0U;
    size_t result_length = 0U;
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_ARRAY_FUNCTION) {
        mylite_execution_set_native_function_parameter_count_error(database, "JSON_ARRAY");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_scalar_json_collect_function_arguments(
        database,
        expression,
        "JSON_ARRAY",
        &arguments,
        &argument_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument_count > SIZE_MAX / sizeof(*values) ||
        argument_count > SIZE_MAX / sizeof(*owned_texts)) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = argument_count == 0U
                 ? NULL
                 : (struct mylite_json_sql_value *)calloc(argument_count, sizeof(*values));
    owned_texts =
        argument_count == 0U ? NULL : (char **)calloc(argument_count, sizeof(*owned_texts));
    if ((argument_count != 0U && values == NULL) || (argument_count != 0U && owned_texts == NULL)) {
        free(values);
        free((void *)owned_texts);
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    argument = arguments == NULL ? NULL : mylite_execution_child_at(arguments, 0U);
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         ++argument_index) {
        rc = mylite_execution_scalar_json_evaluate_constructor_argument(
            database,
            argument,
            &values[argument_index],
            &owned_texts[argument_index]
        );
        argument = argument->next_sibling;
    }
    if (rc == MYLITE_OK && argument != NULL) {
        mylite_execution_set_parse_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_json_array_from_sql_values(
            values,
            argument_count,
            &result_text,
            &result_length,
            &result
        );
        rc = finish_json_construction_result(database, rc, &result);
    }
    if (rc == MYLITE_OK) {
        (void)result_length;
        out_cell->owned_text = result_text;
        out_cell->value = out_cell->owned_text;
        result_text = NULL;
    }

    free(result_text);
    for (size_t argument_index = 0U; argument_index < argument_count; ++argument_index) {
        free(owned_texts[argument_index]);
    }
    free((void *)owned_texts);
    free(values);
    return rc;
}

int mylite_execution_scalar_json_object_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    struct json_object_function_buffers buffers = {0};
    char *result_text = NULL;
    size_t argument_count = 0U;
    size_t pair_count = 0U;
    size_t result_length = 0U;
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_OBJECT_FUNCTION) {
        mylite_execution_set_native_function_parameter_count_error(database, "JSON_OBJECT");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_scalar_json_collect_function_arguments(
        database,
        expression,
        "JSON_OBJECT",
        &arguments,
        &argument_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if ((argument_count % 2U) != 0U) {
        mylite_execution_set_native_function_parameter_count_error(database, "JSON_OBJECT");
        return MYLITE_ERROR;
    }
    pair_count = argument_count / 2U;
    rc = allocate_json_object_function_buffers(database, argument_count, pair_count, &buffers);
    if (rc == MYLITE_OK) {
        rc = evaluate_json_object_scalar_pairs(database, arguments, pair_count, &buffers);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_json_object_from_sql_values(
            buffers.keys,
            buffers.values,
            pair_count,
            &result_text,
            &result_length,
            &result
        );
        rc = finish_json_construction_result(database, rc, &result);
    }
    if (rc == MYLITE_OK) {
        (void)result_length;
        out_cell->owned_text = result_text;
        out_cell->value = out_cell->owned_text;
        result_text = NULL;
    }

    free(result_text);
    free_json_object_function_buffers(&buffers);
    return rc;
}

static int allocate_json_object_function_buffers(
    struct mylite_db *database,
    size_t argument_count,
    size_t pair_count,
    struct json_object_function_buffers *out_buffers
) {
    struct mylite_json_sql_value *keys = NULL;
    struct mylite_json_sql_value *values = NULL;
    char **owned_texts = NULL;

    if (out_buffers == NULL) {
        return MYLITE_MISUSE;
    }
    *out_buffers = (struct json_object_function_buffers){0};

    if (pair_count > SIZE_MAX / sizeof(*keys) || pair_count > SIZE_MAX / sizeof(*values) ||
        argument_count > SIZE_MAX / sizeof(*owned_texts)) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    keys =
        pair_count == 0U ? NULL : (struct mylite_json_sql_value *)calloc(pair_count, sizeof(*keys));
    values = pair_count == 0U ? NULL
                              : (struct mylite_json_sql_value *)calloc(pair_count, sizeof(*values));
    owned_texts =
        argument_count == 0U ? NULL : (char **)calloc(argument_count, sizeof(*owned_texts));
    if ((pair_count != 0U && (keys == NULL || values == NULL)) ||
        (argument_count != 0U && owned_texts == NULL)) {
        free(keys);
        free(values);
        free((void *)owned_texts);
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_buffers->keys = keys;
    out_buffers->values = values;
    out_buffers->owned_texts = owned_texts;
    out_buffers->owned_text_count = argument_count;
    return MYLITE_OK;
}

static void free_json_object_function_buffers(struct json_object_function_buffers *buffers) {
    if (buffers == NULL) {
        return;
    }
    for (size_t argument_index = 0U; argument_index < buffers->owned_text_count; ++argument_index) {
        free(buffers->owned_texts[argument_index]);
    }
    free((void *)buffers->owned_texts);
    free(buffers->values);
    free(buffers->keys);
    *buffers = (struct json_object_function_buffers){0};
}

static int evaluate_json_object_scalar_pairs(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    size_t pair_count,
    struct json_object_function_buffers *buffers
) {
    const struct mylite_sql_ast_node *argument =
        arguments == NULL ? NULL : mylite_execution_child_at(arguments, 0U);

    if (buffers == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t pair_index = 0U; pair_index < pair_count; ++pair_index) {
        int rc = mylite_execution_scalar_json_evaluate_constructor_argument(
            database,
            argument,
            &buffers->keys[pair_index],
            &buffers->owned_texts[pair_index * 2U]
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        if (buffers->keys[pair_index].kind == MYLITE_JSON_SQL_VALUE_NULL) {
            mylite_execution_set_json_null_member_name_error(database);
            return MYLITE_ERROR;
        }

        argument = argument == NULL ? NULL : argument->next_sibling;
        rc = mylite_execution_scalar_json_evaluate_constructor_argument(
            database,
            argument,
            &buffers->values[pair_index],
            &buffers->owned_texts[(pair_index * 2U) + 1U]
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        argument = argument == NULL ? NULL : argument->next_sibling;
    }
    if (argument != NULL) {
        mylite_execution_set_parse_error(database);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

int mylite_execution_scalar_json_collect_function_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    const struct mylite_sql_ast_node **out_arguments,
    size_t *out_argument_count
) {
    size_t child_count = 0U;
    const struct mylite_sql_ast_node *arguments = NULL;

    if (out_arguments == NULL || out_argument_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_arguments = NULL;
    *out_argument_count = 0U;
    if (expression == NULL) {
        mylite_execution_set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }

    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count == 0U) {
        return MYLITE_OK;
    }
    if (child_count != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }
    arguments = mylite_execution_child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        mylite_execution_set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }

    *out_arguments = arguments;
    *out_argument_count = mylite_sql_ast_node_child_count(arguments);
    return MYLITE_OK;
}

int mylite_execution_scalar_json_evaluate_constructor_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_json_sql_value *out_value,
    char **out_owned_text
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (out_value == NULL || out_owned_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct mylite_json_sql_value){0};
    *out_owned_text = NULL;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON constructors support only string, integer, boolean, NULL, and descriptor column "
            "arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            int rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "JSON constructors support only string literals",
                "JSON constructor string literals do not support NUL bytes",
                out_owned_text,
                &out_value->text_length
            );

            if (rc == MYLITE_OK) {
                out_value->kind = MYLITE_JSON_SQL_VALUE_STRING;
                out_value->text = *out_owned_text;
            }
            return rc;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
            int64_t integer = 0;
            int rc = json_constructor_integer_literal_value(database, expression, &integer);

            if (rc == MYLITE_OK) {
                out_value->kind = MYLITE_JSON_SQL_VALUE_INTEGER;
                out_value->integer = integer;
            }
            return rc;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
            out_value->kind = MYLITE_JSON_SQL_VALUE_BOOLEAN;
            out_value->boolean = true;
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
            out_value->kind = MYLITE_JSON_SQL_VALUE_BOOLEAN;
            out_value->boolean = false;
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            out_value->kind = MYLITE_JSON_SQL_VALUE_NULL;
            return MYLITE_OK;
        }
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        int64_t integer = 0;
        int rc = json_constructor_integer_literal_value(database, expression, &integer);

        if (rc == MYLITE_OK) {
            out_value->kind = MYLITE_JSON_SQL_VALUE_INTEGER;
            out_value->integer = integer;
        }
        return rc;
    }

    mylite_execution_set_unsupported_error(
        database,
        "JSON constructors support only string, integer, boolean, NULL, and descriptor column "
        "arguments"
    );
    return MYLITE_ERROR;
}

static int json_constructor_integer_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value
) {
    const struct mylite_sql_ast_node *literal = expression;
    bool is_negative = false;
    uint64_t magnitude = 0U;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0;
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            mylite_execution_set_unsupported_error(
                database,
                "JSON constructors support only signed integer literals"
            );
            return MYLITE_ERROR;
        }
        literal = mylite_execution_unwrap_parenthesized_expression(
            mylite_execution_child_at(expression, 0U)
        );
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON constructors support only signed integer literals"
        );
        return MYLITE_ERROR;
    }

    if (mylite_execution_parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        (is_negative && magnitude > (uint64_t)INT64_MAX + 1U) ||
        (!is_negative && magnitude > (uint64_t)INT64_MAX)) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON constructor integer literals must fit the signed 64-bit range"
        );
        return MYLITE_ERROR;
    }
    if (is_negative && magnitude == (uint64_t)INT64_MAX + 1U) {
        *out_value = INT64_MIN;
    } else if (is_negative) {
        *out_value = -(int64_t)magnitude;
    } else {
        *out_value = (int64_t)magnitude;
    }
    return MYLITE_OK;
}

static int json_extract_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_EXTRACT() supports only string, NULL, JSON_SET(), JSON_INSERT(), "
            "JSON_REPLACE(), and JSON_REMOVE() document arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_SET_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_INSERT_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_REPLACE_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_REMOVE_FUNCTION) {
        int rc_set = MYLITE_OK;

        if (expression->kind == MYLITE_SQL_AST_JSON_INSERT_FUNCTION) {
            rc_set = mylite_execution_scalar_json_insert_function_value(
                database,
                expression,
                inout_cell
            );
        } else if (expression->kind == MYLITE_SQL_AST_JSON_REPLACE_FUNCTION) {
            rc_set = mylite_execution_scalar_json_replace_function_value(
                database,
                expression,
                inout_cell
            );
        } else if (expression->kind == MYLITE_SQL_AST_JSON_REMOVE_FUNCTION) {
            rc_set = mylite_execution_scalar_json_remove_function_value(
                database,
                expression,
                inout_cell
            );
        } else {
            rc_set =
                mylite_execution_scalar_json_set_function_value(database, expression, inout_cell);
        }

        if (rc_set != MYLITE_OK) {
            return rc_set;
        }
        if (inout_cell->value == NULL) {
            *out_is_null = true;
            return MYLITE_OK;
        }
        *out_text = inout_cell->value;
        *out_text_length = strlen(inout_cell->value);
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            int rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "JSON_EXTRACT() supports only string document literals",
                "JSON_EXTRACT() document literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );

            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            *out_is_null = true;
            return MYLITE_OK;
        }
        mylite_execution_set_invalid_json_data_type_error(database, "JSON_EXTRACT");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        mylite_execution_set_invalid_json_data_type_error(database, "JSON_EXTRACT");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_CAST_BINARY_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION) {
        (void)inout_cell;
        mylite_execution_set_json_binary_charset_error(database);
        return MYLITE_ERROR;
    }

    mylite_execution_set_unsupported_error(
        database,
        "JSON_EXTRACT() supports only string, NULL, JSON_SET(), JSON_INSERT(), JSON_REPLACE(), "
        "and JSON_REMOVE() document arguments"
    );
    return MYLITE_ERROR;
}

static int json_introspection_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    const char *function_name,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL || function_name == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON introspection supports only string, NULL, JSON_EXTRACT(), JSON_SET(), "
            "JSON_INSERT(), JSON_REPLACE(), JSON_REMOVE(), and descriptor column arguments"
        );
        return MYLITE_ERROR;
    }
    if (is_json_mutation_function_expression(expression)) {
        return json_mutation_scalar_argument(
            database,
            expression,
            inout_cell,
            out_text,
            out_text_length,
            out_is_null
        );
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION) {
        rc = mylite_execution_scalar_json_extract_function_value(database, expression, inout_cell);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (inout_cell->value == NULL) {
            *out_is_null = true;
            return MYLITE_OK;
        }
        *out_text = inout_cell->value;
        *out_text_length = strlen(inout_cell->value);
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            int rc_decode = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "JSON introspection supports only string document literals",
                "JSON introspection document literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );

            if (rc_decode == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc_decode;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            *out_is_null = true;
            return MYLITE_OK;
        }
        mylite_execution_set_invalid_json_data_type_error(database, function_name);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        mylite_execution_set_invalid_json_data_type_error(database, function_name);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_CAST_BINARY_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION) {
        mylite_execution_set_json_binary_charset_error(database);
        return MYLITE_ERROR;
    }

    mylite_execution_set_unsupported_error(
        database,
        "JSON introspection supports only string, NULL, JSON_EXTRACT(), JSON_SET(), "
        "JSON_INSERT(), JSON_REPLACE(), JSON_REMOVE(), and descriptor column arguments"
    );
    return MYLITE_ERROR;
}

static bool is_json_mutation_function_expression(const struct mylite_sql_ast_node *expression) {
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_SET_FUNCTION) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_INSERT_FUNCTION) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_REPLACE_FUNCTION) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_REMOVE_FUNCTION) {
        return true;
    }
    return false;
}

static int json_mutation_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    int rc = MYLITE_OK;

    if (expression->kind == MYLITE_SQL_AST_JSON_INSERT_FUNCTION) {
        rc = mylite_execution_scalar_json_insert_function_value(database, expression, inout_cell);
    } else if (expression->kind == MYLITE_SQL_AST_JSON_REPLACE_FUNCTION) {
        rc = mylite_execution_scalar_json_replace_function_value(database, expression, inout_cell);
    } else if (expression->kind == MYLITE_SQL_AST_JSON_REMOVE_FUNCTION) {
        rc = mylite_execution_scalar_json_remove_function_value(database, expression, inout_cell);
    } else {
        rc = mylite_execution_scalar_json_set_function_value(database, expression, inout_cell);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

int mylite_execution_scalar_json_path_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    if (out_owned_text == NULL || out_text == NULL || out_text_length == NULL ||
        out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_EXTRACT() supports only string and NULL path literals"
        );
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_EXTRACT() supports only string and NULL path literals"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_execution_decode_sql_string_literal(
        database,
        expression,
        "JSON_EXTRACT() supports only string path literals",
        "JSON_EXTRACT() path literals do not support NUL bytes",
        out_owned_text,
        out_text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    *out_text = *out_owned_text;

    rc = mylite_json_path_validate(*out_text, *out_text_length, &result);
    return mylite_execution_scalar_json_finish_path_result(database, rc, &result);
}

static int json_value_path_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    if (out_owned_text == NULL || out_text == NULL || out_text_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_VALUE() supports only string path literals"
        );
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_VALUE() supports only string path literals"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_execution_decode_sql_string_literal(
        database,
        expression,
        "JSON_VALUE() supports only string path literals",
        "JSON_VALUE() path literals do not support NUL bytes",
        out_owned_text,
        out_text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    *out_text = *out_owned_text;

    rc = mylite_json_path_validate(*out_text, *out_text_length, &result);
    return mylite_execution_scalar_json_finish_path_result(database, rc, &result);
}

static int json_contains_path_one_or_all_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_require_all,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *owned_text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (out_require_all == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_require_all = false;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_CONTAINS_PATH() supports only string and NULL one_or_all arguments"
        );
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        mylite_execution_set_invalid_json_one_or_all_error(database);
        return MYLITE_ERROR;
    }

    rc = mylite_execution_decode_sql_string_literal(
        database,
        expression,
        "JSON_CONTAINS_PATH() supports only string one_or_all literals",
        "JSON_CONTAINS_PATH() one_or_all literals do not support NUL bytes",
        &owned_text,
        &text_length
    );
    if (rc == MYLITE_OK) {
        if (mylite_execution_text_equals_ascii_case_insensitive(owned_text, "all")) {
            *out_require_all = true;
        } else if (mylite_execution_text_equals_ascii_case_insensitive(owned_text, "one")) {
            *out_require_all = false;
        } else {
            mylite_execution_set_invalid_json_one_or_all_error(database);
            rc = MYLITE_ERROR;
        }
    }

    free(owned_text);
    return rc;
}

static int json_length_path_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    if (out_owned_text == NULL || out_text == NULL || out_text_length == NULL ||
        out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_LENGTH() supports only string and NULL path literals"
        );
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_LENGTH() supports only string and NULL path literals"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_execution_decode_sql_string_literal(
        database,
        expression,
        "JSON_LENGTH() supports only string path literals",
        "JSON_LENGTH() path literals do not support NUL bytes",
        out_owned_text,
        out_text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    *out_text = *out_owned_text;

    rc = mylite_json_path_validate(*out_text, *out_text_length, &result);
    return mylite_execution_scalar_json_finish_length_path_result(database, rc, &result);
}

static int json_keys_path_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    if (out_owned_text == NULL || out_text == NULL || out_text_length == NULL ||
        out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_KEYS() supports only string and NULL path literals"
        );
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_KEYS() supports only string and NULL path literals"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_execution_decode_sql_string_literal(
        database,
        expression,
        "JSON_KEYS() supports only string path literals",
        "JSON_KEYS() path literals do not support NUL bytes",
        out_owned_text,
        out_text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    *out_text = *out_owned_text;

    rc = mylite_json_path_validate(*out_text, *out_text_length, &result);
    return mylite_execution_scalar_json_finish_keys_path_result(database, rc, &result);
}

static int json_unquote_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_UNQUOTE() supports only string, NULL, and JSON_EXTRACT() arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION) {
        rc = mylite_execution_scalar_json_extract_function_value(database, expression, inout_cell);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (inout_cell->value == NULL) {
            *out_is_null = true;
            return MYLITE_OK;
        }
        *out_text = inout_cell->value;
        *out_text_length = strlen(inout_cell->value);
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "JSON_UNQUOTE() supports only string literals",
                "JSON_UNQUOTE() string literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            *out_is_null = true;
            return MYLITE_OK;
        }
        mylite_execution_set_json_unquote_incorrect_type_error(database);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        mylite_execution_set_json_unquote_incorrect_type_error(database);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_CAST_BINARY_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION) {
        mylite_execution_set_json_binary_charset_error(database);
        return MYLITE_ERROR;
    }

    mylite_execution_set_unsupported_error(
        database,
        "JSON_UNQUOTE() supports only string, NULL, and JSON_EXTRACT() arguments"
    );
    return MYLITE_ERROR;
}

static int json_quote_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (out_owned_text == NULL || out_text == NULL || out_text_length == NULL ||
        out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_QUOTE() supports only string and NULL arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return mylite_execution_set_unknown_column_reference_error(database, expression);
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            int rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "JSON_QUOTE() supports only string literals",
                "JSON_QUOTE() string literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );

            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            *out_is_null = true;
            return MYLITE_OK;
        }
        mylite_execution_set_json_quote_incorrect_type_error(database);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        mylite_execution_set_json_quote_incorrect_type_error(database);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_CAST_BINARY_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION) {
        mylite_execution_set_json_binary_charset_error(database);
        return MYLITE_ERROR;
    }

    mylite_execution_set_unsupported_error(
        database,
        "JSON_QUOTE() supports only string and NULL arguments"
    );
    return MYLITE_ERROR;
}

int mylite_execution_scalar_json_finish_extract_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
) {
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (result != NULL && result->status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_EXTRACT() path or document shape is not supported"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_invalid_json_function_text_error(
        database,
        result == NULL ? 0U : result->position
    );
    return MYLITE_ERROR;
}

static int finish_json_value_scalar_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result,
    bool allow_invalid_document_warning
) {
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (result != NULL && result->status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_VALUE() path or document shape is not supported"
        );
        return MYLITE_ERROR;
    }
    if (allow_invalid_document_warning && result != NULL &&
        result->status == MYLITE_JSON_NORMALIZE_INVALID) {
        return mylite_execution_append_invalid_json_value_warning(database, result);
    }
    mylite_execution_set_invalid_json_function_text_error(
        database,
        result == NULL ? 0U : result->position
    );
    return MYLITE_ERROR;
}

static int finish_json_length_scalar_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
) {
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (result != NULL && result->status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_LENGTH() path or document shape is not supported"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_invalid_json_function_text_error(
        database,
        result == NULL ? 0U : result->position
    );
    return MYLITE_ERROR;
}

static int finish_json_keys_scalar_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
) {
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (result != NULL && result->status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_KEYS() path or document shape is not supported"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_invalid_json_function_text_error(
        database,
        result == NULL ? 0U : result->position
    );
    return MYLITE_ERROR;
}

static int finish_json_type_scalar_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
) {
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (result != NULL && result->status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_TYPE() document shape is not supported"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_invalid_json_function_text_error(
        database,
        result == NULL ? 0U : result->position
    );
    return MYLITE_ERROR;
}

int mylite_execution_scalar_json_finish_path_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
) {
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (result != NULL && result->status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_EXTRACT() path expression is not supported"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_invalid_json_path_error(database, result == NULL ? 0U : result->position);
    return MYLITE_ERROR;
}

static int finish_json_contains_scalar_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
) {
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (result != NULL && result->status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_CONTAINS() path or document shape is not supported"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_invalid_json_function_text_error(
        database,
        result == NULL ? 0U : result->position
    );
    return MYLITE_ERROR;
}

static int finish_json_contains_path_scalar_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
) {
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (result != NULL && result->status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_CONTAINS_PATH() path or document shape is not supported"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_invalid_json_function_text_error(
        database,
        result == NULL ? 0U : result->position
    );
    return MYLITE_ERROR;
}

int mylite_execution_scalar_json_finish_length_path_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
) {
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (result != NULL && result->status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_LENGTH() path expression is not supported"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_invalid_json_path_error(database, result == NULL ? 0U : result->position);
    return MYLITE_ERROR;
}

int mylite_execution_scalar_json_finish_keys_path_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
) {
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (result != NULL && result->status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_KEYS() path expression is not supported"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_invalid_json_path_error(database, result == NULL ? 0U : result->position);
    return MYLITE_ERROR;
}

static int finish_json_unquote_scalar_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
) {
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (result != NULL && result->status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON_UNQUOTE() JSON string shape is not supported"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_invalid_json_function_text_error(
        database,
        result == NULL ? 0U : result->position
    );
    return MYLITE_ERROR;
}

static int finish_json_construction_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
) {
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (result != NULL && result->status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
        mylite_execution_set_unsupported_error(
            database,
            "JSON constructor JSON value shape is not supported"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_invalid_json_function_text_error(
        database,
        result == NULL ? 0U : result->position
    );
    return MYLITE_ERROR;
}
