#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_json_internal.h"

#include "mylite_ast.h"
#include "mylite_json.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct json_set_function_buffers {
    char **owned_paths;
    const char **paths;
    size_t *path_lengths;
    struct mylite_json_sql_value *values;
    char **owned_value_texts;
    size_t pair_count;
    bool force_null;
};

static int json_mutation_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_node_kind expected_kind,
    const char *function_name,
    enum planned_json_mutation_kind mutation_kind,
    struct session_scalar_cell *out_cell
);
static size_t json_mutation_path_count(
    enum planned_json_mutation_kind mutation_kind,
    size_t argument_count
);
static int evaluate_json_mutation_scalar_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *first_argument,
    const char *function_name,
    enum planned_json_mutation_kind mutation_kind,
    struct json_set_function_buffers *buffers
);
static int apply_json_mutation_scalar_function(
    enum planned_json_mutation_kind mutation_kind,
    struct json_set_function_buffers *buffers,
    const char *document,
    size_t document_length,
    char **out_result_text,
    size_t *out_result_length,
    struct mylite_json_normalize_result *out_result
);
static int json_set_document_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int allocate_json_set_function_buffers(
    struct mylite_db *database,
    size_t pair_count,
    struct json_set_function_buffers *out_buffers
);
static int evaluate_json_set_scalar_pairs(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *first_pair_argument,
    size_t pair_count,
    const char *function_name,
    struct json_set_function_buffers *buffers
);
static int evaluate_json_remove_scalar_paths(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *first_path_argument,
    size_t path_count,
    const char *function_name,
    struct json_set_function_buffers *buffers
);
static int json_set_path_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int evaluate_json_set_scalar_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_json_sql_value *out_value,
    char **out_owned_text
);
static int evaluate_json_set_json_function_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_json_sql_value *out_value,
    char **out_owned_text
);
static int json_set_json_extract_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static void free_json_set_function_buffers(struct json_set_function_buffers *buffers);
static int finish_json_set_scalar_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result,
    const char *function_name
);

int mylite_execution_scalar_json_set_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return json_mutation_function_value(
        database,
        expression,
        MYLITE_SQL_AST_JSON_SET_FUNCTION,
        "JSON_SET",
        PLANNED_JSON_MUTATION_SET,
        out_cell
    );
}

int mylite_execution_scalar_json_insert_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return json_mutation_function_value(
        database,
        expression,
        MYLITE_SQL_AST_JSON_INSERT_FUNCTION,
        "JSON_INSERT",
        PLANNED_JSON_MUTATION_INSERT,
        out_cell
    );
}

int mylite_execution_scalar_json_array_append_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return json_mutation_function_value(
        database,
        expression,
        MYLITE_SQL_AST_JSON_ARRAY_APPEND_FUNCTION,
        "JSON_ARRAY_APPEND",
        PLANNED_JSON_MUTATION_ARRAY_APPEND,
        out_cell
    );
}

int mylite_execution_scalar_json_array_insert_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return json_mutation_function_value(
        database,
        expression,
        MYLITE_SQL_AST_JSON_ARRAY_INSERT_FUNCTION,
        "JSON_ARRAY_INSERT",
        PLANNED_JSON_MUTATION_ARRAY_INSERT,
        out_cell
    );
}

int mylite_execution_scalar_json_replace_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return json_mutation_function_value(
        database,
        expression,
        MYLITE_SQL_AST_JSON_REPLACE_FUNCTION,
        "JSON_REPLACE",
        PLANNED_JSON_MUTATION_REPLACE,
        out_cell
    );
}

int mylite_execution_scalar_json_remove_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return json_mutation_function_value(
        database,
        expression,
        MYLITE_SQL_AST_JSON_REMOVE_FUNCTION,
        "JSON_REMOVE",
        PLANNED_JSON_MUTATION_REMOVE,
        out_cell
    );
}

