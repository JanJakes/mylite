#include "mylite_execution_scalar.h"

#include "mylite_ast.h"
#include "mylite_json.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct json_merge_function_buffers {
    char **owned_texts;
    struct session_scalar_cell *cells;
    const char **documents;
    size_t *document_lengths;
    size_t argument_count;
    size_t document_count;
    bool force_null;
};

static int json_merge_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_node_kind expected_kind,
    const char *function_name,
    enum planned_json_mutation_kind merge_kind,
    struct session_scalar_cell *out_cell
);
static int allocate_json_merge_function_buffers(
    struct mylite_db *database,
    size_t argument_count,
    struct json_merge_function_buffers *out_buffers
);
static int evaluate_json_merge_scalar_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    size_t argument_count,
    const char *function_name,
    bool patch_null_rule,
    struct json_merge_function_buffers *buffers
);
static int json_merge_document_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    char **out_owned_text,
    struct session_scalar_cell *inout_cell,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int evaluate_json_merge_document_function_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell
);
static bool json_merge_ast_kind_is_mutation_function(enum mylite_sql_ast_node_kind kind);
static bool json_merge_ast_kind_is_merge_function(enum mylite_sql_ast_node_kind kind);
static void free_json_merge_function_buffers(struct json_merge_function_buffers *buffers);
static int apply_json_merge_scalar_function(
    enum planned_json_mutation_kind merge_kind,
    struct json_merge_function_buffers *buffers,
    char **out_result_text,
    size_t *out_result_length,
    struct mylite_json_normalize_result *out_result
);
static int finish_json_merge_scalar_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result,
    const char *function_name
);
static const char *json_merge_document_unsupported_message(const char *function_name);

int mylite_execution_scalar_json_merge_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return json_merge_function_value(
        database,
        expression,
        MYLITE_SQL_AST_JSON_MERGE_FUNCTION,
        "JSON_MERGE",
        PLANNED_JSON_MUTATION_MERGE,
        out_cell
    );
}

int mylite_execution_scalar_json_merge_patch_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return json_merge_function_value(
        database,
        expression,
        MYLITE_SQL_AST_JSON_MERGE_PATCH_FUNCTION,
        "JSON_MERGE_PATCH",
        PLANNED_JSON_MUTATION_MERGE_PATCH,
        out_cell
    );
}

int mylite_execution_scalar_json_merge_preserve_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return json_merge_function_value(
        database,
        expression,
        MYLITE_SQL_AST_JSON_MERGE_PRESERVE_FUNCTION,
        "JSON_MERGE_PRESERVE",
        PLANNED_JSON_MUTATION_MERGE_PRESERVE,
        out_cell
    );
}

