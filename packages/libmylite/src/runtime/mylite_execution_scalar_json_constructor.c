#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_json_internal.h"

#include "mylite_ast.h"
#include "mylite_json.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

struct json_object_function_buffers {
    struct mylite_json_sql_value *keys;
    struct mylite_json_sql_value *values;
    char **owned_texts;
    size_t owned_text_count;
};

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
static int finish_json_construction_result(
    struct mylite_db *database,
    int rc,
    const struct mylite_json_normalize_result *result
);

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
        out_cell->owned_text = result_text;
        out_cell->value = out_cell->owned_text;
        out_cell->value_size = result_length;
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
        out_cell->owned_text = result_text;
        out_cell->value = out_cell->owned_text;
        out_cell->value_size = result_length;
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