static int json_mutation_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_node_kind expected_kind,
    const char *function_name,
    enum planned_json_mutation_kind mutation_kind,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *argument = NULL;
    struct json_set_function_buffers buffers = {0};
    char *owned_document = NULL;
    char *result_text = NULL;
    const char *document = NULL;
    size_t argument_count = 0U;
    size_t document_length = 0U;
    size_t pair_count = 0U;
    size_t result_length = 0U;
    bool document_is_null = false;
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (function_name == NULL) {
        return MYLITE_MISUSE;
    }
    if (expression == NULL || expression->kind != expected_kind) {
        mylite_execution_set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }

    rc = mylite_execution_scalar_json_collect_function_arguments(
        database,
        expression,
        function_name,
        &arguments,
        &argument_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_execution_scalar_json_require_mutation_argument_count(
        database,
        argument_count,
        function_name,
        mutation_kind
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    argument = mylite_execution_child_at(arguments, 0U);
    rc = json_set_document_scalar_argument(
        database,
        argument,
        function_name,
        &owned_document,
        &document,
        &document_length,
        &document_is_null
    );
    if (rc != MYLITE_OK || document_is_null) {
        goto done;
    }

    pair_count = json_mutation_path_count(mutation_kind, argument_count);
    rc = allocate_json_set_function_buffers(database, pair_count, &buffers);
    if (rc == MYLITE_OK) {
        rc = evaluate_json_mutation_scalar_arguments(
            database,
            argument == NULL ? NULL : argument->next_sibling,
            function_name,
            mutation_kind,
            &buffers
        );
    }
    if (rc != MYLITE_OK) {
        goto done;
    }
    if (buffers.force_null) {
        if (mutation_kind == PLANNED_JSON_MUTATION_ARRAY_INSERT) {
            rc = mylite_json_array_insert_validate_before_null(
                document,
                document_length,
                buffers.paths,
                buffers.path_lengths,
                buffers.pair_count,
                &result
            );
        } else {
            rc = mylite_json_mutation_validate_before_null(
                document,
                document_length,
                buffers.paths,
                buffers.path_lengths,
                buffers.pair_count,
                &result
            );
        }
        rc = finish_json_set_scalar_result(database, rc, &result, function_name);
        if (rc == MYLITE_OK) {
            out_cell->value = NULL;
        }
        goto done;
    }

    rc = apply_json_mutation_scalar_function(
        mutation_kind,
        &buffers,
        document,
        document_length,
        &result_text,
        &result_length,
        &result
    );
    rc = finish_json_set_scalar_result(database, rc, &result, function_name);
    if (rc == MYLITE_OK) {
        (void)result_length;
        out_cell->owned_text = result_text;
        out_cell->value = out_cell->owned_text;
        result_text = NULL;
    }

done:
    free(result_text);
    free_json_set_function_buffers(&buffers);
    free(owned_document);
    return rc;
}

int mylite_execution_scalar_json_require_mutation_argument_count(
    struct mylite_db *database,
    size_t argument_count,
    const char *function_name,
    enum planned_json_mutation_kind mutation_kind
) {
    bool valid = false;

    if (function_name == NULL) {
        return MYLITE_MISUSE;
    }
    if (mutation_kind == PLANNED_JSON_MUTATION_REMOVE) {
        valid = argument_count >= 2U;
    } else if (argument_count >= 3U) {
        valid = (argument_count % 2U) != 0U;
    }
    if (!valid) {
        mylite_execution_set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static size_t json_mutation_path_count(
    enum planned_json_mutation_kind mutation_kind,
    size_t argument_count
) {
    return mutation_kind == PLANNED_JSON_MUTATION_REMOVE ? argument_count - 1U
                                                         : (argument_count - 1U) / 2U;
}

static int evaluate_json_mutation_scalar_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *first_argument,
    const char *function_name,
    enum planned_json_mutation_kind mutation_kind,
    struct json_set_function_buffers *buffers
) {
    if (buffers == NULL) {
        return MYLITE_MISUSE;
    }
    if (mutation_kind == PLANNED_JSON_MUTATION_REMOVE) {
        return evaluate_json_remove_scalar_paths(
            database,
            first_argument,
            buffers->pair_count,
            function_name,
            buffers
        );
    }
    return evaluate_json_set_scalar_pairs(
        database,
        first_argument,
        buffers->pair_count,
        function_name,
        buffers
    );
}

static int apply_json_mutation_scalar_function(
    enum planned_json_mutation_kind mutation_kind,
    struct json_set_function_buffers *buffers,
    const char *document,
    size_t document_length,
    char **out_result_text,
    size_t *out_result_length,
    struct mylite_json_normalize_result *out_result
) {
    if (buffers == NULL) {
        return MYLITE_MISUSE;
    }
    if (mutation_kind == PLANNED_JSON_MUTATION_REPLACE) {
        return mylite_json_replace(
            document,
            document_length,
            buffers->paths,
            buffers->path_lengths,
            buffers->values,
            buffers->pair_count,
            out_result_text,
            out_result_length,
            out_result
        );
    }
    if (mutation_kind == PLANNED_JSON_MUTATION_INSERT) {
        return mylite_json_insert(
            document,
            document_length,
            buffers->paths,
            buffers->path_lengths,
            buffers->values,
            buffers->pair_count,
            out_result_text,
            out_result_length,
            out_result
        );
    }
    if (mutation_kind == PLANNED_JSON_MUTATION_ARRAY_APPEND) {
        return mylite_json_array_append(
            document,
            document_length,
            buffers->paths,
            buffers->path_lengths,
            buffers->values,
            buffers->pair_count,
            out_result_text,
            out_result_length,
            out_result
        );
    }
    if (mutation_kind == PLANNED_JSON_MUTATION_ARRAY_INSERT) {
        return mylite_json_array_insert(
            document,
            document_length,
            buffers->paths,
            buffers->path_lengths,
            buffers->values,
            buffers->pair_count,
            out_result_text,
            out_result_length,
            out_result
        );
    }
    if (mutation_kind == PLANNED_JSON_MUTATION_REMOVE) {
        return mylite_json_remove(
            document,
            document_length,
            buffers->paths,
            buffers->path_lengths,
            buffers->pair_count,
            out_result_text,
            out_result_length,
            out_result
        );
    }
    return mylite_json_set(
        document,
        document_length,
        buffers->paths,
        buffers->path_lengths,
        buffers->values,
        buffers->pair_count,
        out_result_text,
        out_result_length,
        out_result
    );
}

static int json_set_document_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
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
    if (function_name == NULL) {
        return MYLITE_MISUSE;
    }
    const char *document_message = "JSON_SET() supports only string and NULL documents";
    const char *literal_message = "JSON_SET() supports only string document literals";
    const char *nul_message = "JSON_SET() document literals do not support NUL bytes";

    if (strcmp(function_name, "JSON_REPLACE") == 0) {
        document_message = "JSON_REPLACE() supports only string and NULL documents";
        literal_message = "JSON_REPLACE() supports only string document literals";
        nul_message = "JSON_REPLACE() document literals do not support NUL bytes";
    } else if (strcmp(function_name, "JSON_INSERT") == 0) {
        document_message = "JSON_INSERT() supports only string and NULL documents";
        literal_message = "JSON_INSERT() supports only string document literals";
        nul_message = "JSON_INSERT() document literals do not support NUL bytes";
    } else if (strcmp(function_name, "JSON_ARRAY_APPEND") == 0) {
        document_message = "JSON_ARRAY_APPEND() supports only string and NULL documents";
        literal_message = "JSON_ARRAY_APPEND() supports only string document literals";
        nul_message = "JSON_ARRAY_APPEND() document literals do not support NUL bytes";
    } else if (strcmp(function_name, "JSON_ARRAY_INSERT") == 0) {
        document_message = "JSON_ARRAY_INSERT() supports only string and NULL documents";
        literal_message = "JSON_ARRAY_INSERT() supports only string document literals";
        nul_message = "JSON_ARRAY_INSERT() document literals do not support NUL bytes";
    } else if (strcmp(function_name, "JSON_REMOVE") == 0) {
        document_message = "JSON_REMOVE() supports only string and NULL documents";
        literal_message = "JSON_REMOVE() supports only string document literals";
        nul_message = "JSON_REMOVE() document literals do not support NUL bytes";
    }

    if (expression == NULL) {
        mylite_execution_set_unsupported_error(database, document_message);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            int rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                literal_message,
                nul_message,
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

    mylite_execution_set_unsupported_error(database, document_message);
    return MYLITE_ERROR;
}

static int allocate_json_set_function_buffers(
    struct mylite_db *database,
    size_t pair_count,
    struct json_set_function_buffers *out_buffers
) {
    if (out_buffers == NULL) {
        return MYLITE_MISUSE;
    }
    *out_buffers = (struct json_set_function_buffers){0};

    if (pair_count > SIZE_MAX / sizeof(*out_buffers->owned_paths) ||
        pair_count > SIZE_MAX / sizeof(*out_buffers->paths) ||
        pair_count > SIZE_MAX / sizeof(*out_buffers->path_lengths) ||
        pair_count > SIZE_MAX / sizeof(*out_buffers->values) ||
        pair_count > SIZE_MAX / sizeof(*out_buffers->owned_value_texts)) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_buffers->pair_count = pair_count;
    out_buffers->owned_paths = (char **)calloc(pair_count, sizeof(*out_buffers->owned_paths));
    out_buffers->paths = (const char **)calloc(pair_count, sizeof(*out_buffers->paths));
    out_buffers->path_lengths = (size_t *)calloc(pair_count, sizeof(*out_buffers->path_lengths));
    out_buffers->values =
        (struct mylite_json_sql_value *)calloc(pair_count, sizeof(*out_buffers->values));
    out_buffers->owned_value_texts =
        (char **)calloc(pair_count, sizeof(*out_buffers->owned_value_texts));
    if (out_buffers->owned_paths == NULL || out_buffers->paths == NULL ||
        out_buffers->path_lengths == NULL || out_buffers->values == NULL ||
        out_buffers->owned_value_texts == NULL) {
        free_json_set_function_buffers(out_buffers);
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int evaluate_json_set_scalar_pairs(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *first_pair_argument,
    size_t pair_count,
    const char *function_name,
    struct json_set_function_buffers *buffers
) {
    const struct mylite_sql_ast_node *argument = first_pair_argument;
    int rc = MYLITE_OK;

    if (buffers == NULL || function_name == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t pair_index = 0U; rc == MYLITE_OK && pair_index < pair_count; ++pair_index) {
        bool path_is_null = false;

        rc = json_set_path_scalar_argument(
            database,
            argument,
            function_name,
            &buffers->owned_paths[pair_index],
            &buffers->paths[pair_index],
            &buffers->path_lengths[pair_index],
            &path_is_null
        );
        if (rc == MYLITE_OK && path_is_null) {
            buffers->force_null = true;
            buffers->pair_count = pair_index;
            return MYLITE_OK;
        }
        argument = argument == NULL ? NULL : argument->next_sibling;
        if (rc == MYLITE_OK) {
            rc = evaluate_json_set_scalar_value_argument(
                database,
                argument,
                &buffers->values[pair_index],
                &buffers->owned_value_texts[pair_index]
            );
        }
        argument = argument == NULL ? NULL : argument->next_sibling;
    }
    if (rc == MYLITE_OK && argument != NULL) {
        mylite_execution_set_parse_error(database);
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int evaluate_json_remove_scalar_paths(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *first_path_argument,
    size_t path_count,
    const char *function_name,
    struct json_set_function_buffers *buffers
) {
    const struct mylite_sql_ast_node *argument = first_path_argument;
    int rc = MYLITE_OK;

    if (buffers == NULL || function_name == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t path_index = 0U; rc == MYLITE_OK && path_index < path_count; ++path_index) {
        bool path_is_null = false;

        rc = json_set_path_scalar_argument(
            database,
            argument,
            function_name,
            &buffers->owned_paths[path_index],
            &buffers->paths[path_index],
            &buffers->path_lengths[path_index],
            &path_is_null
        );
        if (rc == MYLITE_OK && path_is_null) {
            buffers->force_null = true;
            buffers->pair_count = path_index;
            return MYLITE_OK;
        }
        argument = argument == NULL ? NULL : argument->next_sibling;
    }
    if (rc == MYLITE_OK && argument != NULL) {
        mylite_execution_set_parse_error(database);
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int json_set_path_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (out_owned_text == NULL || out_text == NULL || out_text_length == NULL ||
        out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    if (function_name == NULL) {
        return MYLITE_MISUSE;
    }
    const char *path_message = "JSON_SET() supports only string and NULL path literals";
    const char *literal_message = "JSON_SET() supports only string path literals";
    const char *nul_message = "JSON_SET() path literals do not support NUL bytes";

    if (strcmp(function_name, "JSON_REPLACE") == 0) {
        path_message = "JSON_REPLACE() supports only string and NULL path literals";
        literal_message = "JSON_REPLACE() supports only string path literals";
        nul_message = "JSON_REPLACE() path literals do not support NUL bytes";
    } else if (strcmp(function_name, "JSON_INSERT") == 0) {
        path_message = "JSON_INSERT() supports only string and NULL path literals";
        literal_message = "JSON_INSERT() supports only string path literals";
        nul_message = "JSON_INSERT() path literals do not support NUL bytes";
    } else if (strcmp(function_name, "JSON_ARRAY_APPEND") == 0) {
        path_message = "JSON_ARRAY_APPEND() supports only string and NULL path literals";
        literal_message = "JSON_ARRAY_APPEND() supports only string path literals";
        nul_message = "JSON_ARRAY_APPEND() path literals do not support NUL bytes";
    } else if (strcmp(function_name, "JSON_ARRAY_INSERT") == 0) {
        path_message = "JSON_ARRAY_INSERT() supports only string and NULL path literals";
        literal_message = "JSON_ARRAY_INSERT() supports only string path literals";
        nul_message = "JSON_ARRAY_INSERT() path literals do not support NUL bytes";
    } else if (strcmp(function_name, "JSON_REMOVE") == 0) {
        path_message = "JSON_REMOVE() supports only string and NULL path literals";
        literal_message = "JSON_REMOVE() supports only string path literals";
        nul_message = "JSON_REMOVE() path literals do not support NUL bytes";
    }

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        mylite_execution_set_unsupported_error(database, path_message);
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        mylite_execution_set_unsupported_error(database, path_message);
        return MYLITE_ERROR;
    }

    rc = mylite_execution_decode_sql_string_literal(
        database,
        expression,
        literal_message,
        nul_message,
        out_owned_text,
        out_text_length
    );
    if (rc == MYLITE_OK) {
        *out_text = *out_owned_text;
    }
    return rc;
}

static int evaluate_json_set_scalar_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_json_sql_value *out_value,
    char **out_owned_text
) {
    if (out_value == NULL || out_owned_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct mylite_json_sql_value){0};
    *out_owned_text = NULL;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression != NULL && (expression->kind == MYLITE_SQL_AST_JSON_ARRAY_FUNCTION ||
                               expression->kind == MYLITE_SQL_AST_JSON_OBJECT_FUNCTION ||
                               expression->kind == MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION)) {
        return evaluate_json_set_json_function_value_argument(
            database,
            expression,
            out_value,
            out_owned_text
        );
    }

    return mylite_execution_scalar_json_evaluate_constructor_argument(
        database,
        expression,
        out_value,
        out_owned_text
    );
}

static int evaluate_json_set_json_function_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_json_sql_value *out_value,
    char **out_owned_text
) {
    struct session_scalar_cell cell = {0};
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return mylite_execution_scalar_json_evaluate_constructor_argument(
            database,
            expression,
            out_value,
            out_owned_text
        );
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_JSON_ARRAY_FUNCTION:
        rc = mylite_execution_scalar_json_array_function_value(database, expression, &cell);
        break;
    case MYLITE_SQL_AST_JSON_OBJECT_FUNCTION:
        rc = mylite_execution_scalar_json_object_function_value(database, expression, &cell);
        break;
    case MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION:
        rc = json_set_json_extract_function_value(database, expression, &cell);
        break;
    default:
        return mylite_execution_scalar_json_evaluate_constructor_argument(
            database,
            expression,
            out_value,
            out_owned_text
        );
    }

    if (rc != MYLITE_OK) {
        mylite_execution_session_scalar_cell_deinit(&cell);
        return rc;
    }
    if (cell.value == NULL) {
        out_value->kind = MYLITE_JSON_SQL_VALUE_NULL;
        mylite_execution_session_scalar_cell_deinit(&cell);
        return MYLITE_OK;
    }
    if (cell.has_value_size) {
        text_length = cell.value_size;
    } else {
        text_length = strlen(cell.value);
    }
    *out_owned_text = (char *)malloc(text_length + 1U);
    if (*out_owned_text == NULL) {
        mylite_execution_session_scalar_cell_deinit(&cell);
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(*out_owned_text, cell.value, text_length);
    (*out_owned_text)[text_length] = '\0';
    out_value->kind = MYLITE_JSON_SQL_VALUE_JSON;
    out_value->text = *out_owned_text;
    out_value->text_length = text_length;
    mylite_execution_session_scalar_cell_deinit(&cell);
    return MYLITE_OK;
}

static int json_set_json_extract_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
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

    rc = json_set_document_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        "JSON_SET",
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
    return rc;
}

static void free_json_set_function_buffers(struct json_set_function_buffers *buffers) {
    if (buffers == NULL) {
        return;
    }
    if (buffers->owned_paths != NULL) {
        for (size_t pair_index = 0U; pair_index < buffers->pair_count; ++pair_index) {
            free(buffers->owned_paths[pair_index]);
        }
    }
    if (buffers->owned_value_texts != NULL) {
        for (size_t pair_index = 0U; pair_index < buffers->pair_count; ++pair_index) {
            free(buffers->owned_value_texts[pair_index]);
        }
    }
    free((void *)buffers->owned_value_texts);
    free(buffers->values);
    free(buffers->path_lengths);
    free((void *)buffers->paths);
    free((void *)buffers->owned_paths);
    *buffers = (struct json_set_function_buffers){0};
}

static int finish_json_set_scalar_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result,
    const char *function_name
) {
    const char *unsupported_message = "JSON_SET() path or document shape is not supported";

    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return rc;
    }
    if (result != NULL && result->status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
        if (function_name != NULL && strcmp(function_name, "JSON_REPLACE") == 0) {
            unsupported_message = "JSON_REPLACE() path or document shape is not supported";
        } else if (function_name != NULL && strcmp(function_name, "JSON_INSERT") == 0) {
            unsupported_message = "JSON_INSERT() path or document shape is not supported";
        } else if (function_name != NULL && strcmp(function_name, "JSON_ARRAY_APPEND") == 0) {
            unsupported_message = "JSON_ARRAY_APPEND() path or document shape is not supported";
        } else if (function_name != NULL && strcmp(function_name, "JSON_ARRAY_INSERT") == 0) {
            unsupported_message = "JSON_ARRAY_INSERT() path or document shape is not supported";
        } else if (function_name != NULL && strcmp(function_name, "JSON_REMOVE") == 0) {
            unsupported_message = "JSON_REMOVE() path or document shape is not supported";
        }
        mylite_execution_set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (result != NULL && result->status == MYLITE_JSON_NORMALIZE_PATH_NOT_ALLOWED) {
        mylite_execution_set_json_path_not_allowed_error(database);
        return MYLITE_ERROR;
    }
    if (result != NULL && result->status == MYLITE_JSON_NORMALIZE_PATH_NOT_ARRAY_CELL) {
        mylite_execution_set_json_path_not_array_cell_error(database);
        return MYLITE_ERROR;
    }
    if (result != NULL && result->status == MYLITE_JSON_NORMALIZE_INVALID_PATH) {
        mylite_execution_set_invalid_json_path_error(database, result->position);
        return MYLITE_ERROR;
    }
    mylite_execution_set_invalid_json_function_text_error(
        database,
        result == NULL ? 0U : result->position
    );
    return MYLITE_ERROR;
}