int mylite_execution_scalar_json_require_merge_argument_count(
    struct mylite_db *database,
    size_t argument_count,
    const char *function_name
) {
    if (argument_count < 2U) {
        mylite_execution_set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int json_merge_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_sql_ast_node_kind expected_kind,
    const char *function_name,
    enum planned_json_mutation_kind merge_kind,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    struct json_merge_function_buffers buffers = {0};
    char *result_text = NULL;
    size_t argument_count = 0U;
    size_t result_length = 0U;
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    if (out_cell == NULL || function_name == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
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
    if (rc == MYLITE_OK) {
        rc = mylite_execution_scalar_json_require_merge_argument_count(
            database,
            argument_count,
            function_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = allocate_json_merge_function_buffers(database, argument_count, &buffers);
    }
    if (rc == MYLITE_OK) {
        rc = evaluate_json_merge_scalar_arguments(
            database,
            arguments,
            argument_count,
            function_name,
            merge_kind == PLANNED_JSON_MUTATION_MERGE_PATCH,
            &buffers
        );
    }
    if (rc != MYLITE_OK) {
        goto done;
    }

    if (buffers.force_null) {
        rc = mylite_json_merge_patch_validate_documents(
            buffers.documents,
            buffers.document_lengths,
            buffers.document_count,
            &result
        );
        rc = finish_json_merge_scalar_result(database, rc, &result, function_name);
        if (rc == MYLITE_OK) {
            out_cell->value = NULL;
        }
        goto done;
    }

    rc = apply_json_merge_scalar_function(
        merge_kind,
        &buffers,
        &result_text,
        &result_length,
        &result
    );
    rc = finish_json_merge_scalar_result(database, rc, &result, function_name);
    if (rc == MYLITE_OK) {
        out_cell->owned_text = result_text;
        out_cell->value = out_cell->owned_text;
        out_cell->value_size = result_length;
        result_text = NULL;
    }

done:
    free(result_text);
    free_json_merge_function_buffers(&buffers);
    return rc;
}

static int allocate_json_merge_function_buffers(
    struct mylite_db *database,
    size_t argument_count,
    struct json_merge_function_buffers *out_buffers
) {
    if (out_buffers == NULL) {
        return MYLITE_MISUSE;
    }
    *out_buffers = (struct json_merge_function_buffers){0};

    if (argument_count > SIZE_MAX / sizeof(*out_buffers->owned_texts) ||
        argument_count > SIZE_MAX / sizeof(*out_buffers->cells) ||
        argument_count > SIZE_MAX / sizeof(*out_buffers->documents) ||
        argument_count > SIZE_MAX / sizeof(*out_buffers->document_lengths)) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_buffers->argument_count = argument_count;
    out_buffers->owned_texts = (char **)calloc(argument_count, sizeof(*out_buffers->owned_texts));
    out_buffers->cells =
        (struct session_scalar_cell *)calloc(argument_count, sizeof(*out_buffers->cells));
    out_buffers->documents = (const char **)calloc(argument_count, sizeof(*out_buffers->documents));
    out_buffers->document_lengths =
        (size_t *)calloc(argument_count, sizeof(*out_buffers->document_lengths));
    if (out_buffers->owned_texts == NULL || out_buffers->cells == NULL ||
        out_buffers->documents == NULL || out_buffers->document_lengths == NULL) {
        free_json_merge_function_buffers(out_buffers);
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int evaluate_json_merge_scalar_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    size_t argument_count,
    const char *function_name,
    bool patch_null_rule,
    struct json_merge_function_buffers *buffers
) {
    const struct mylite_sql_ast_node *argument =
        arguments == NULL ? NULL : mylite_execution_child_at(arguments, 0U);
    int rc = MYLITE_OK;

    if (buffers == NULL || function_name == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         ++argument_index) {
        const char *document = NULL;
        size_t document_length = 0U;
        bool is_null = false;

        rc = json_merge_document_scalar_argument(
            database,
            argument,
            function_name,
            &buffers->owned_texts[argument_index],
            &buffers->cells[argument_index],
            &document,
            &document_length,
            &is_null
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (is_null) {
            buffers->force_null = true;
            if (!patch_null_rule) {
                return MYLITE_OK;
            }
        } else {
            buffers->documents[buffers->document_count] = document;
            buffers->document_lengths[buffers->document_count] = document_length;
            ++buffers->document_count;
        }

        argument = argument->next_sibling;
    }
    if (rc == MYLITE_OK && argument != NULL) {
        mylite_execution_set_parse_error(database);
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int json_merge_document_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    char **out_owned_text,
    struct session_scalar_cell *inout_cell,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (function_name == NULL || out_owned_text == NULL || inout_cell == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *inout_cell = (struct session_scalar_cell){0};
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            json_merge_document_unsupported_message(function_name)
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "JSON merge functions support only string document literals",
                "JSON merge document literals do not support NUL bytes",
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

    rc = evaluate_json_merge_document_function_argument(database, expression, inout_cell);
    if (rc == MYLITE_OK) {
        if (inout_cell->value == NULL) {
            *out_is_null = true;
            return MYLITE_OK;
        }
        *out_text = inout_cell->value;
        *out_text_length =
            inout_cell->value_size != 0U ? inout_cell->value_size : strlen(inout_cell->value);
        return MYLITE_OK;
    }
    if (rc != MYLITE_MISUSE) {
        return rc;
    }

    mylite_execution_set_unsupported_error(
        database,
        json_merge_document_unsupported_message(function_name)
    );
    return MYLITE_ERROR;
}

static int evaluate_json_merge_document_function_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell
) {
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || inout_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_ARRAY_FUNCTION) {
        return mylite_execution_scalar_json_array_function_value(database, expression, inout_cell);
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_OBJECT_FUNCTION) {
        return mylite_execution_scalar_json_object_function_value(database, expression, inout_cell);
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION) {
        return mylite_execution_scalar_json_extract_function_value(
            database,
            expression,
            inout_cell
        );
    }
    if (json_merge_ast_kind_is_mutation_function(expression->kind)) {
        switch (expression->kind) {
        case MYLITE_SQL_AST_JSON_INSERT_FUNCTION:
            return mylite_execution_scalar_json_insert_function_value(
                database,
                expression,
                inout_cell
            );
        case MYLITE_SQL_AST_JSON_ARRAY_APPEND_FUNCTION:
            return mylite_execution_scalar_json_array_append_function_value(
                database,
                expression,
                inout_cell
            );
        case MYLITE_SQL_AST_JSON_ARRAY_INSERT_FUNCTION:
            return mylite_execution_scalar_json_array_insert_function_value(
                database,
                expression,
                inout_cell
            );
        case MYLITE_SQL_AST_JSON_REPLACE_FUNCTION:
            return mylite_execution_scalar_json_replace_function_value(
                database,
                expression,
                inout_cell
            );
        case MYLITE_SQL_AST_JSON_REMOVE_FUNCTION:
            return mylite_execution_scalar_json_remove_function_value(
                database,
                expression,
                inout_cell
            );
        default:
            return mylite_execution_scalar_json_set_function_value(
                database,
                expression,
                inout_cell
            );
        }
    }
    if (json_merge_ast_kind_is_merge_function(expression->kind)) {
        switch (expression->kind) {
        case MYLITE_SQL_AST_JSON_MERGE_FUNCTION:
            return mylite_execution_scalar_json_merge_function_value(
                database,
                expression,
                inout_cell
            );
        case MYLITE_SQL_AST_JSON_MERGE_PATCH_FUNCTION:
            return mylite_execution_scalar_json_merge_patch_function_value(
                database,
                expression,
                inout_cell
            );
        case MYLITE_SQL_AST_JSON_MERGE_PRESERVE_FUNCTION:
            return mylite_execution_scalar_json_merge_preserve_function_value(
                database,
                expression,
                inout_cell
            );
        default:
            break;
        }
    }
    return MYLITE_MISUSE;
}

static bool json_merge_ast_kind_is_mutation_function(enum mylite_sql_ast_node_kind kind) {
    return kind == MYLITE_SQL_AST_JSON_SET_FUNCTION ||
           kind == MYLITE_SQL_AST_JSON_INSERT_FUNCTION ||
           kind == MYLITE_SQL_AST_JSON_ARRAY_APPEND_FUNCTION ||
           kind == MYLITE_SQL_AST_JSON_ARRAY_INSERT_FUNCTION ||
           kind == MYLITE_SQL_AST_JSON_REPLACE_FUNCTION ||
           kind == MYLITE_SQL_AST_JSON_REMOVE_FUNCTION;
}

static bool json_merge_ast_kind_is_merge_function(enum mylite_sql_ast_node_kind kind) {
    return kind == MYLITE_SQL_AST_JSON_MERGE_FUNCTION ||
           kind == MYLITE_SQL_AST_JSON_MERGE_PATCH_FUNCTION ||
           kind == MYLITE_SQL_AST_JSON_MERGE_PRESERVE_FUNCTION;
}

static void free_json_merge_function_buffers(struct json_merge_function_buffers *buffers) {
    if (buffers == NULL) {
        return;
    }
    if (buffers->owned_texts != NULL) {
        for (size_t index = 0U; index < buffers->argument_count; ++index) {
            free(buffers->owned_texts[index]);
        }
    }
    if (buffers->cells != NULL) {
        for (size_t index = 0U; index < buffers->argument_count; ++index) {
            mylite_execution_session_scalar_cell_deinit(&buffers->cells[index]);
        }
    }
    free(buffers->document_lengths);
    free((void *)buffers->documents);
    free(buffers->cells);
    free((void *)buffers->owned_texts);
    *buffers = (struct json_merge_function_buffers){0};
}

static int apply_json_merge_scalar_function(
    enum planned_json_mutation_kind merge_kind,
    struct json_merge_function_buffers *buffers,
    char **out_result_text,
    size_t *out_result_length,
    struct mylite_json_normalize_result *out_result
) {
    if (buffers == NULL) {
        return MYLITE_MISUSE;
    }
    if (merge_kind == PLANNED_JSON_MUTATION_MERGE_PATCH) {
        return mylite_json_merge_patch(
            buffers->documents,
            buffers->document_lengths,
            buffers->document_count,
            out_result_text,
            out_result_length,
            out_result
        );
    }
    return mylite_json_merge_preserve(
        buffers->documents,
        buffers->document_lengths,
        buffers->document_count,
        out_result_text,
        out_result_length,
        out_result
    );
}

static int finish_json_merge_scalar_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result,
    const char *function_name
) {
    (void)function_name;
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
            "JSON merge document shape is not supported"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_invalid_json_function_text_error(
        database,
        result == NULL ? 0U : result->position
    );
    return MYLITE_ERROR;
}

static const char *json_merge_document_unsupported_message(const char *function_name) {
    if (strcmp(function_name, "JSON_MERGE_PATCH") == 0) {
        return "JSON_MERGE_PATCH() supports only string, NULL, and JSON function documents";
    }
    if (strcmp(function_name, "JSON_MERGE_PRESERVE") == 0) {
        return "JSON_MERGE_PRESERVE() supports only string, NULL, and JSON function documents";
    }
    return "JSON_MERGE() supports only string, NULL, and JSON function documents";
}
